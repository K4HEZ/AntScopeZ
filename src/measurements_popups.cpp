#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"
#include "../analyzer/qcpgraphdatahelpers.h"

extern QMap<QString, QString> g_mapTabPlotNames;
extern int g_maxMeasurements; // defined in measurements.cpp
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

double regulate(double val, double limit); // defined in measurements.cpp

// Tier-1 mechanical split of the original measurements.cpp (still in
// measurements.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of Measurements.

void Measurements::setGraphHintEnabled(bool enabled)
{
    m_graphHintEnabled = enabled;
    showHideHints();
}

void Measurements::setGraphBriefHintEnabled(bool enabled)
{
    m_graphBriefHintEnabled = enabled;
    showHideHints();
}

void Measurements::on_focus(bool focus)
{
    m_focus = focus;
    // Deferred, not called inline: this runs from inside MainWindow::event()
    // handling QEvent::WindowActivate. showHideHints() QWidget::show()s
    // m_graphBriefHint -- a separate top-level Qt::Tool window (see
    // popup.cpp) deliberately made activatable so it can be dragged on this
    // window manager. (m_graphHintBox used to be one too, until it was
    // docked into mainwindow.ui's middle column -- see setGraphHintWidgets()
    // -- so it no longer has a WM identity to race against.) Doing that
    // show() synchronously, nested inside MainWindow's own WindowActivate
    // handling, races MainWindow's activation against the popup's on window
    // managers like Cinnamon/Muffin -- most visibly on cold start, when
    // m_graphBriefHint (never shown before this point) and MainWindow both
    // request activation for the first time within the same event. Symptom:
    // the plot's mouse wheel/drag stop responding until something else
    // (e.g. opening/closing the modal "Connect analyzer" dialog) forces a
    // fresh activation cycle. Letting MainWindow's own activation finish
    // settling first, by deferring showHideHints() a tick, avoids the race.
    QTimer::singleShot(50, this, &Measurements::showHideHints);
}

void Measurements::hideGraphBriefHint()
{
    if(m_graphBriefHint)
    {
        m_graphBriefHint->focusHide();
    }
}

void Measurements::showHideHints()
{
    if(m_graphHintBox)
    {
        // Unlike m_graphBriefHint below, m_graphHintBox isn't interactive-
        // until-positioned -- it's always had real text since construction
        // (see setGraphHintWidgets()) -- so there's no equivalent reason to
        // gate it on m_focus. Gating it did mean opening the (non-modal)
        // Settings dialog -- which steals MainWindow's WM activation,
        // triggering on_focus(false) -- hid it regardless of the checkbox,
        // and toggling the checkbox while Settings had focus appeared to do
        // nothing since m_focus stayed false either way. Checkbox state
        // alone decides visibility here, same as before docking.
        m_graphHintBox->setVisible(m_graphHintEnabled);
    }
    if(m_graphBriefHint)
    {
        // Unlike m_graphHintBox, m_graphBriefHint is never proactively shown
        // here. It starts with no text and, because PopUp::setName() only
        // restores a persisted position for the name "Hint" (not
        // "BriefHint"), it always sits at PopUp's hardcoded construction
        // default (850,130) on every launch -- which lands right on top of
        // the plot. It only gets real text/position once the cursor is
        // actually over a plot with a measurement selected (see
        // on_newCursorFq() below, and the ->show() call where a measurement
        // is named). Showing it here, on window activate, before any of
        // that has happened made it an invisible-but-fully-interactive
        // window sitting on top of the plot from the moment the app
        // activates -- silently eating mouse/wheel input for whatever
        // screen region it covered, which looked exactly like the plot's
        // wheel/drag being "stuck" until something else (switching
        // windows, then back; selecting an analyzer) happened to hide or
        // move it out of the way. Still fine -- correct, even -- to hide it
        // eagerly here on focus loss/disable.
        if(!(m_graphBriefHintEnabled && m_focus))
        {
            m_graphBriefHint->focusHide();
        }
    }
}

void Measurements::on_newCursorFq(double x, int number, int mouseX, int mouseY)
{
    updatePopUp(x, number, mouseX, mouseY);
}

