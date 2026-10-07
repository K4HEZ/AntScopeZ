#include "markers.h"
#include "mainwindow.h"
#include "style.h"
#include "qcpgraphdatahelpers.h"
#include "markermath.h"
#include "appconfig.h"


Markers::Markers(QObject *parent) : QObject(parent),
    m_swrWidget(NULL),
    m_phaseWidget(NULL),
    m_rsWidget(NULL),
    m_rpWidget(NULL),
    m_rlWidget(NULL),
    m_s21Widget(NULL),
    m_tdrWidget(NULL),
    m_smithWidget(NULL),
    m_markersHint(NULL),
    m_markersHintEnabled(true),
    m_measurements(NULL)
{
    QString path = Settings::setIniFile();
    m_settings = new QSettings(path, QSettings::IniFormat);
    m_settings->beginGroup("Markers");

    m_markersHintEnabled = m_settings->value("markersHintEnabled", true).toBool();

    m_settings->endGroup();

    if(m_markersHint == NULL)
    {
        // Parentless here -- MainWindow reparents it into mainwindow.ui's
        // markersPanelContainer right after constructing this Markers
        // object (see MainWindow's constructor). It used to be a top-level
        // floating Qt::Tool window and never needed a parent at all.
        m_markersHint = new MarkersPanel();
        updateHintVisibility();
        connect(m_markersHint, SIGNAL(removeMarker(int)), SLOT(on_removeMarker(int)));
        connect(m_markersHint, &MarkersPanel::clearAllMarkers, this, &Markers::on_removeAllMarkers);
        connect(m_markersHint, &MarkersPanel::markerActivated, this, [this](int i) {
            if (i >= 0 && i < m_markersList.length())
                emit markerActivated(m_markersList.at(i)->frequency);
        });
        connect(m_markersHint, &MarkersPanel::changeColumns, this, [&](){ repaint(); });
        repaint();
    }
}

Markers::~Markers()
{
    m_settings->beginGroup("Markers");
    m_settings->setValue("markersHintEnabled", m_markersHintEnabled);
    m_settings->endGroup();

    // Was: explicit `delete m_markersHint;` here, correct back when it was
    // a parentless top-level Qt::Tool popup this was the sole owner of. Now
    // that it's docked (MainWindow reparents it into mainwindow.ui's
    // markersPanelContainer right after constructing this object -- see
    // MainWindow's constructor), Qt's own widget-tree teardown owns and
    // deletes it instead: MainWindow::~MainWindow() destroys the whole `ui`
    // widget tree (markersPanelContainer included) before this Markers
    // object -- a plain QObject parented directly to MainWindow, not part
    // of that widget tree -- gets destroyed in turn. Deleting it again here
    // was a double-delete on an already-freed pointer (m_markersHint isn't
    // a QPointer, so it doesn't know the widget tree beat it to it) --
    // confirmed 2026-09-01 via a segfault in QObjectPrivate::deleteChildren()
    // unwinding straight into this line.
}

void Markers::setWidgets(QCustomPlot * swr, QCustomPlot * phase, QCustomPlot * rs, QCustomPlot * rp,
                         QCustomPlot * rl, QCustomPlot * tdr, QCustomPlot * s21, QCustomPlot * smith)
{
    m_swrWidget = swr;
    m_phaseWidget = phase;
    m_rsWidget = rs;
    m_rpWidget = rp;
    m_rlWidget = rl;
    m_tdrWidget = tdr;
    m_s21Widget = s21;
    m_smithWidget = smith;
}

void Markers::setMeasurements(Measurements *m)
{
    m_measurements = m;
}

