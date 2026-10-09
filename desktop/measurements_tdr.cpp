#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"
#include "qcpgraphdatahelpers.h"

extern QMap<QString, QString> g_mapTabPlotNames;
extern int g_maxMeasurements; // defined in measurements.cpp
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original measurements.cpp (still in
// measurements.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of Measurements.

int Measurements::calcTdrDist(QVector<RawData> *data)
{
    if (data == nullptr || data->length() == 0)
        return 0;

    int asize = data->length();

    double minfq = data->at(0).fq;
    if ( minfq > 0.1 )
    {
        return 0; // Wrong fq
    }

    double maxfq = data->at(asize-1).fq;

    // See TdrMath::estimateRaw() -- this used to duplicate CalcTdr()'s FFT-size/
    // resolution/range math inline; both now share one implementation. Its
    // asize<2||maxfq<=minfq NaN guard is a small behavior addition here (this
    // function never had it before, unlike CalcTdr()), closing the same
    // latent single-point-measurement crash CalcTdr() was already guarded
    // against.
    return (int)TdrMath::estimateRaw(asize, minfq, maxfq, m_cableVelFactor, m_measureSystemMetric).unambiguousRange;
}

int Measurements::CalcTdr(QVector <RawData> *data)
{
    if (data == nullptr || data->length() == 0)
        return 0;

    int asize = data->length();

    // m_tdrDots is the dot count TDR mode's own scan was
    // started with (startTDRProgress()) -- this guard's real job is
    // refusing to FFT a TDR-tool scan that was canceled before collecting
    // as many points as it was asked for (stopTDRProgress() always
    // redraws once on the way out, canceled or not). stopTDRProgress()
    // resets m_tdrDots back to 0 right after, specifically so this can't
    // outlive that one scan: found 2026-08-26 that without the reset, any
    // later CalcTdr() call -- a normal scan, or even just switching to the
    // TDR tab -- silently returned 0 forever after, since a normal scan's
    // point count (~100) is routinely below the TDR tool's own minimum
    // (TDR_MINPOINTS = 200), regardless of that later data's own frequency
    // range being perfectly valid.
    if (asize < (int)m_tdrDots)
    {
        return 0;
    }

    TdrMath::Result tdr = TdrMath::compute(*data, m_cableVelFactor, m_measureSystemMetric,
                                           m_tdrWindowType, m_tdrKaiserBeta, m_Z0);
    if (tdr.fftSize == 0)
        return 0;
    m_tdrRange = tdr.range;
    m_tdrImp = tdr.impulse;
    m_tdrStep = tdr.step;
    m_tdrZ = tdr.impedance;
    return tdr.fftSize;
}

void Measurements::startTDRProgress(QWidget* _parent, int _dots)
{
    m_tdrDots = _dots;
    delete m_tdrProgressDlg;

    m_tdrProgressDlg = new ProgressDlg(_parent);
    m_tdrProgressDlg->setWindowModality(Qt::WindowModal);
    m_tdrProgressDlg->setValue(0);
    m_tdrProgressDlg->setProgressData(0, _dots, 1);
    m_tdrProgressDlg->updateActionInfo(tr("TDR measuring"));
    m_tdrProgressDlg->updateStatusInfo(tr("please wait ...."));
    m_tdrProgressDlg->setCancelable();
    connect(m_tdrProgressDlg, &ProgressDlg::canceled, this, &Measurements::measurementCanceled);
    m_tdrProgressDlg->show();
}

void Measurements::stopTDRProgress()
{
    if (m_tdrProgressDlg != nullptr)
    {
        m_tdrProgressDlg->hide();
        delete m_tdrProgressDlg;
        m_tdrProgressDlg = nullptr;
    }
    on_redrawGraphs();
    // on_redrawGraphs() only redraws whichever tab is currently visible
    // (see its own tab_swr/tab_phase/.../tab_tdr dispatch) -- fine for a
    // normal frequency-sweep scan, since the user is necessarily looking at
    // *some* chart tab while one runs. TdrScanPanel (TDR mode)
    // breaks that assumption: it's deliberately tab-
    // independent (see the tdr-scan-rework-plan memory -- "replaces the old
    // tab-implicit trigger"), so a scan run from there with any *other* tab
    // selected left tdrImpGraph/tdrStepGraph/tdrZGraph never (re)populated
    // for the just-finished measurement. the Result readout then read an empty
    // container and TdrScanPanel::refreshResult() (connected to the same
    // measurementComplete() signal, right after this call returns) always
    // showed "-- (run a TDR scan first)" no matter what was actually
    // scanned. Confirmed 2026-08-25.
    if (m_currentTab != "tab_tdr")
        redrawTDR();

    // Reset now that both redraw calls above (which need this scan's own
    // dots target for CalcTdr()'s canceled-early guard, see its comment)
    // are done with it. m_tdrDots has no other reader once this scan is
    // over (only CalcTdr()'s guard and updateTDRProgress()'s status text,
    // both scoped to this one scan) -- left set, it silently blocked every
    // later CalcTdr() call (a normal scan, or just switching to the TDR
    // tab) for the rest of the session, since a normal scan's point count
    // is routinely below whatever this TDR-tool scan asked for. Found
    // 2026-08-26.
    m_tdrDots = 0;
}

void Measurements::updateTDRProgress(int dots)
{
    if (m_tdrProgressDlg != nullptr) {
        //if ((dots%10) == 0)
        {
            m_tdrProgressDlg->setValue(dots);
            m_tdrProgressDlg->updateStatusInfo(QString(tr("processed %1 dots, from %2")).arg(dots).arg(m_tdrDots));
        }
    }
}

int Measurements::selectedRow() const
{
    if (m_tableWidget == nullptr)
        return -1;
    const QList<QTableWidgetItem*> items = m_tableWidget->selectedItems();
    int row = items.isEmpty() ? -1 : items.first()->row();
    return (row >= 0 && row < m_measurements.length()) ? row : -1;
}

// The selected measurement if it can be shown as TDR, otherwise the newest
// one that can.
int Measurements::tdrDisplayRow() const
{
    int selected = selectedRow();
    if (tdrCapable(selected))
        return selected;
    for (int i = m_measurements.length() - 1; i >= 0; --i) {
        if (tdrCapable(i))
            return i;
    }
    return -1;
}

int Measurements::tdrUserMarkerCount() const
{
    int row = tdrDisplayRow();
    return row < 0 ? 0 : m_tdrUserMeters.value(m_measurements.at(row).serialNumber).size();
}

Measurements::TdrEventSet Measurements::tdrEvents()
{
    TdrEventSet set;
    set.metric = m_measureSystemMetric;
    int i = tdrDisplayRow();
    if (i < 0) {
        int selected = selectedRow();
        if (m_measurements.isEmpty()) {
            set.note = tr("Run a TDR scan, or open a sweep that starts at %1 MHz or lower.").arg(TDR_MAX_START_MHZ);
        } else if (selected >= 0 && !m_measurements.at(selected).dataRX.isEmpty()
                   && m_measurements.at(selected).dataRX.first().fq > TDR_MAX_START_MHZ) {
            set.note = tr("%1 can't be shown as TDR: it starts at %2 MHz (TDR needs %3 MHz or lower).")
                           .arg(m_measurements.at(selected).name)
                           .arg(m_measurements.at(selected).dataRX.first().fq)
                           .arg(TDR_MAX_START_MHZ);
        } else {
            set.note = tr("TDR needs a sweep that starts at %1 MHz or lower, with at least %2 points.")
                           .arg(TDR_MAX_START_MHZ).arg(TDR_MIN_DATA_POINTS);
        }
    }
    if (i >= 0) {
        int selected = selectedRow();
        if (selected >= 0 && selected != i)
            set.note = tr("the selected measurement can't be shown as TDR");
        int serial = m_measurements.at(i).serialNumber;
        const QVector<double> userMeters = m_tdrUserMeters.value(serial);
        int mode = rowCable(i);
        const measurement& mm = (mode == 1) ? m_farEndMeasurementsSub[i]
                              : (mode == 2) ? m_farEndMeasurementsAdd[i] : m_measurements.at(i);
        bool calib = (mode == 0) && rowOsl(i);
        TdrMath::Result r = TdrMath::compute(calib ? mm.dataRXCalib : mm.dataRX, m_cableVelFactor,
                                             m_measureSystemMetric, m_tdrWindowType, m_tdrKaiserBeta, m_Z0);
        if (r.fftSize == 0)
            return set;
        set.valid = true;
        set.events = TdrMath::findEvents(r);
        set.userIndex = QVector<int>(set.events.size(), -1);
        double step = r.range / r.fftSize;
        for (int u = 0; u < userMeters.size(); ++u) {
            TdrMath::Event e;
            e.distance = m_measureSystemMetric ? userMeters.at(u) : userMeters.at(u) * FEETINMETER;
            int idx = qBound(0, qRound(e.distance / step), r.fftSize - 1);
            e.amplitude = r.impulse.at(idx);
            e.impedance = r.impedance.at(idx);
            e.nearRangeEdge = e.distance > r.range;
            e.kind = TdrMath::EventKind::User;
            int at = 0;
            while (at < set.events.size() && set.events.at(at).distance <= e.distance)
                ++at;
            // Keep echoOf pointing at the same events after the insert.
            for (TdrMath::Event& other : set.events) {
                if (other.echoOf >= at)
                    ++other.echoOf;
            }
            set.events.insert(at, e);
            set.userIndex.insert(at, u);
        }
        set.velFactor = m_cableVelFactor;
        set.metric = m_measureSystemMetric;
        set.name = m_measurements.at(i).name;
        return set;
    }
    return set;
}

static const QColor kTdrEventColor(215, 85, 40);
static const QColor kTdrUserColor(0, 130, 150);

// User markers belong to the measurement the events table is showing.
void Measurements::addTdrUserMarker(double chartDistance)
{
    int row = tdrDisplayRow();
    if (row < 0)
        return;
    m_tdrUserMeters[m_measurements.at(row).serialNumber]
        << (m_measureSystemMetric ? chartDistance : chartDistance / FEETINMETER);
    emit tdrEventsChanged();
}

void Measurements::removeTdrUserMarker(int index)
{
    int row = tdrDisplayRow();
    if (row < 0)
        return;
    QVector<double>& list = m_tdrUserMeters[m_measurements.at(row).serialNumber];
    if (index < 0 || index >= list.size())
        return;
    list.remove(index);
    emit tdrEventsChanged();
}

void Measurements::clearTdrUserMarkers()
{
    int row = tdrDisplayRow();
    if (row < 0 || m_tdrUserMeters.value(m_measurements.at(row).serialNumber).isEmpty())
        return;
    m_tdrUserMeters.remove(m_measurements.at(row).serialNumber);
    emit tdrEventsChanged();
}

void Measurements::setTdrEventLines(const QVector<double>& distances, const QVector<bool>& user)
{
    for (QCPItemStraightLine* line : m_tdrEventLines)
        m_tdrWidget->removeItem(line);
    for (QCPItemText* label : m_tdrEventLabels)
        m_tdrWidget->removeItem(label);
    m_tdrEventLines.clear();
    m_tdrEventLabels.clear();
    m_tdrEventIsUser = user;

    for (int i = 0; i < distances.size(); ++i) {
        QCPItemStraightLine* line = new QCPItemStraightLine(m_tdrWidget);
        line->setAntialiased(false);
        line->point1->setCoords(distances.at(i), 0);
        line->point2->setCoords(distances.at(i), 1);
        m_tdrEventLines << line;

        // Pinned to the top of the plot whatever the y zoom.
        QCPItemText* label = new QCPItemText(m_tdrWidget);
        label->position->setTypeX(QCPItemPosition::ptPlotCoords);
        label->position->setTypeY(QCPItemPosition::ptAxisRectRatio);
        label->position->setAxisRect(m_tdrWidget->xAxis->axisRect());
        label->position->setCoords(distances.at(i), 0.02);
        label->setPositionAlignment(Qt::AlignHCenter | Qt::AlignTop);
        label->setText(QString("#%1").arg(i + 1));
        label->setColor(user.value(i) ? kTdrUserColor : kTdrEventColor);
        label->setBrush(QBrush(QColor(255, 255, 255, 140)));
        label->setPadding(QMargins(3, 1, 3, 1));
        m_tdrEventLabels << label;
    }
    setTdrEventHighlight(-1);
}

void Measurements::setTdrEventHighlight(int index)
{
    for (int i = 0; i < m_tdrEventLines.size(); ++i) {
        bool on = (i == index);
        bool user = m_tdrEventIsUser.value(i);
        QPen pen(user ? kTdrUserColor : kTdrEventColor, on ? 2.5 : 1.0,
                 (on || user) ? Qt::SolidLine : Qt::DashLine);
        m_tdrEventLines.at(i)->setPen(pen);
        QFont font = m_tdrEventLabels.at(i)->font();
        font.setBold(on);
        m_tdrEventLabels.at(i)->setFont(font);
    }
    m_tdrWidget->replot();
}

void Measurements::redrawTDR(int _index, bool resetRange)
{
    m_tdrZRange = 0;
    int begin = _index < 0 ? 0 : _index;
    int end = _index < 0 ? m_measurements.length() : (_index+1);
    for (int index=begin; index<end; index++) {
        int mode = rowCable(index);
        measurement& mm = (mode == 1)
                ? m_farEndMeasurementsSub[index]
                : ( (mode == 2) ? m_farEndMeasurementsAdd[index] : m_measurements[index] );

        // Cable-corrected points already include calibration (see calcFarEnd()).
        bool calib = (mode == 0) && rowOsl(index);
        int len = CalcTdr(calib ? &mm.dataRXCalib : &mm.dataRX);
        if (len <= 0)
        {
            // CalcTdr() returns 0 for several "not enough/valid data yet"
            // cases (too few points, wrong fq, FFT size out of range). The
            // code below unconditionally divides m_tdrRange by len, which
            // would itself be a division by zero (0/0 == NaN when m_tdrRange
            // is still its initial 0) -- skip this measurement's TDR redraw
            // instead of feeding that into the axis/graphs.
            continue;
        }
        if (resetRange)
        {
            // setRangeMax(m_tdrRange) used to follow here too -- removed in
            // the 2.x port (2026-08-25), see Print::setRange()'s comment
            // (print.cpp): not real QCustomPlot API, and set to exactly the
            // value setRangeUpper() just applied on the line above anyway.
            m_tdrWidget->xAxis->setRangeUpper(m_tdrRange);
        }
        double step = m_tdrRange/len;
        mm.tdrImpGraph.clear();
        mm.tdrStepGraph.clear();
        mm.tdrZGraph.clear();
        mm.tdrImpGraphFeet.clear();
        mm.tdrStepGraphFeet.clear();
        mm.tdrZGraphFeet.clear();
        for(int i = 0; i < len; ++i)
        {
            double x = i;
            QCPGraphData data;
            data.key = x*step;
            data.value = m_tdrImp[i];
            mm.tdrImpGraph.add(data);
            data.value = m_tdrStep[i];
            mm.tdrStepGraph.add(data);
            data.value = m_tdrZ[i];
            mm.tdrZGraph.add(data);

            QCPGraphData dataFeet;
            dataFeet.key = x*step;
            dataFeet.value = m_tdrImp[i];
            mm.tdrImpGraphFeet.add(dataFeet);
            dataFeet.value = m_tdrStep[i];
            mm.tdrStepGraphFeet.add(dataFeet);
            dataFeet.value = m_tdrZ[i];
            mm.tdrZGraphFeet.add(dataFeet);

            m_tdrZRange = m_measureSystemMetric ? qMax(m_tdrZRange, data.value) : qMax(m_tdrZRange, dataFeet.value);
        }
        m_tdrWidget->graph(index*3+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measureSystemMetric ? mm.tdrImpGraph : mm.tdrImpGraphFeet));
        m_tdrWidget->graph(index*3+2)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measureSystemMetric ? mm.tdrStepGraph : mm.tdrStepGraphFeet));
        m_tdrWidget->graph(index*3+3)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measureSystemMetric ? mm.tdrZGraph : mm.tdrZGraphFeet));
    } // for ( index )
    // m_tdrZRange is reset to 0 at the top of this function and only raised
    // inside the per-measurement loop above, which is skipped entirely
    // (continue) whenever CalcTdr() had no valid TDR data for that
    // measurement -- e.g. every call during a normal frequency-band scan,
    // since CalcTdr() rejects any data that doesn't start near DC. Setting
    // yAxis2 to [0, 0*1.05] == [0, 0] unconditionally collapses it to a
    // zero-size range; QCPAxis::coordToPixel() then divides by
    // mRange.size() (== 0) whenever it maps a value of 0 (the axis's own
    // lower bound, hit on essentially every replot), giving 0/0 == NaN and
    // crashing the next qRound() on that pixel coordinate. Only touch the
    // axis when we actually have a new, real range to show.
    if (m_tdrZRange > 0)
    {
        m_tdrWidget->yAxis2->setRangeUpper(m_tdrZRange*1.05);
        m_tdrWidget->yAxis2->setRangeLower(0);
    }
    extern MainWindow* g_mainWindow;
    g_mainWindow->m_tdrZRange = m_tdrZRange;

    replot();
    emit tdrRedrawn();
}