void Measurements::on_newCursorSmithPos (double x, double y, int index)
{
    QCPCurveDataContainer map;
    if((m_calibration != NULL) && (m_calibration->getCalibrationEnabled()))
    {
        map = m_measurements.at(index).smithGraphCalib;
    }else
    {
        map = m_measurements.at(index).smithGraph;
    }
    // QCPCurveDataContainer has no .values() (2026-08-25 QCustomPlot 2.x
    // port) -- it's already index-accessible directly, no QList copy needed.
    if(map.isEmpty())
    {
        return;
    }
    int findedNum = 0;
    bool finded = false;
    for(double n = 0; n < 6; n+=0.1)
    {
        for(int i = 0; i < map.size(); ++i)
        {
            if( ((x >= map.at(i)->key - n) && (x <= map.at(i)->key + n)) &&
                 (y >= map.at(i)->value - n) && (y <= map.at(i)->value + n))
            {
                findedNum = i;
                finded = true;
                break;
            }
        }
        if(finded)
        {
            break;
        }
    }
    if(m_smithTracer == NULL)
    {
        m_smithTracer = new QCPItemEllipse(m_smithWidget);
    }
    m_smithTracer->setAntialiased(true);
    QPen pen;
    pen.setColor(QColor(250,30,20,180));
    pen.setWidth(4);
    m_smithTracer->setPen(pen);
    m_smithTracer->topLeft->setCoords(map.at(findedNum)->key-0.1, map.at(findedNum)->value+0.1);
    m_smithTracer->bottomRight->setCoords(map.at(findedNum)->key+0.1, map.at(findedNum)->value-0.1);
    m_smithWidget->replot();


    QCPGraphDataContainer *swrmap;
    if((m_calibration != NULL) && (m_calibration->getCalibrationEnabled()))
    {
        swrmap = &(m_measurements[index].swrGraphCalib);
    }else
    {
        swrmap = &(m_measurements[index].swrGraph);
    }
    // No .keys() on QCPGraphDataContainer (2026-08-25 QCustomPlot 2.x
    // port) -- swrmap->at(n) replaces swrmap->at(n)->key directly below.

    double frequency = 0;
    double swr = 0;
    double phase = 0;
    double rho = 0;
    double rl = 0;
    double l = 0;
    double c = 0;
    double lpar = 0;
    double cpar = 0;
    double r = 0;
    double x1 = 0;
    double z = 0;
    double rpar = 0;
    double xpar = 0;
    QString zString;
    QString zparString;

    frequency = findedNum < swrmap->size() ? swrmap->at(findedNum)->key : swrmap->at(swrmap->size()-1)->key;

    if(m_calibration->getCalibrationEnabled())
    {
        if(m_farEndMeasurement == 1)
        {
            rho = graphValueAt(m_farEndMeasurementsSub.at(index).rhoGraph, frequency);
            phase = graphValueAt(m_farEndMeasurementsSub.at(index).phaseGraph, frequency);
            r = graphValueAt(m_farEndMeasurementsSub.at(index).rsrGraph, frequency);//dataRX.at(previousI).r;
            x1 = graphValueAt(m_farEndMeasurementsSub.at(index).rsxGraph, frequency);//dataRX.at(previousI).x;
        }else if(m_farEndMeasurement == 2)
        {
            rho = graphValueAt(m_farEndMeasurementsAdd.at(index).rhoGraph, frequency);
            phase = graphValueAt(m_farEndMeasurementsAdd.at(index).phaseGraph, frequency);
            r = graphValueAt(m_farEndMeasurementsAdd.at(index).rsrGraph, frequency);//dataRX.at(previousI).r;
            x1 = graphValueAt(m_farEndMeasurementsAdd.at(index).rsxGraph, frequency);//dataRX.at(previousI).x;
        }else
        {
            rho = graphValueAt(m_measurements.at(index).rhoGraph, frequency);
            phase = graphValueAt(m_measurements.at(index).phaseGraphCalib, frequency);
            // findedNum comes from a nearest-point search over smithGraph/
            // smithGraphCalib (above), a separate container from dataRXCalib
            // that can end up a different size (e.g. across a Continuous
            // "continue" -- see on_newData()'s smith-point comment). Without
            // this clamp, .at(findedNum) below throws/aborts on an
            // out-of-range index. Matches RigExpert AntScope2 2.0.3's fix
            // (issue #10); was already here but commented out.
            if (findedNum >= m_measurements.at(index).dataRXCalib.count())
                findedNum = m_measurements.at(index).dataRXCalib.count()-1;
            if (findedNum < 0)
                findedNum = 0;
            r = m_measurements.at(index).dataRXCalib.at(findedNum).r;
            x1 = m_measurements.at(index).dataRXCalib.at(findedNum).x;
        }
        swr = graphValueAt(m_measurements.at(index).swrGraphCalib, frequency);
        rl = graphValueAt(m_measurements.at(index).rlGraphCalib, frequency);
//        z = graphValueAt(m_viewMeasurements.at(index).rszGraphCalib, frequency);
//        rpar = graphValueAt(m_viewMeasurements.at(index).rprGraphCalib, frequency);
//        xpar = graphValueAt(m_viewMeasurements.at(index).rpxGraphCalib, frequency);
        z = graphValueAt(m_measurements.at(index).rszGraphCalib, frequency);
        rpar = graphValueAt(m_measurements.at(index).rprGraphCalib, frequency);
        xpar = graphValueAt(m_measurements.at(index).rpxGraphCalib, frequency);
    }else
    {
        if(m_farEndMeasurement == 1)
        {
            rho = graphValueAt(m_farEndMeasurementsSub.at(index).rhoGraph, frequency);
            phase = graphValueAt(m_farEndMeasurementsSub.at(index).phaseGraph, frequency);
            r = graphValueAt(m_farEndMeasurementsSub.at(index).rsrGraph, frequency);//dataRX.at(previousI).r;
            x1 = graphValueAt(m_farEndMeasurementsSub.at(index).rsxGraph, frequency);//dataRX.at(previousI).x;
        }else if(m_farEndMeasurement == 2)
        {
            rho = graphValueAt(m_farEndMeasurementsAdd.at(index).rhoGraph, frequency);
            phase = graphValueAt(m_farEndMeasurementsAdd.at(index).phaseGraph, frequency);
            r = graphValueAt(m_farEndMeasurementsAdd.at(index).rsrGraph, frequency);//dataRX.at(previousI).r;
            x1 = graphValueAt(m_farEndMeasurementsAdd.at(index).rsxGraph, frequency);//dataRX.at(previousI).x;
        }else
        {
            rho = graphValueAt(m_measurements.at(index).rhoGraph, frequency);
            phase = graphValueAt(m_measurements.at(index).phaseGraph, frequency);
            // See the calibrated branch above for why this clamp exists.
            if (findedNum >= m_measurements.at(index).dataRX.count())
                findedNum = m_measurements.at(index).dataRX.count()-1;
            if (findedNum < 0)
                findedNum = 0;
            r = m_measurements.at(index).dataRX.at(findedNum).r;
            x1 = m_measurements.at(index).dataRX.at(findedNum).x;
        }
        swr = graphValueAt(m_measurements.at(index).swrGraph, frequency);
        rl = graphValueAt(m_measurements.at(index).rlGraph, frequency);
//        z = graphValueAt(m_viewMeasurements.at(index).rszGraph, frequency);
//        rpar = graphValueAt(m_viewMeasurements.at(index).rprGraph, frequency);
//        xpar = graphValueAt(m_viewMeasurements.at(index).rpxGraph, frequency);
        z = graphValueAt(m_measurements.at(index).rszGraph, frequency);
        rpar = graphValueAt(m_measurements.at(index).rprGraph, frequency);
        xpar = graphValueAt(m_measurements.at(index).rpxGraph, frequency);
    }

    const double maxRp = VALUE_LIMIT;
    zString+= QString::number(r,'f', 2);
    if(x1 >= 0)
    {
        if (x1 > maxRp)
            x1 = maxRp; // HUCK
        zString+= " + j";
        zString+= QString::number(x1,'f', 2);
    }else
    {
        if (x1 < -maxRp)
            x1 = -maxRp; // HUCK
        zString+= " - j";
        zString+= QString::number((x1 * (-1)),'f', 2);
    }
    zparString+= QString::number(rpar,'f', 2);
    if(x1 >= 0)
    {
        zparString+= " + j";
        zparString+= QString::number(xpar,'f', 2);
    }else
    {
        zparString+= " - j";
        zparString+= QString::number((xpar * (-1)),'f', 2);
    }
    l = 1E9 * x1 / (2*M_PI * frequency * 1E3);//nH
    c = 1E12 / (2*M_PI * frequency * (x1 * (-1)) * 1E3);//pF

    lpar = 1E9 * xpar / (2*M_PI * frequency * 1E3);//nH
    cpar = 1E12 / (2*M_PI * frequency * (xpar * (-1)) * 1E3);//pF

    QString str = QString::number(frequency, 'f', 2);
    int pos = str.indexOf(".");
    if(pos < 0)
    {
        if(str.length() > 6)
        {
            str.insert(str.length()-6," ");
        }
        if(str.length() > 3)
        {
            str.insert(str.length()-3," ");
        }
    }else
    {
        int len = str.length() - pos;
        if((str.length()-len) > 6)
        {
            str.insert(str.length()-len-6," ");
        }
        if((str.length()-len) > 3)
        {
            str.insert(str.length()-len-3," ");
        }
    }


    QList<QPair<QString, QString>> fields;
    fields << qMakePair(tr("Frequency"), str + " kHz")
           << qMakePair(tr("SWR"), QString::number(swr,'f', 2))
           << qMakePair(tr("RL"), QString::number(rl,'f', 2) + " dB")
           << qMakePair(tr("Z"), zString + " Ohm")
           << qMakePair(tr("|Z|"), QString::number(z,'f', 2) + " Ohm")
           << qMakePair(tr("|rho|"), QString::number(rho,'f', 2))
           << qMakePair(tr("Phase"), QString::number(phase,'f', 2) + " °");
    if (x1 > 0) {
        fields << qMakePair(tr("L"), QString::number(l,'f', 2) + " nH");
    } else {
        fields << qMakePair(tr("C"), QString::number(c,'f', 2) + " pF");
    }
    fields << qMakePair(tr("Zpar"), zparString + " Ohm");
    if (x1 > 0) {
        fields << qMakePair(tr("Lpar"), QString::number(lpar,'f', 2) + " nH");
    } else {
        fields << qMakePair(tr("Cpar"), QString::number(cpar,'f', 2) + " pF");
    }

    if(!m_farEndMeasurement)
    {
        QString lenUnits;
        if(m_measureSystemMetric)
        {
            lenUnits = tr("m");
        }else
        {
            lenUnits = tr("ft");
        }

        double len14 = SPEEDOFLIGHT/4/frequency/1000*m_cableVelFactor;
        double len12 = SPEEDOFLIGHT/2/frequency/1000*m_cableVelFactor;

        if(!m_measureSystemMetric)
        {
            len14 *= FEETINMETER;
            len12 *= FEETINMETER;
        }

        fields << qMakePair(tr("Cable"), tr("length(1/4) = %1 %2, length(1/2) = %3 %4")
                .arg(QString::number(len14,'f',2))
                .arg(lenUnits)
                .arg(QString::number(len12,'f',2))
                .arg(lenUnits));
    }

    setGraphHintFields(fields);
}