void Markers::create(double fq)
{
    marker *m = new marker();
    m->frequency = fq;

    const QColor markerColor = Style::theme().marker;

    m->swrLine = new QCPItemStraightLine(m_swrWidget);
    m->swrLineText = new QCPItemText(m_swrWidget);
    m->swrLine->setAntialiased(false);
    m->swrLine->setPen(QPen(markerColor));
    m->swrLineText->setColor(markerColor);

    m->phaseLine = new QCPItemStraightLine(m_phaseWidget);
    m->phaseLineText = new QCPItemText(m_phaseWidget);
    m->phaseLine->setAntialiased(false);
    m->phaseLine->setPen(QPen(markerColor));
    m->phaseLineText->setColor(markerColor);

    m->rsLine = new QCPItemStraightLine(m_rsWidget);
    m->rsLineText = new QCPItemText(m_rsWidget);
    m->rsLine->setAntialiased(false);
    m->rsLine->setPen(QPen(markerColor));
    m->rsLineText->setColor(markerColor);

    m->rpLine = new QCPItemStraightLine(m_rpWidget);
    m->rpLineText = new QCPItemText(m_rpWidget);
    m->rpLine->setAntialiased(false);
    m->rpLine->setPen(QPen(markerColor));
    m->rpLineText->setColor(markerColor);

    m->rlLine = new QCPItemStraightLine(m_rlWidget);
    m->rlLineText = new QCPItemText(m_rlWidget);
    m->rlLine->setAntialiased(false);
    m->rlLine->setPen(QPen(markerColor));
    m->rlLineText->setColor(markerColor);

    m->s21Line = new QCPItemStraightLine(m_s21Widget);
    m->s21LineText = new QCPItemText(m_s21Widget);
    m->s21Line->setAntialiased(false);
    m->s21Line->setPen(QPen(markerColor));
    m->s21LineText->setColor(markerColor);

    m_markersList.append(m);
}

void Markers::setFq(double fq)
{
    if(m_markersList.length() == 0)
    {
        return;
    }

    m_markersList.last()->frequency = fq;

    m_markersList.last()->swrLine->point1->setCoords(fq, MIN_SWR);
    m_markersList.last()->swrLine->point2->setCoords(fq, MAX_SWR);

    double offsetX = (m_swrWidget->xAxis->range().upper - m_swrWidget->xAxis->range().lower)/40;
    double offsetY = (m_swrWidget->yAxis->range().upper - m_swrWidget->yAxis->range().lower)/10;
    m_markersList.last()->swrLineText->position->setCoords(fq + offsetX, m_swrWidget->yAxis->range().center()-offsetY);
    m_markersList.last()->swrLineText->setText(QString::number(fq));

    //==========================================================================
    m_markersList.last()->phaseLine->point1->setCoords(fq, -180);
    m_markersList.last()->phaseLine->point2->setCoords(fq, 180);

    offsetX = (m_phaseWidget->xAxis->range().upper - m_phaseWidget->xAxis->range().lower)/40;
    offsetY = (m_phaseWidget->yAxis->range().upper - m_phaseWidget->yAxis->range().lower)/10;
    m_markersList.last()->phaseLineText->position->setCoords(fq + offsetX, m_phaseWidget->yAxis->range().center()-offsetY);
    m_markersList.last()->phaseLineText->setText(QString::number(fq));

    //==========================================================================
    m_markersList.last()->rsLine->point1->setCoords(fq, -1600);
    m_markersList.last()->rsLine->point2->setCoords(fq, 1600);

    offsetX = (m_rsWidget->xAxis->range().upper - m_rsWidget->xAxis->range().lower)/40;
    offsetY = (m_rsWidget->yAxis->range().upper - m_rsWidget->yAxis->range().lower)/10;
    m_markersList.last()->rsLineText->position->setCoords(fq + offsetX, m_rsWidget->yAxis->range().center()-offsetY);
    m_markersList.last()->rsLineText->setText(QString::number(fq));

    //==========================================================================
    m_markersList.last()->rpLine->point1->setCoords(fq, -1600);
    m_markersList.last()->rpLine->point2->setCoords(fq, 1600);

    offsetX = (m_rpWidget->xAxis->range().upper - m_rpWidget->xAxis->range().lower)/40;
    offsetY = (m_rpWidget->yAxis->range().upper - m_rpWidget->yAxis->range().lower)/10;
    m_markersList.last()->rpLineText->position->setCoords(fq + offsetX, m_rpWidget->yAxis->range().center()-offsetY);
    m_markersList.last()->rpLineText->setText(QString::number(fq));

    //==========================================================================
    m_markersList.last()->rlLine->point1->setCoords(fq, 0);
    m_markersList.last()->rlLine->point2->setCoords(fq, 60);

    offsetX = (m_rlWidget->xAxis->range().upper - m_rlWidget->xAxis->range().lower)/40;
    offsetY = (m_rlWidget->yAxis->range().upper - m_rlWidget->yAxis->range().lower)/10;
    m_markersList.last()->rlLineText->position->setCoords(fq + offsetX, m_rlWidget->yAxis->range().center()-offsetY);
    m_markersList.last()->rlLineText->setText(QString::number(fq));

    //==========================================================================
    m_markersList.last()->s21Line->point1->setCoords(fq, 0);
    m_markersList.last()->s21Line->point2->setCoords(fq, 60);

    offsetX = (m_s21Widget->xAxis->range().upper - m_s21Widget->xAxis->range().lower)/40;
    offsetY = (m_s21Widget->yAxis->range().upper - m_s21Widget->yAxis->range().lower)/10;
    m_markersList.last()->s21LineText->position->setCoords(fq + offsetX, m_s21Widget->yAxis->range().center()-offsetY);
    m_markersList.last()->s21LineText->setText(QString::number(fq));

    redraw();
}

