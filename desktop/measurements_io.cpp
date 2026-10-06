#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"
#include "measurementfiles.h"
#include "Notification.h"

extern QMap<QString, QString> g_mapTabPlotNames;
extern int g_maxMeasurements; // defined in measurements.cpp

extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original measurements.cpp (still in
// measurements.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of Measurements.

void Measurements::saveData(quint32 number, QString path)
{
    if (number >= (quint32)g_maxMeasurements)
        number = g_maxMeasurements-1;

    // As shown, with a record of its corrections and the original points.
    const measurement& mm = m_measurements.at(number);
    MeasurementFiles::writeAsd(path, mm.shownPoints(), mm.shownCorrections(), mm.originalPoints(), mm.kind);
}

Corrections Measurements::currentCorrections() const
{
    Corrections c;
    c.osl = (m_calibration != nullptr) && m_calibration->getCalibrationEnabled();
    c.cableMode = (m_farEndMeasurement == 1 || m_farEndMeasurement == 2) ? m_farEndMeasurement : 0;
    c.cable = cableParams();
    c.cableLossFqMHz = m_cableLossFqMHz;
    return c;
}

void Measurements::loadData(QString path)
{
    if(path.indexOf(".asd") >= 0 )
    {
        MeasurementFiles::ReadResult r = MeasurementFiles::readAsd(path);
        if (r.error == MeasurementFiles::ReadError::CannotOpen) {
            g_showMessageBox(NULL, QMessageBox::Information, tr("Error"), tr("Couldn't open saved file."));
            return;
        }
        if (r.error == MeasurementFiles::ReadError::TooShort) {
            g_showMessageBox(NULL, QMessageBox::Information, tr("Error"), tr("The saved file is too short."));
            return;
        }

        int size = r.raw.size();
        on_newMeasurement(MeasurementFiles::nameFromPath(path),
                          static_cast<qint64>(r.fqMinMHz * 1000000),
                          static_cast<qint64>(r.fqMaxMHz * 1000000), size);
        m_measurements.last().dirty = false; // loaded from a file, see measurement::dirty's own comment
        setLastMeasurementKind(r.kind);
        noteLoadedCorrections(r.applied);
        m_measurements.last().asReceived = r.asReceived;

        ProgressDlg* progressDlg = new ProgressDlg();
        progressDlg->setValue(0);
        progressDlg->setProgressData(0, size, 1);
        progressDlg->updateActionInfo(tr("Load measurement"));
        progressDlg->updateStatusInfo(tr("please wait ...."));
        progressDlg->show();
        progressDlg->setWindowModality(Qt::WindowModal);
        QApplication::processEvents();

        for(int i = 0; i < size; ++i)
        {
            on_newData(r.raw.at(i));
            if ((i%10) == 0) {
                progressDlg->setValue(i);
                progressDlg->updateStatusInfo(QString(tr("loaded %1 dots, from %2")).arg(i).arg(size));
            }
        }
        progressDlg->hide();
        delete progressDlg;

        emit import_finished(r.fqMinMHz*1000, r.fqMaxMHz*1000);
        // Points column stays "--" (the on_newMeasurement() table rebuild's
        // own placeholder for a not-yet-populated row) until something
        // stamps the real count in -- on_measurementComplete() already
        // does exactly that for a finished live scan; every import path
        // finishes populating dataRX with no equivalent call, so it was
        // silently left at "--" until some unrelated later table rebuild
        // happened to notice the real count.
        on_measurementComplete();
    }else
    {
        importData(path);
    }
    on_redrawGraphs();
}

void Measurements::exportData(QString _name, int _type, int _number)
{
    if (_number < 0 || m_measurements.isEmpty() || (_number >= m_measurements.size()))
        return;

    // The measurement as shown, with its corrections recorded (.s1p).
    const measurement& mm = m_measurements.at(_number);
    Corrections rec = mm.shownCorrections();
    double z0 = (rec.osl && m_calibration != nullptr) ? m_calibration->getZ0() : 50;
    MeasurementFiles::writeRaw(_name, _type, mm.shownPoints(), z0, QString(), rec);
}

void Measurements::importData(QString _name, bool /*user_format*/)
{
    on_newMeasurement(MeasurementFiles::nameFromPath(_name));
    m_measurements.last().dirty = false; // loaded from a file, see measurement::dirty's own comment
    setLastMeasurementKind(MeasurementKind::Sweep);

    QFile file(_name);
    bool result = file.open(QFile::ReadOnly);
    if(result)
    {
        QString str = file.readAll();
        double fqMin = DBL_MAX;
        double fqMax = 0;
        QStringList nList = str.split('\n');

        str = nList.takeFirst();
        if (str.at(0) == '#') {
            str.replace('#', ' ');
            on_newUserDataHeader(str.trimmed().split(','));
        }

        while (!nList.isEmpty()) {
            str = nList.takeFirst();
            if (str.isEmpty())
                continue;
            QStringList fields = str.split(',');
            RawData rdata;
            UserData udata;
            bool ok;
            QString field = fields.takeFirst();
            rdata.fq = field.toDouble(&ok);
            if (!ok) {
                qDebug() << "***** ERROR: " << str;
                return;
            }
            udata.fq = rdata.fq;
            fqMin = qMin(fqMin, rdata.fq);
            fqMax = qMax(fqMax, rdata.fq);

            field = fields.takeFirst();
            rdata.r = field.toDouble(&ok);
            if (!ok) {
                qDebug() << "***** ERROR: " << str;
                return;
            }
            field = fields.takeFirst();
            rdata.x = field.toDouble(&ok);
            if (!ok) {
                qDebug() << "***** ERROR: " << str;
                return;
            }
            while (!fields.isEmpty()) {
                udata.values.append(fields.takeFirst().toDouble(&ok));
                if (!ok) {
                    qDebug() << "***** ERROR: " << str;
                    return;
                }
            }
            on_newUserData(rdata, udata);
        }
        emit import_finished(fqMin*1000, fqMax*1000);
        on_measurementComplete(); // stamp the real Points count -- see loadData()'s own comment on this
        on_redrawGraphs();
    }
}

// See MeasurementFiles::writeTouchstone2Port().
void Measurements::exportSParamData(QString _name, int _type, int _number, QString _description)
{
    if (_number < 0 || m_measurements.isEmpty() || (_number >= m_measurements.size()))
        return;

    const QList<SParamPoint>& points = m_measurements.at(_number).dataSParam;
    if (points.isEmpty())
        return;

    MeasurementFiles::writeTouchstone2Port(_name, _type, points, _description);
}

// Fills in a just-imported 2-port measurement's dataSParam plus the
// derived S21/S12 magnitude(dB)/phase(degrees) graphs the S21 tab
// actually plots. Batch, not per-point -- on_newSParamPoint() (the live-
// capture path) redraws after every single point, which is fine for one
// point at a time but far too slow across a whole imported file.
void Measurements::populateSParamData(const QList<SParamPoint>& points)
{
    if (m_measurements.isEmpty())
        return;

    measurement& mm = m_measurements.last();

    bool haveS21Prev = false, haveS12Prev = false;
    double s21PrevRaw = 0, s21PrevUnwrapped = 0;
    double s12PrevRaw = 0, s12PrevUnwrapped = 0;

    foreach (const SParamPoint& sp, points) {
        mm.dataSParam.append(sp);

        // sp.fq is MHz, matching RawData.fq's own convention -- *1000 to
        // the kHz every chart key actually uses (see e.g. this file's
        // RawData path, or "double fq = _rawData.fq*1000;" in
        // measurements.cpp) -- without it, the whole trace renders
        // compressed 1000x toward the origin.
        double fqKey = sp.fq*1000;

        QCPGraphData mag, phase;
        mag.key = phase.key = fqKey;

        mag.value = 20*log10(std::abs(sp.s21));
        phase.value = RfMath::unwrapPhaseDeg(std::arg(sp.s21)*180.0/M_PI, haveS21Prev, s21PrevRaw, s21PrevUnwrapped);
        mm.s21MagGraph.add(mag);
        mm.s21PhaseGraph.add(phase);

        mag.value = 20*log10(std::abs(sp.s12));
        phase.value = RfMath::unwrapPhaseDeg(std::arg(sp.s12)*180.0/M_PI, haveS12Prev, s12PrevRaw, s12PrevUnwrapped);
        mm.s12MagGraph.add(mag);
        mm.s12PhaseGraph.add(phase);
    }
}

void Measurements::importData(QString _name)
{
    MeasurementFiles::ReadResult r;
    if((_name.indexOf(".s1p") >= 0) || (_name.indexOf(".s2p") >= 0))
    {
        r = MeasurementFiles::readTouchstone(_name);
    }
    else if(_name.indexOf(".csv") >= 0 )
    {
        // Never confirmed working (User Defined tab, gated by
        // USER_DEFINED_FEATURE -- see CMakeLists.txt); left exactly as it
        // was rather than fixed, per 2026-08-20 decision to just gate it
        // off until real EFRX-capable hardware turns up. Also required
        // g_developerMode until that flag was removed 2026-09-07.
#if USER_DEFINED_FEATURE
        importData(_name, true);
        return;
#endif
        r = MeasurementFiles::readCsv(_name);
    }
    else if(_name.indexOf(".nwl") >= 0 )
    {
        r = MeasurementFiles::readNwl(_name);
    } else {
        g_showMessageBox(nullptr, QMessageBox::Information, tr("Load data"), tr("Oops, this format is not supported!"), QMessageBox::Close);
        return;
    }
    if (!r.ok())
        return; // these formats never reported read failures

    on_newMeasurement(MeasurementFiles::nameFromPath(_name),
                      static_cast<qint64>(r.fqMinMHz*1000000), static_cast<qint64>(r.fqMaxMHz*1000000),
                      r.raw.length());
    m_measurements.last().dirty = false; // loaded from a file, see measurement::dirty's own comment
    noteLoadedCorrections(r.applied);
    foreach (auto data, r.raw) {
        on_newData(data);
    }
    if (!r.sparams.isEmpty())
    {
        populateSParamData(r.sparams);
    }
    emit import_finished(r.fqMinMHz*1000, r.fqMaxMHz*1000);
    on_measurementComplete(); // stamp the real Points count -- see loadData()'s own comment on this
}

void Measurements::noteLoadedCorrections(const Corrections& applied)
{
    // Shown as saved; the Corr. column says what's in it.
    m_measurements.last().applied = applied;
    m_measurements.last().corrections = Corrections();
    m_measurements.last().analyzerSerial.clear();
    refreshCorrectionsCell(m_measurements.length() - 1);
}