void Measurements::updatePopUp(double xPos, int index, int mouseX, int mouseY)
{
#define DELTA 5
    // hideGraphCursor() hides m_graphBriefHint via focusHide() (plain
    // QWidget::hide()) whenever the cursor leaves the data area, but nothing
    // else re-shows it on the way back in -- setPopupText()/setPosition()
    // below don't call show(), and showHideHints() only reacts to
    // focus/enable-checkbox events, not mouse movement. Without this, hiding
    // it once (leaving the plot, or the phase tab's out-of-range gap) hid it
    // for the rest of the session. Re-show here, right before we're about to
    // populate real content, whenever it's enabled but not currently
    // visible. m_graphHintBox doesn't need the equivalent -- it's never
    // hidden by hideGraphCursor() (see that function's own comment), only by
    // showHideHints(), which already keeps it in sync with the checkbox.
    if (m_graphBriefHintEnabled && m_focus && m_graphBriefHint && !m_graphBriefHint->isVisible())
        m_graphBriefHint->focusShow();

    if(!m_graphHintValueLabels.isEmpty())
    {
        if (!m_measurements[index].visible) {
            return;
        }
        if(m_currentTab == "tab_tdr")
        {
            QCPGraphDataContainer *tdrmapImp;
            QCPGraphDataContainer *tdrmapStep;
            QCPGraphDataContainer *tdrmapZ;
            double pdTdrImp;
            double pdTdrStep;
            double pdTdrZ;
            if(m_farEndMeasurement == 1)
            {
                if(m_measureSystemMetric)
                {
                    tdrmapImp = &(m_farEndMeasurementsSub[index].tdrImpGraph);
                    tdrmapStep = &(m_farEndMeasurementsSub[index].tdrStepGraph);
                    tdrmapZ = &(m_farEndMeasurementsSub[index].tdrZGraph);
                }else
                {
                    tdrmapImp = &(m_farEndMeasurementsSub[index].tdrImpGraphFeet);
                    tdrmapStep = &(m_farEndMeasurementsSub[index].tdrStepGraphFeet);
                    tdrmapZ = &(m_farEndMeasurementsSub[index].tdrZGraphFeet);
                }
            }else if(m_farEndMeasurement == 2)
            {
                if(m_measureSystemMetric)
                {
                    tdrmapImp = &(m_farEndMeasurementsAdd[index].tdrImpGraph);
                    tdrmapStep = &(m_farEndMeasurementsAdd[index].tdrStepGraph);
                    tdrmapZ = &(m_farEndMeasurementsAdd[index].tdrZGraph);
                }else
                {
                    tdrmapImp = &(m_farEndMeasurementsAdd[index].tdrImpGraphFeet);
                    tdrmapStep = &(m_farEndMeasurementsAdd[index].tdrStepGraphFeet);
                    tdrmapZ = &(m_farEndMeasurementsAdd[index].tdrZGraphFeet);
                }
            }else
            {
                if(m_measureSystemMetric)
                {
                    tdrmapImp = &(m_measurements[index].tdrImpGraph);
                    tdrmapStep = &(m_measurements[index].tdrStepGraph);
                    tdrmapZ = &(m_measurements[index].tdrZGraph);
                }else
                {
                    tdrmapImp = &(m_measurements[index].tdrImpGraphFeet);
                    tdrmapStep = &(m_measurements[index].tdrStepGraphFeet);
                    tdrmapZ = &(m_measurements[index].tdrZGraphFeet);
                }
            }

            // No .keys() on QCPGraphDataContainer (2026-08-25 QCustomPlot
            // 2.x port) -- tdrmapImp->at(n) replaces tdrkeys.at(n) directly
            // below (self-referencing, same container); tdrmapStep/tdrmapZ
            // are genuinely different containers so those two lookups go
            // through graphValueAt() (exact-key-or-zero, matching the old
            // QMap::value() semantics) instead.

            bool res = false;
            int start = m_previousI-DELTA;
            if(start < 0)
            {
                start = 0;
            }
            int stop = m_previousI+DELTA;
            if (stop > tdrmapImp->size()-1)
            {
                stop = tdrmapImp->size()-1;
            }
            for(int t = 0; t < 2; ++t)
            {
                if(t == 1)
                {
                    if(res)
                    {
                        break;
                    }
                    start = 0;
                    stop = tdrmapImp->size()-1;
                }
                double place;
                for(int i = start; i < stop; ++i)
                {
                    if((tdrmapImp->at(i)->key <= xPos) && (tdrmapImp->at(i+1)->key >= xPos))
                    {
                        double center = (tdrmapImp->at(i)->key + tdrmapImp->at(i+1)->key)/2;
                        if( xPos > center )
                        {
                            if(m_previousI == i+1)
                            {
                                return;
                            }
                            place = tdrmapImp->at(i+1)->key;
                            m_previousI = i+1;
                        }else
                        {
                            if(m_previousI == i)
                            {
                                return;
                            }
                            place = tdrmapImp->at(i)->key;
                            m_previousI = i;
                        }

                        pdTdrImp = graphValueAt(*tdrmapImp, place);
                        pdTdrStep = graphValueAt(*tdrmapStep, place);
                        pdTdrZ = graphValueAt(*tdrmapZ, place);

                        if(!m_tdrLine)
                        {
                            m_tdrLine = new QCPItemStraightLine(m_tdrWidget);
                            m_tdrLine->setAntialiased(false);
                        }
                        m_tdrLine->setPen(QPen(inverseChartBackground()));
                        m_tdrLine->setVisible(true);
                        m_tdrLine->point1->setCoords(place, -1);
                        m_tdrLine->point2->setCoords(place, 1);

                        double Z = m_Z0*(1+pdTdrStep)/(1-pdTdrStep);
                        if (Z<0)                        
                            Z = 0;
                        if (Z > VALUE_LIMIT)
                            Z = VALUE_LIMIT;

                        QString distance;

                        QString lenUnits;
                        QString timeNs;
                        distance = QString::number(place,'f',3);
                        double airLen = place/m_cableVelFactor;
                        QString distanceInAir = QString::number(airLen,'f',3);//FEETINMETER
                        if(m_measureSystemMetric)
                        {
                            lenUnits = "m";
                            timeNs = QString::number(airLen/0.299792458,'f',2);
                        }else
                        {
                            lenUnits = "ft";
                            timeNs = QString::number(airLen/FEETINMETER/0.299792458,'f',2);
                        }


                        QString ir = QString::number(pdTdrImp,'f',3);
                        QString sr = QString::number(pdTdrStep,'f',3);

                        QString zStr = QString::number(Z,'f',1);
                        setGraphHintFields({
                            {tr("Distance"), distance + " " + lenUnits},
                            {tr("Distance in air"), distanceInAir + " " + lenUnits},
                            {tr("Time"), timeNs + " ns"},
                            {tr("Impulse response"), ir},
                            {tr("Step response"), sr},
                            {tr("|Z|"), zStr + " Ohm"},
                        });

                        if(m_graphBriefHint != NULL)
                        {
                            m_graphBriefHint->setPosition(mouseX+1,mouseY+1);
                            QString str = QString(tr("Distance = %1 %2\n"
                                                      "|Z| = %3 Ohm"))
                                        .arg(distance)
                                        .arg(lenUnits)
                                        .arg(zStr);
                            m_graphBriefHint->setPopupText(str);
                        }

                        res = true;
                        break;
                    }
                }
            }
            // No bracketing data pair found anywhere for xPos (cursor is
            // past the end of the actual TDR trace, even though still
            // inside the widget's axis range) -- hide rather than leave the
            // crosshair/popups showing stale content from the last real hit
            // (or, before this fix, leftover text from a different tab).
            if (!res) {
                hideGraphCursor();
                return;
            }
        }else if(m_currentTab == "tab_s21") {
            if(m_graphBriefHint != NULL)
            {
                m_graphBriefHint->setPosition(mouseX+1,mouseY+1);
            }
            // s21Graph/s21StageGraph (used here previously) are the old
            // live-only scalar capture's maps -- never populated by a
            // .s2p import (see populateSParamData()), so this always came
            // up empty and hid the cursor entirely for any imported
            // measurement. s21MagGraph is populated by both a real import
            // and (once wired up) live capture, so it's the right key set
            // to search regardless of data source.
            QCPGraphDataContainer *s21map = &(m_measurements[index].s21MagGraph);
            double frequency = -1;
            double s21mag = 0;
            double s21phase = 0;
            double s12mag = 0;
            double s12phase = 0;
            bool res = false;
            // No .keys() on QCPGraphDataContainer (2026-08-25 QCustomPlot
            // 2.x port) -- s21map->at(n)->key replaces the old s21keys
            // snapshot's at(n) directly below.
            int start = m_previousI-DELTA;
            if(start < 0)
            {
                start = 0;
            }
            int stop = m_previousI+DELTA;
            if (stop > s21map->size()-1)
            {
                stop = s21map->size()-1;
            }
            for(int t = 0; t < 2; ++t)
            {
                if(t == 1)
                {
                    if(res)
                    {
                        break;
                    }
                    start = 0;
                    stop = s21map->size()-1;
                }
                for(int i = start; i < stop; ++i)
                {
                    if((s21map->at(i)->key <= xPos) && (s21map->at(i+1)->key >= xPos))
                    {
                        double center = (s21map->at(i)->key + s21map->at(i+1)->key)/2;
                        if( xPos > center )
                        {
                            if(m_previousI == i+1)
                            {
                                //return;
                            }
                            frequency = s21map->at(i+1)->key;
                            m_previousI = i+1;
                        }else
                        {
                            if(m_previousI == i)
                            {
                                //return;
                            }
                            frequency = s21map->at(i)->key;
                            m_previousI = i;
                        }
                    }
                    if (frequency < 0)
                        continue;
                    s21mag = graphValueAt(m_measurements.at(index).s21MagGraph, frequency);
                    s21phase = graphValueAt(m_measurements.at(index).s21PhaseGraph, frequency);
                    s12mag = graphValueAt(m_measurements.at(index).s12MagGraph, frequency);
                    s12phase = graphValueAt(m_measurements.at(index).s12PhaseGraph, frequency);
                    res = true;
                    if(m_graphBriefHint != NULL)
                    {
                        QString str;
                        str = QString::number(frequency, 'f', 2);
                        int idx = str.indexOf(".");
                        if(idx < 0)
                        {
                            if(str.length() > 6)
                            {
                                str.insert(str.length()-6," ");
                            }
                            if(str.length() > 3)
                            {
                                str.insert(str.length()-3," ");
                            }
                        }else
                        {
                            int len = str.length() - idx;
                            if((str.length()-len) > 6)
                            {
                                str.insert(str.length()-len-6," ");
                            }
                            if((str.length()-len) > 3)
                            {
                                str.insert(str.length()-len-3," ");
                            }
                        }
                        QString freqStr = str; // clean, no units yet -- for the graph-hint panel below
                        QList<QPair<QString, QString>> fields;
                        fields << qMakePair(tr("Frequency"), freqStr + " kHz")
                               << qMakePair(tr("S21"), QString::number(s21mag,'f', 2) + " dB")
                               << qMakePair(tr("S21 Phase"), QString::number(s21phase,'f', 2) + "°")
                               << qMakePair(tr("S12"), QString::number(s12mag,'f', 2) + " dB")
                               << qMakePair(tr("S12 Phase"), QString::number(s12phase,'f', 2) + "°");
                        setGraphHintFields(fields);

                        // Popup that follows the mouse stays terse (same
                        // convention as the swr/phase/rl tabs below) --
                        // just frequency + the one value this tab's own
                        // y-axis is actually labeled for.
                        str += " kHz\n" + QString::number(s21mag,'f',2);
                        m_graphBriefHint->setPopupText(str);
                        break;
                    }
                }
            }
            // See the identical check in the tab_tdr branch above.
            if (!res) {
                hideGraphCursor();
                return;
            }

            // Crosshair: vertical line at the frequency, horizontal line
            // at S21 magnitude -- the one value this tab's primary
            // (dB) axis is actually labeled for. S21 phase/S12 mag/phase
            // are still available via the Cursor Details panel above,
            // just not all four sensibly represented by one horizontal
            // line on two different axes/scales at once.
            if(!m_s21Line)
            {
                m_s21Line = new QCPItemStraightLine(m_s21Widget);
                m_s21Line->setAntialiased(false);
            }
            if(!m_s21Line2)
            {
                m_s21Line2 = new QCPItemStraightLine(m_s21Widget);
                m_s21Line2->setAntialiased(false);
            }
            QPen crosshairPen(inverseChartBackground());
            m_s21Line->setPen(crosshairPen);
            m_s21Line2->setPen(crosshairPen);
            m_s21Line->setVisible(true);
            m_s21Line2->setVisible(true);
            m_s21Line->point1->setCoords(frequency, -2000);
            m_s21Line->point2->setCoords(frequency, 2000);
            m_s21Line2->point1->setCoords(m_s21Widget->yAxis->range().lower, s21mag);
            m_s21Line2->point2->setCoords(m_s21Widget->yAxis->range().upper, s21mag);
        }else
        {
            if(m_graphBriefHint != NULL)
            {
                m_graphBriefHint->setPosition(mouseX+1,mouseY+1);
            }

            QCPGraphDataContainer *swrmap;
            if((m_calibration != NULL) && (m_calibration->getCalibrationEnabled()))
            {
                swrmap = &(m_measurements[index].swrGraphCalib);
            }else
            {
                swrmap = &(m_measurements[index].swrGraph);
            }
            // No .keys() on QCPGraphDataContainer (2026-08-25
            // QCustomPlot 2.x port) -- swrmap->at(n)->key replaces
            // the old swrkeys snapshot's at(n) directly below.

            double frequency = 0;
            double swr = 0;
            double phase = 0;
            double rho = 0;
            double rl = 0;
            double l = 0;
            double c = 0;
            double lpar = 0;
            double cpar = 0;
            double r = 0;
            double z = 0;
            double x = 0;
            double rpar = 0;
            double xpar = 0;
            QString zString;
            QString zparString;
            bool res = false;
            int start = m_previousI-DELTA;
            if(start < 0)
            {
                start = 0;
            }
            int stop = m_previousI+DELTA;
            if (stop > swrmap->size()-1)
            {
                stop = swrmap->size()-1;
            }
            for(int t = 0; t < 2; ++t)
            {
                if(t == 1)
                {
                    if(res)
                    {
                        break;
                    }
                    start = 0;
                    stop = swrmap->size()-1;
                }
                for(int i = start; i < stop; ++i)
                {
                    if((swrmap->at(i)->key <= xPos) && (swrmap->at(i+1)->key >= xPos))
                    {
                        double center = (swrmap->at(i)->key + swrmap->at(i+1)->key)/2;
                        if( xPos > center )
                        {
                            if(m_previousI == i+1)
                            {
                                return;
                            }
                            frequency = swrmap->at(i+1)->key;
                            m_previousI = i+1;
                        }else
                        {
                            if(m_previousI == i)
                            {
                                return;
                            }
                            frequency = swrmap->at(i)->key;
                            m_previousI = i;
                        }

                        int dataSize = m_measurements.at(index).dataRX.size();

                        if(m_calibration->getCalibrationEnabled())
                        {
                            if(m_farEndMeasurement == 1)
                            {
                                swr = graphValueAt(m_farEndMeasurementsSub.at(index).swrGraphCalib, frequency);
                                rl = graphValueAt(m_farEndMeasurementsSub.at(index).rlGraphCalib, frequency);
                                rho = graphValueAt(m_farEndMeasurementsSub.at(index).rhoGraphCalib, frequency);
                                phase = graphValueAt(m_farEndMeasurementsSub.at(index).phaseGraphCalib, frequency);
                                r = graphValueAt(m_farEndMeasurementsSub.at(index).rsrGraphCalib, frequency);
                                x = graphValueAt(m_farEndMeasurementsSub.at(index).rsxGraphCalib, frequency);
                                z = graphValueAt(m_farEndMeasurementsSub.at(index).rszGraphCalib, frequency);
                                rpar = graphValueAt(m_farEndMeasurementsSub.at(index).rprGraphCalib, frequency);
                                xpar = graphValueAt(m_farEndMeasurementsSub.at(index).rpxGraphCalib, frequency);
                            }else if(m_farEndMeasurement == 2)
                            {
                                swr = graphValueAt(m_farEndMeasurementsAdd.at(index).swrGraphCalib, frequency);
                                rl = graphValueAt(m_farEndMeasurementsAdd.at(index).rlGraphCalib, frequency);
                                rho = graphValueAt(m_farEndMeasurementsAdd.at(index).rhoGraphCalib, frequency);
                                phase = graphValueAt(m_farEndMeasurementsAdd.at(index).phaseGraphCalib, frequency);
                                r = graphValueAt(m_farEndMeasurementsAdd.at(index).rsrGraphCalib, frequency);
                                x = graphValueAt(m_farEndMeasurementsAdd.at(index).rsxGraphCalib, frequency);
                                z = graphValueAt(m_farEndMeasurementsAdd.at(index).rszGraphCalib, frequency);
                                rpar = graphValueAt(m_farEndMeasurementsAdd.at(index).rprGraphCalib, frequency);
                                xpar = graphValueAt(m_farEndMeasurementsAdd.at(index).rpxGraphCalib, frequency);
                            }else
                            {
                                swr = graphValueAt(m_measurements.at(index).swrGraphCalib, frequency);
                                rl = graphValueAt(m_measurements.at(index).rlGraphCalib, frequency);
                                rho = graphValueAt(m_measurements.at(index).rhoGraphCalib, frequency);
                                phase = graphValueAt(m_measurements.at(index).phaseGraphCalib, frequency);
                                if (m_previousI < 0 || m_previousI >=dataSize)
                                    return;
                                r = m_measurements.at(index).dataRXCalib.at(m_previousI).r;
                                if (m_previousI < 0 || m_previousI >=dataSize)
                                    return;
                                x = m_measurements.at(index).dataRXCalib.at(m_previousI).x;
                                z = graphValueAt(m_measurements.at(index).rszGraphCalib, frequency);
                                rpar = graphValueAt(m_measurements.at(index).rprGraphCalib, frequency);
                                xpar = graphValueAt(m_measurements.at(index).rpxGraphCalib, frequency);
                            }
                        }else
                        {
                            if(m_farEndMeasurement == 1)
                            {
                                swr = graphValueAt(m_farEndMeasurementsSub.at(index).swrGraph, frequency);
                                rl = graphValueAt(m_farEndMeasurementsSub.at(index).rlGraph, frequency);
                                rho = graphValueAt(m_farEndMeasurementsSub.at(index).rhoGraph, frequency);
                                phase = graphValueAt(m_farEndMeasurementsSub.at(index).phaseGraph, frequency);
                                r = graphValueAt(m_farEndMeasurementsSub.at(index).rsrGraph, frequency);
                                x = graphValueAt(m_farEndMeasurementsSub.at(index).rsxGraph, frequency);
                                z = graphValueAt(m_farEndMeasurementsSub.at(index).rszGraph, frequency);
                                rpar = graphValueAt(m_farEndMeasurementsSub.at(index).rprGraph, frequency);
                                xpar = graphValueAt(m_farEndMeasurementsSub.at(index).rpxGraph, frequency);
                            }else if(m_farEndMeasurement == 2)
                            {
                                swr = graphValueAt(m_farEndMeasurementsAdd.at(index).swrGraph, frequency);
                                rl = graphValueAt(m_farEndMeasurementsAdd.at(index).rlGraph, frequency);
                                rho = graphValueAt(m_farEndMeasurementsAdd.at(index).rhoGraph, frequency);
                                phase = graphValueAt(m_farEndMeasurementsAdd.at(index).phaseGraph, frequency);
                                r = graphValueAt(m_farEndMeasurementsAdd.at(index).rsrGraph, frequency);
                                x = graphValueAt(m_farEndMeasurementsAdd.at(index).rsxGraph, frequency);
                                z = graphValueAt(m_farEndMeasurementsAdd.at(index).rszGraph, frequency);
                                rpar = graphValueAt(m_farEndMeasurementsAdd.at(index).rprGraph, frequency);
                                xpar = graphValueAt(m_farEndMeasurementsAdd.at(index).rpxGraph, frequency);
                            }else
                            {
                                swr = graphValueAt(m_measurements.at(index).swrGraph, frequency);
                                rl = graphValueAt(m_measurements.at(index).rlGraph, frequency);
                                rho = graphValueAt(m_measurements.at(index).rhoGraph, frequency);
                                phase = graphValueAt(m_measurements.at(index).phaseGraph, frequency);
                                if (m_previousI < 0 || m_previousI >=dataSize)
                                    return;
                                r = m_measurements.at(index).dataRX.at(m_previousI).r;
                                if (m_previousI < 0 || m_previousI >=dataSize)
                                    return;
                                x = m_measurements.at(index).dataRX.at(m_previousI).x;
                                z = graphValueAt(m_measurements.at(index).rszGraph, frequency);
                                rpar = graphValueAt(m_measurements.at(index).rprGraph, frequency);
                                xpar = graphValueAt(m_measurements.at(index).rpxGraph, frequency);
                            }
                        }

                        const double maxRp = VALUE_LIMIT;
                        rpar = regulate(rpar, maxRp);
                        zString+= QString::number(r,'f', 2);
                        if(x >= 0)
                        {
                            if (x > maxRp)
                                x = maxRp; // HUCK
                            zString+= " + j";
                            zString+= QString::number(x,'f', 2);
                        }else
                        {
                            if (x < -maxRp)
                                x = -maxRp; // HUCK
                            zString+= " - j";
                            zString+= QString::number((x * (-1)),'f', 2);
                        }
                        zparString+= QString::number(rpar,'f', 2);
                        if(xpar >= 0)
                        {
                            if (xpar > maxRp)
                                xpar = maxRp; // HUCK
                            zparString+= " + j";
                            zparString+= QString::number(xpar,'f', 2);
                        }else
                        {
                            if (xpar < -maxRp)
                                xpar = -maxRp; // HUCK
                            zparString+= " - j";
                            zparString+= QString::number((xpar * (-1)),'f', 2);
                        }

                        l = 1E9 * x / (2*M_PI * frequency * 1E3);//nH
                        c = 1E12 / (2*M_PI * frequency * (x * (-1)) * 1E3);//pF

                        lpar = 1E9 * xpar / (2*M_PI * frequency * 1E3);
                        cpar = 1E12 / (2*M_PI * frequency * (xpar * (-1)) * 1E3);

                        res = true;
                        if(m_graphBriefHint != NULL)
                        {
                            QString str;
                            str = QString::number(frequency, 'f', 2);
                            int idx = str.indexOf(".");
                            if(idx < 0)
                            {
                                if(str.length() > 6)
                                {
                                    str.insert(str.length()-6," ");
                                }
                                if(str.length() > 3)
                                {
                                    str.insert(str.length()-3," ");
                                }
                            }else
                            {
                                int len = str.length() - idx;
                                if((str.length()-len) > 6)
                                {
                                    str.insert(str.length()-len-6," ");
                                }
                                if((str.length()-len) > 3)
                                {
                                    str.insert(str.length()-len-3," ");
                                }
                            }
                            str += " kHz\n";
                            if(m_currentTab == "tab_swr")
                            {
                                str += QString::number(swr,'f',2);
                            }else if(m_currentTab == "tab_phase")
                            {
                                str += QString::number(phase,'f',2) + "°";
                            }else if(m_currentTab == "tab_rs")
                            {
                                //str += QString::number(computeZ(r,x),'f',2);
                            }else if(m_currentTab == "tab_rp")
                            {
                                //str += QString::number(computeZ(r,x),'f',2);
                            }else if(m_currentTab == "tab_rl")
                            {
                                str += QString::number(rl,'f',2) + " dB";
                            }
                            m_graphBriefHint->setPopupText(str);
                        }
                        break;
                    }
                }
            }
            // See the identical check in the tab_tdr branch above.
            if (!res) {
                hideGraphCursor();
                return;
            }
            if(m_currentTab == "tab_swr")
            {
                if(!m_swrLine)
                {
                    m_swrLine = new QCPItemStraightLine(m_swrWidget);
                    m_swrLine->setAntialiased(false);
                }
                if(!m_swrLine2)
                {
                    m_swrLine2 = new QCPItemStraightLine(m_swrWidget);
                    m_swrLine2->setAntialiased(false);
                }
                QPen crosshairPen(inverseChartBackground());
                m_swrLine->setPen(crosshairPen);
                m_swrLine2->setPen(crosshairPen);
                m_swrLine->setVisible(true);
                m_swrLine2->setVisible(true);
                m_swrLine->point1->setCoords(frequency, MIN_SWR);
                m_swrLine->point2->setCoords(frequency, MAX_SWR);

                m_swrLine2->point1->setCoords(m_swrWidget->yAxis->range().lower, swr);
                m_swrLine2->point2->setCoords(m_swrWidget->yAxis->range().upper, swr);
            }else if(m_currentTab == "tab_phase")
            {
                if(!m_phaseLine)
                {
                    m_phaseLine = new QCPItemStraightLine(m_phaseWidget);
                    m_phaseLine->setAntialiased(false);
                }
                if(!m_phaseLine2)
                {
                    m_phaseLine2 = new QCPItemStraightLine(m_phaseWidget);
                    m_phaseLine2->setAntialiased(false);
                }
                QPen crosshairPen(inverseChartBackground());
                m_phaseLine->setPen(crosshairPen);
                m_phaseLine2->setPen(crosshairPen);
                m_phaseLine->setVisible(true);
                m_phaseLine2->setVisible(true);
                m_phaseLine->point1->setCoords(frequency, -2000);
                m_phaseLine->point2->setCoords(frequency, 2000);
                m_phaseLine2->point1->setCoords(m_phaseWidget->yAxis->range().lower, phase);
                m_phaseLine2->point2->setCoords(m_phaseWidget->yAxis->range().upper, phase);
            }else if(m_currentTab == "tab_rs")
            {
                if(!m_rsLine)
                {
                    m_rsLine = new QCPItemStraightLine(m_rsWidget);
                    m_rsLine->setAntialiased(false);
                }
                m_rsLine->setPen(QPen(inverseChartBackground()));
                m_rsLine->setVisible(true);
                m_rsLine->point1->setCoords(frequency, -2000);
                m_rsLine->point2->setCoords(frequency, 2000);
            }else if(m_currentTab == "tab_rp")
            {
                if(!m_rpLine)
                {
                    m_rpLine = new QCPItemStraightLine(m_rpWidget);
                    m_rpLine->setAntialiased(false);
                }
                m_rpLine->setPen(QPen(inverseChartBackground()));
                m_rpLine->setVisible(true);
                m_rpLine->point1->setCoords(frequency, -2000);
                m_rpLine->point2->setCoords(frequency, 2000);
            }else if(m_currentTab == "tab_rl")
            {
                if(!m_rlLine)
                {
                    m_rlLine = new QCPItemStraightLine(m_rlWidget);
                    m_rlLine->setAntialiased(false);
                }
                if(!m_rlLine2)
                {
                    m_rlLine2 = new QCPItemStraightLine(m_rlWidget);
                    m_rlLine2->setAntialiased(false);
                }
                QPen crosshairPen(inverseChartBackground());
                m_rlLine->setPen(crosshairPen);
                m_rlLine2->setPen(crosshairPen);
                m_rlLine->setVisible(true);
                m_rlLine2->setVisible(true);
                m_rlLine->point1->setCoords(frequency, -2000);
                m_rlLine->point2->setCoords(frequency, 2000);
                m_rlLine2->point1->setCoords(m_rlWidget->yAxis->range().lower, rl);
                m_rlLine2->point2->setCoords(m_rlWidget->yAxis->range().upper, rl);
            }
            // tab_s21 had a dead copy of this here -- unreachable, since
            // the dedicated "else if(m_currentTab == "tab_s21")" branch
            // above (this file, ~line 562) always returns before
            // execution ever reaches this shared block. That branch now
            // draws its own crosshair directly using real S21 data
            // instead of the borrowed-and-wrong "rl" this used.

            QString str = QString::number(frequency, 'f', 2);
            int idx = str.indexOf(".");
            if(idx < 0)
            {
                if(str.length() > 6)
                {
                    str.insert(str.length()-6," ");
                }
                if(str.length() > 3)
                {
                    str.insert(str.length()-3," ");
                }
            }else
            {
                int len = str.length() - idx;
                if((str.length()-len) > 6)
                {
                    str.insert(str.length()-len-6," ");
                }
                if((str.length()-len) > 3)
                {
                    str.insert(str.length()-len-3," ");
                }
            }

            QList<QPair<QString, QString>> fields;
            fields << qMakePair(tr("Frequency"), str + " kHz")
                   << qMakePair(tr("SWR"), QString::number(swr,'f', 2))
                   << qMakePair(tr("RL"), QString::number(rl,'f', 2) + " dB")
                   << qMakePair(tr("Z"), zString + " Ohm")
                   << qMakePair(tr("|Z|"), QString::number(z,'f', 2) + " Ohm")
                   << qMakePair(tr("|rho|"), QString::number(rho,'f', 2))
                   << qMakePair(tr("Phase"), QString::number(phase,'f', 2) + " °");
            if (x > 0) {
                fields << qMakePair(tr("L"), QString::number(l,'f', 2) + " nH");
            } else {
                fields << qMakePair(tr("C"), QString::number(c,'f', 2) + " pF");
            }
            fields << qMakePair(tr("Zpar"), zparString + " Ohm");
            if (x > 0) {
                fields << qMakePair(tr("Lpar"), QString::number(lpar,'f', 2) + " nH");
            } else {
                fields << qMakePair(tr("Cpar"), QString::number(cpar,'f', 2) + " pF");
            }

            if(!m_farEndMeasurement)
            {
                QString lenUnits;
                if(m_measureSystemMetric)
                {
                    lenUnits = tr("m");
                }else
                {
                    lenUnits = tr("ft");
                }

                double len14 = SPEEDOFLIGHT/4/frequency/1000*m_cableVelFactor;
                double len12 = SPEEDOFLIGHT/2/frequency/1000*m_cableVelFactor;

                if(!m_measureSystemMetric)
                {
                    len14 *= FEETINMETER;
                    len12 *= FEETINMETER;
                }

                fields << qMakePair(tr("Cable"), tr("length(1/4) = %1 %2, length(1/2) = %3 %4")
                        .arg(QString::number(len14,'f',2))
                        .arg(lenUnits)
                        .arg(QString::number(len12,'f',2))
                        .arg(lenUnits));
            }
            setGraphHintFields(fields);
        }
    }
    replot();
}