void Markers::rescale()
{
    for(int i = 0; i < m_markersList.length(); ++i)
    {
        double fq = m_markersList.at(i)->frequency;
        double offsetX;
        double offsetY;

        if(m_currentTab == "tab_swr")
        {
            offsetX = (m_swrWidget->xAxis->range().upper - m_swrWidget->xAxis->range().lower)/40;
            offsetY = (m_swrWidget->yAxis->range().upper - m_swrWidget->yAxis->range().lower)/10;
            m_markersList.at(i)->swrLineText->position->setCoords(fq + offsetX/2, m_swrWidget->yAxis->range().center()-offsetY);
        }else if(m_currentTab == "tab_phase")
        {
            offsetX = (m_phaseWidget->xAxis->range().upper - m_phaseWidget->xAxis->range().lower)/40;
            offsetY = (m_phaseWidget->yAxis->range().upper - m_phaseWidget->yAxis->range().lower)/10;
            m_markersList.at(i)->phaseLineText->position->setCoords(fq + offsetX/2, m_phaseWidget->yAxis->range().center()-offsetY);
        }else if(m_currentTab == "tab_rs")
        {
            offsetX = (m_rsWidget->xAxis->range().upper - m_rsWidget->xAxis->range().lower)/40;
            offsetY = (m_rsWidget->yAxis->range().upper - m_rsWidget->yAxis->range().lower)/10;
            m_markersList.at(i)->rsLineText->position->setCoords(fq + offsetX/2, m_rsWidget->yAxis->range().center()-offsetY);
        }else if(m_currentTab == "tab_rp")
        {
            offsetX = (m_rpWidget->xAxis->range().upper - m_rpWidget->xAxis->range().lower)/40;
            offsetY = (m_rpWidget->yAxis->range().upper - m_rpWidget->yAxis->range().lower)/10;
            m_markersList.at(i)->rpLineText->position->setCoords(fq + offsetX/2, m_rpWidget->yAxis->range().center()-offsetY);
        }else if(m_currentTab == "tab_rl")
        {
            offsetX = (m_rlWidget->xAxis->range().upper - m_rlWidget->xAxis->range().lower)/40;
            offsetY = (m_rlWidget->yAxis->range().upper - m_rlWidget->yAxis->range().lower)/10;
            m_markersList.at(i)->rlLineText->position->setCoords(fq + offsetX/2, m_rlWidget->yAxis->range().center()-offsetY);
        }else if(m_currentTab == "tab_s21")
        {
            offsetX = (m_s21Widget->xAxis->range().upper - m_s21Widget->xAxis->range().lower)/40;
            offsetY = (m_s21Widget->yAxis->range().upper - m_s21Widget->yAxis->range().lower)/10;
            m_markersList.at(i)->s21LineText->position->setCoords(fq + offsetX/2, m_s21Widget->yAxis->range().center()-offsetY);
        }else if(m_currentTab == "tab_tdr")
        {
        }else if(m_currentTab == "tab_smith")
        {
        }
    }
}

void Markers::add()
{
    if(m_markersHint == NULL)
    {
        return;
    }
    for(int i = 0; i < m_markersList.length(); ++i)
    {
        QString index = QString::number(MarkerList<marker>::number(i));
        m_markersList.at(i)->swrLineText->setText(index);
        m_markersList.at(i)->phaseLineText->setText(index);
        m_markersList.at(i)->rsLineText->setText(index);
        m_markersList.at(i)->rpLineText->setText(index);
        m_markersList.at(i)->rlLineText->setText(index);
        m_markersList.at(i)->s21LineText->setText(index);
    }

    changeMarkersHint();
    redraw();
    updateHintVisibility();
    emit markersChanged();
}

void Markers::updateHintVisibility()
{
    if (m_markersHint)
        m_markersHint->setVisible(m_markersHintEnabled && !m_panelSuppressed);
}

void Markers::repaint()
{
    if(!m_measurements)
    {
        return;
    }
    QList<int> types = m_markersHint->getColumns();
    QList<QList<QVariant>> info = updateInfo(types);
    m_markersHint->updateInfo(info);
}

QList<QList<QVariant>> Markers::updateInfo(QList<int> _columnTypes)
{
    QList<QList<QVariant>> info;

    for(int n = 0; n < m_markersList.length(); ++n)
    {
        double fq0 = m_markersList.at(n)->frequency;

        int count = m_measurements->getMeasurementLength();
        if (count == 0) {
            // No scan yet -- still show the marker's own number/frequency
            // (known the instant it's placed) instead of the row simply
            // not appearing. See MarkersPanel::updateInfo()'s matching
            // rowCount fallback, which is what actually renders this row.
            info << emptyMarkerRow(fq0, n+1, _columnTypes);
            continue;
        }
        for(int i=count-1; i>=0; i--)
        {
            info << computeMarkerRow(fq0, n+1, i, _columnTypes);
        } // for (m_measurements)
    } // for (m_markersList)
    return info;
}

QList<QVariant> Markers::emptyMarkerRow(double fq0, int markerNumber, const QList<int>& _columnTypes)
{
    QList<QVariant> row;
    row << QVariant();             // fieldDelete
    row << QVariant(markerNumber); // fieldMarker
    row << QVariant();             // fieldSerie -- no measurement to number yet
    row << QVariant(fq0);          // fieldFQ
    for (int j = MarkersHeaderColumn::fieldFQ+1; j < _columnTypes.size(); j++)
        row << QVariant(); // SWR/RL/R/X/... all unknown until there's a scan
    return row;
}