void Measurements::hideGraphCursor()
{
    // Counterpart is in updatePopUp(): each per-tab branch below sets
    // setVisible(true) back on its own line(s) right before repositioning
    // them, since setPen()/setCoords() alone don't undo a prior hide.
    if (m_swrLine)   m_swrLine->setVisible(false);
    if (m_swrLine2)  m_swrLine2->setVisible(false);
    if (m_phaseLine) m_phaseLine->setVisible(false);
    if (m_phaseLine2) m_phaseLine2->setVisible(false);
    if (m_rsLine)    m_rsLine->setVisible(false);
    if (m_rpLine)    m_rpLine->setVisible(false);
    if (m_rlLine)    m_rlLine->setVisible(false);
    if (m_rlLine2)   m_rlLine2->setVisible(false);
    if (m_s21Line)   m_s21Line->setVisible(false);
    if (m_s21Line2)  m_s21Line2->setVisible(false);
    if (m_tdrLine)   m_tdrLine->setVisible(false);

    if (m_graphBriefHint) m_graphBriefHint->focusHide();
    // m_graphHintBox (the full "Frequency = .../SWR = ..." panel, as opposed
    // to the small cursor-following m_graphBriefHint) deliberately does NOT
    // hide here anymore. Cursor movement fires many times a second, and
    // near the edge of the scanned data it flickers in/out of range on
    // consecutive events -- hiding/reshowing it every time made it visibly
    // blink. It's meant to stay up as a steady readout whenever its
    // checkbox is enabled; showHideHints() already hides it on focus loss
    // or the checkbox being unchecked, which is the only hiding it needs.

    // updatePopUp() dedups against m_previousI: if the cursor re-enters at
    // the same data index it was at when we hid everything, it early-returns
    // before ever showing the crosshairs/popups again. Reset it so the next
    // updatePopUp() call always does real work.
    m_previousI = -1;

    replot();
}