// Single-marker/single-measurement version of the row body updateInfo()
// loops over -- factored out so a caller that only cares about one
// marker (e.g. TunerHelperDialog) can reuse the exact same interpolation/
// calibration/far-end-adjustment logic instead of duplicating it.
QList<QVariant> Markers::computeMarkerRow(double fq0, int markerNumber, int i, const QList<int>& _columnTypes)
{
            QList<QVariant> row;
            // measurement::serialNumber -- a stable per-measurement id
            // assigned once at creation (Measurements::nextSerialNumber()),
            // not the measurement's transient position in the list (which
            // this used to fall back to, and which could -- and did --
            // collide with an unrelated measurement's real scan number).
            int index = m_measurements->getMeasurement(i)->serialNumber;
            row << QVariant(); // fieldDelete
            row << QVariant(markerNumber); // fieldMarker
            row << QVariant(index); // fieldSerie
            row << QVariant(fq0); // fieldFQ

            // Values come from the measurement's data via the same per-point
            // formulas the charts draw (MarkerMath::chartPoint()).
            MarkerMath::Values v = MarkerMath::valuesAt(*m_measurements->getMeasurement(i),
                                                        m_measurements->shownFarEnd(i),
                                                        m_measurements->shownOsl(i),
                                                        fq0, m_measurements->getZ0());
            double dSwr = v.swr, dRl = v.rl, dR = v.r, dX = v.x, dZmod = v.zmod;
            double dL = v.l, dC = v.c, dPhase = v.phase, dRho = v.rho;
            double dRpar = v.rpar, dXpar = v.xpar, dLpar = v.lpar, dCpar = v.cpar;
            double dS21 = v.s21, dS21Phase = v.s21Phase, dS12 = v.s12, dS12Phase = v.s12Phase;

            QString zString;
            QString zparString;
            if (v.found)
            {
                zString += QString::number(dR,'f', 2);
                if(dX >= 0)
                {
                    zString+= " + j";
                    zString+= QString::number(dX,'f', 2);
                }else
                {
                    zString+= " - j";
                    zString+= QString::number((dX * (-1)),'f', 2);
                }
                zparString += QString::number(dRpar,'f', 2);
                if(dXpar >= 0)
                {
                    zparString+= " + j";
                    zparString+= QString::number(dXpar,'f', 2);
                }else
                {
                    zparString+= " - j";
                    zparString+= QString::number((dXpar * (-1)),'f', 2);
                }
            }
            for (int j=MarkersHeaderColumn::fieldFQ+1; j<_columnTypes.size(); j++) {
                switch (_columnTypes[j]) {
                case MarkersHeaderColumn::fieldSWR:
                    row << QVariant(dSwr);
                    break;
                case MarkersHeaderColumn::fieldRL:
                    row << QVariant(dRl);
                    break;
                case MarkersHeaderColumn::fieldPhase:
                    row << QVariant(dPhase);
                    break;
                case MarkersHeaderColumn::fieldR:
                    row << QVariant(dR);
                    break;
                case MarkersHeaderColumn::fieldX:
                    row << QVariant(dX);
                    break;
                case MarkersHeaderColumn::fieldL:
                    row << QVariant(dL);
                    break;
                case MarkersHeaderColumn::fieldC:
                    row << QVariant(dC);
                    break;
                case MarkersHeaderColumn::fieldRpar:
                    row << QVariant(dRpar);
                    break;
                case MarkersHeaderColumn::fieldXpar:
                    row << QVariant(dXpar);
                    break;
                case MarkersHeaderColumn::fieldLpar:
                    row << QVariant(dLpar);
                    break;
                case MarkersHeaderColumn::fieldCpar:
                    row << QVariant(dCpar);
                    break;
                case MarkersHeaderColumn::fieldRho:
                    row << QVariant(dRho);
                    break;
                case MarkersHeaderColumn::fieldZ:
                    row << QVariant(zString);
                    break;
                case MarkersHeaderColumn::fieldZpar:
                    row << QVariant(zparString);
                    break;
                case MarkersHeaderColumn::fieldZmod:
                    row << QVariant(dZmod);
                    break;
                case MarkersHeaderColumn::fieldS21:
                    row << QVariant(dS21);
                    break;
                case MarkersHeaderColumn::fieldS21Phase:
                    row << QVariant(dS21Phase);
                    break;
                case MarkersHeaderColumn::fieldS12:
                    row << QVariant(dS12);
                    break;
                case MarkersHeaderColumn::fieldS12Phase:
                    row << QVariant(dS12Phase);
                    break;
                default:
                    row << QVariant();
                    break;
                }
            }
            return row;
}

// Single marker, most recent measurement only -- what TunerHelperDialog
// actually wants (it doesn't care about every marker x every measurement
// the way the Markers popup table does). markerNumber is 1-based, same
// convention as fieldMarker/getMarker(). Empty list if markerNumber is out
// of range or there's no measurement yet to read values from.
QList<QVariant> Markers::valuesForMarkerNumber(int markerNumber, const QList<int>& columnTypes)
{
    if (markerNumber < 1 || markerNumber > m_markersList.length() || m_measurements->isEmpty())
        return QList<QVariant>();

    double fq0 = m_markersList.at(markerNumber - 1)->frequency;
    int mostRecent = 0; // getMeasurement() indexes backwards from newest -- 0 is most recent (see measurements.h)
    return computeMarkerRow(fq0, markerNumber, mostRecent, columnTypes);
}

void Markers::on_currentTab(QString name)
{
    m_currentTab = name;
    rescale();
}

void Markers::on_newMeasurement(QString )
{
}

void Markers::on_measurementComplete()
{
    changeMarkersHint();
}

// See markers.h for the full contract. Callers are responsible for only
// invoking this on single/full scan completion (never Continuous) --
// nothing here checks that itself.
void Markers::autoPlaceAtLowestSwr()
{
    if (!m_markersList.wantsAutoMarker(AppConfig::get().autoMarkerAtLowestSwr, AppConfig::get().maxMarkers))
        return; // off, or no free slot -- silent no-op, see markers.h

    if (m_measurements == nullptr || m_measurements->isEmpty())
        return;

    // Search whatever trace the user is looking at (far-end / calibrated).
    int mostRecent = 0; // getMeasurement()/Sub()/Add() index backwards from newest -- 0 is most recent
    double bestFq;
    if (!MarkerMath::lowestSwr(MarkerMath::swrSeries(*m_measurements->last(),
                                                     m_measurements->shownFarEnd(mostRecent),
                                                     m_measurements->shownOsl(mostRecent),
                                                     m_measurements->getZ0()), &bestFq))
        return;

    create(bestFq);
    setFq(bestFq);
    add();
}

void Markers::setMarkersHintEnabled(bool enabled)
{
    m_markersHintEnabled = enabled;
    updateHintVisibility();
}

bool Markers::getMarkersHintEnabled(void)
{
    return m_markersHintEnabled;
}