void Measurements::on_mainWindowPos(int x, int y)
{
    // m_graphHintBox used to need repositioning here too, back when it was
    // a floating PopUp tracking the main window's own moves (see PopUp::
    // MainWindowPos()) -- now it's a plain child widget in mainwindow.ui's
    // layout, which the layout engine keeps positioned for free.
    if (m_oneFqWidget)
        m_oneFqWidget->MainWindowPos(x, y);
}

bool Measurements::getGraphHintEnabled(void)
{
    return m_graphHintEnabled;
}

bool Measurements::getGraphBriefHintEnabled(void)
{
    return m_graphBriefHintEnabled;
}

QColor Measurements::chartBackgroundColor()
{
    return Style::theme().chartBackground;
}

// Shared by setBriefHintColor() and the crosshair lines (updatePopUp()) --
// both need a color that reads against the plot's own chart-background,
// which is independent of the app's Light/Dark theme.
QColor Measurements::inverseChartBackground()
{
    QColor color = chartBackgroundColor();
#ifndef Q_OS_MACX
    return QColor(255-color.red(), 255-color.green(), 255-color.blue());
#else
    return color;
#endif
}

// Used to also back hintBackgroundColor() (m_graphHintBox's tinted
// background) -- removed once m_graphHintBox became a plain docked
// QGroupBox/QLabel (see setGraphHintWidgets()): it's themed by the app's
// normal Light/Dark QSS cascade like every other left-column widget now,
// same as chart-background-independent widgets always have been, so there's
// no per-instance color to compute here any more. m_graphBriefHint stays a
// floating overlay on the plot itself, so it still needs a color that reads
// against the plot's own (independently configurable) chart-background.
void Measurements::setBriefHintColor()
{
    if (m_graphBriefHint != nullptr) {
        m_graphBriefHint->setTextColor(inverseChartBackground().name());
    }
}