void Markers::saveBmp(QString path)
{
    if(m_markersHint)
    {
        QPixmap map = m_markersHint->grab();
        QPixmap mapScaled = map.scaled(5000,3000,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        mapScaled.save(path,"BMP",100);
    }
}

bool Markers::canAddMarker() const
{
    return m_markersList.canAdd(AppConfig::get().maxMarkers);
}

qint32 Markers::getMarkersCount()
{
    return m_markersList.length();
}
marker Markers::getMarker( quint32 number)
{
    return *m_markersList.at(number);
}

void Markers::redraw(void)
{
    rescale();
    if(m_currentTab == "tab_swr")
    {
        // Third copy of the same SWR-Y-label-blanking hack found in this
        // app (see Measurements::replot()'s comment, measurements_redraw.cpp,
        // and MainWindow::replotY_swr(), mainwindow_mouse.cpp) -- all three
        // replaced by SwrAxisTicker (CustomPlot.h) in the 2026-08-25
        // QCustomPlot 2.x port, which blanks the same labels as part of
        // computing them, on every replot regardless of trigger.
        m_swrWidget->replot();
    }else if(m_currentTab == "tab_phase")
    {
        m_phaseWidget->replot();
    }else if(m_currentTab == "tab_rs")
    {
        m_rsWidget->replot();
    }else if(m_currentTab == "tab_rp")
    {
        m_rpWidget->replot();
    }else if(m_currentTab == "tab_rl")
    {
        m_rlWidget->replot();
    }else if(m_currentTab == "tab_s21")
    {
        m_s21Widget->replot();
    }else if(m_currentTab == "tab_tdr")
    {
        m_tdrWidget->replot();
    }else if(m_currentTab == "tab_smith")
    {
        m_smithWidget->replot();
    }
#ifndef NO_MULTITAB
    else if(m_currentTab == "tab_multi")
    {
        QString old_m_currentTab = m_currentTab;
        const QList<QString>& tabs = MainWindow::m_mainWindow->multiTabs();
        foreach (const QString& tab, tabs) {
            QCustomPlot* plot = MainWindow::m_mainWindow->plotForTab(tab);
            plot->replot();
        }
        m_currentTab = old_m_currentTab;
    }
#endif
}

void Markers::on_removeMarker(int number)
{
    // The index comes from a button object name, so it can go stale if the
    // popup and this list ever disagree. Bound it rather than trusting it.
    if(number < 0 || number >= m_markersList.length())
    {
        return;
    }

    marker *m = m_markersList.take(number);
    m->clear();
    delete m;

    for(int i = 0; i < m_markersList.length(); ++i)
    {
        QString index = QString::number(MarkerList<marker>::number(i));
        m_markersList.at(i)->swrLineText->setText(index);
        m_markersList.at(i)->phaseLineText->setText(index);
        m_markersList.at(i)->rsLineText->setText(index);
        m_markersList.at(i)->rpLineText->setText(index);
        m_markersList.at(i)->rlLineText->setText(index);
        m_markersList.at(i)->s21LineText->setText(index);
    }

    changeMarkersHint();
    redraw();
    // redraw() only replots whichever tab is currently tracked as active; the
    // marker's line/label lives on every plot at once, so force all of them
    // to drop the just-removed item instead of leaving it visible until the
    // user happens to switch tabs.
    m_swrWidget->replot();
    m_phaseWidget->replot();
    m_rsWidget->replot();
    m_rpWidget->replot();
    m_rlWidget->replot();
    m_s21Widget->replot();
    updateHintVisibility();
    emit markersChanged();
}

void Markers::on_removeAllMarkers()
{
    if (m_markersList.isEmpty())
        return;

    for (marker* m : m_markersList) {
        m->clear();
        delete m;
    }
    m_markersList.clear();

    changeMarkersHint();
    redraw();
    // See on_removeMarker()'s identical comment -- redraw() only replots the
    // currently-active tab, so force every one to drop the removed markers.
    m_swrWidget->replot();
    m_phaseWidget->replot();
    m_rsWidget->replot();
    m_rpWidget->replot();
    m_rlWidget->replot();
    m_s21Widget->replot();
    updateHintVisibility();
    emit markersChanged();
}

void Markers::on_translate()
{
    if (m_markersHint != nullptr)
        m_markersHint->on_translate();
}

void Markers::changeColorTheme()
{
    // m_markersHint used to need re-coloring here too, back when it tracked
    // the plot's own chart-background instead of the app theme (same class
    // of fix as Measurements' m_graphHint/m_graphBriefHint -- see
    // measurements_popups.cpp) -- now it's a plain docked, normally-themed
    // QTableWidget that qApp->setStyleSheet()/setPalette() (MainWindow::
    // changeColorTheme()) already re-skins for free, same as every other
    // table in the app.

    // Markers::create() only sets each line/text pair's color once, at
    // creation time -- a marker already on the chart when the theme (or
    // just its marker color) changes never got told about it, unlike a
    // freshly-placed one, which picks up Style::theme().marker fresh via
    // create() itself. Re-apply to every existing marker here.
    const QColor markerColor = Style::theme().marker;
    for (marker* m : m_markersList) {
        m->swrLine->setPen(QPen(markerColor));
        m->swrLineText->setColor(markerColor);
        m->phaseLine->setPen(QPen(markerColor));
        m->phaseLineText->setColor(markerColor);
        m->rsLine->setPen(QPen(markerColor));
        m->rsLineText->setColor(markerColor);
        m->rpLine->setPen(QPen(markerColor));
        m->rpLineText->setColor(markerColor);
        m->rlLine->setPen(QPen(markerColor));
        m->rlLineText->setColor(markerColor);
        m->s21Line->setPen(QPen(markerColor));
        m->s21LineText->setColor(markerColor);
    }
}

void Markers::changeMarkersHint()
{
    if (!m_measurements) {
        return;
    }
    // Always push the count through, including zero -- otherwise removing the
    // last marker left the popup holding its old rows.
    m_markersHint->updateMarkers(m_markersList.size(), m_measurements->getMeasurementLength());
    if (!m_markersList.isEmpty()) {
        repaint();
    }
    // markersChanged() is emitted by add()/on_removeMarker() themselves, not
    // here -- on_measurementComplete() also routes through this function on
    // every scan tick, and that already has its own signal
    // (MainWindow::on_actionMarkerComparison_triggered() wires up both), so
    // emitting from here too would fire MarkerComparisonDialog::refresh()
    // twice per tick.
}
