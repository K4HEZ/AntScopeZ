#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"

extern QMap<QString, QString> g_mapTabPlotNames;
extern int g_maxMeasurements; // defined in measurements.cpp
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original measurements.cpp (still in
// measurements.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of Measurements.

void Measurements::on_redrawGraphs(bool _incrementally)
{
    if(m_calibration == NULL)
    {
        return;
    }
    if(m_measurements.length() == 0)
    {
        replot();
        return;
    }

    if(m_farEndMeasurement)
    {
        calcFarEnd(_incrementally);
    }

    if( m_currentTab == "tab_swr")//SWR
    {
        redrawSWR(_incrementally);
    } else if( m_currentTab == "tab_phase")//Phase
    {
        redrawPhase(_incrementally);
    } else if( m_currentTab == "tab_rs")//Rs
    {
        redrawRs(_incrementally);
    } else if( m_currentTab == "tab_rp")//Rp
    {
        redrawRp(_incrementally);
    } else if( m_currentTab == "tab_rl")//Rl
    {
        redrawRl(_incrementally);
    } else if( m_currentTab == "tab_s21")//S21
    {
        redrawS21(_incrementally);
    } else if( m_currentTab == "tab_tdr")//TDR
    {
        redrawTDR();
    } else if( m_currentTab == "tab_smith")//Smith
    {
        redrawSmith(_incrementally);
    } else if( m_currentTab == "tab_user")//User
    {
        redrawUser(_incrementally);
    } else if( m_currentTab == "tab_multi")
    {
        redrawMultiGraph(_incrementally);
    }
}


void Measurements::restrictData(qreal _min, qreal _max, QCPGraphData& _data)
{
    _data.value = (_data.value > _max) ? _max : ((_data.value < _min) ? _min : _data.value);
}

void Measurements::replot()
{
    // Used to special-case a single (non-continuous) scan in progress to call
    // on_drawPoint() (CustomPlot::drawIncrementally()'s XOR-buffer trick) and
    // return here, skipping the real per-tab replot() below entirely. That
    // path only ever moves a small frequency-cursor tick (graph(0), fed by
    // setData() calls like m_swrWidget->graph(0)->setData(x,y) in
    // prepareGraphs()) -- it never draws the actual measurement trace, which
    // lives in graph(i+1) and is only rendered by the real replot() below.
    // So a single scan's line never appeared until the scan finished, while a
    // continuous scan (which always fell through to here) drew progressively
    // the whole time -- not a difference Harold expects or wants. Removed the
    // special case so single scans redraw for real on every point too, same
    // as continuous. See todo #25.
    if(m_currentTab == "tab_swr")
    {
        // Used to replot twice here -- once normally, then read back the
        // auto-computed Y tick labels, blank out any below 1 (not a
        // physically meaningful SWR value) via setAutoTickLabels(false)/
        // setTickVectorLabels(), and replot again. Both those setters are
        // 1.x-only (removed by 2.0's QCPAxisTicker refactor); replaced by
        // SwrAxisTicker (CustomPlot.h, assigned to this axis in
        // MainWindow::setWidgetsSettings()), which blanks the same labels
        // as part of the ticker's own single pass -- see its comment.
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
    }else if(m_currentTab == "tab_user")
    {
        m_userWidget->replot();
    }
#ifndef NO_MULTITAB
    else if(m_currentTab == "tab_multi")
    {
        QString old_m_currentTab = m_currentTab;
        const QList<QString>& tabs = MainWindow::m_mainWindow->multiTabs();
        foreach (const QString& tab, tabs) {
            // plotForTab() returns nullptr for a tab name it doesn't recognize
            // (e.g. a stale/self-referential "tab_multi" entry that slipped into
            // the persisted multi-tab list -- see restoreMultitab()). Guard here
            // the same way the Printmulti path already does at mainwindow.cpp's
            // MainWindow::print() tab_multi branch, instead of crashing.
            QCustomPlot* plot = MainWindow::m_mainWindow->plotForTab(tab);
            if (plot != nullptr) {
                plot->replot();
            }
        }
        m_currentTab = old_m_currentTab;
    }
#endif

    MainWindow* mainWindow = qobject_cast<MainWindow*>(parent());
    QTabWidget* tabWidget = mainWindow->tabWidget();
    QWidget* tab = tabWidget->currentWidget();
    tab->repaint();
}

void Measurements::NormRXtoSmithPoint(double Rnorm, double Xnorm, double &x, double &y)
{
    // Unlike the calibrated path a little further down in measurements.cpp
    // (which explicitly guards qIsNaN(calR)/qIsNaN(calX) before using them),
    // this shared helper never validated its inputs. Defensive hardening,
    // not itself the confirmed crash site (2026-08-20 core dump traced that
    // to a separate pixelToCoord() misuse in measurements_onefq.cpp -- see
    // its own comment) -- but a NaN or Inf Rnorm/Xnorm (device data gone
    // bad upstream) or the Rnorm==-1 && Xnorm==0 degenerate case (Denom==0)
    // would propagate the same way into QCustomPlot's
    // coordToPixel/pixelToCoord, which Q_ASSERTs on NaN in a debug build.
    if (qIsNaN(Rnorm) || qIsNaN(Xnorm) || qIsInf(Rnorm) || qIsInf(Xnorm)) {
        x = 0;
        y = 0;
        return;
    }
    double Denom = (Rnorm+1)*(Rnorm+1)+Xnorm*Xnorm;
    if (qFuzzyIsNull(Denom)) {
        x = -6; // Rnorm==-1, Xnorm==0 -- the reflection-coefficient edge closest to this input
        y = 0;
        return;
    }
    double RhoReal = ((Rnorm-1)*(Rnorm+1)+Xnorm*Xnorm)/Denom;
    double RhoImag = 2*Xnorm/Denom;

    x = RhoReal*6;// 6 - radius
    y = RhoImag*6;// 6 - radius
}

void Measurements::drawSmithImage (void)
{
    QPen pen;
    pen.setColor(Qt::black);
#define ROUND_DOTS_NUM 360
    QCPCurve *round1 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round7 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round2 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round3 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round4 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round5 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round6 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);

    QCPCurveDataContainer map1;
    QCPCurveDataContainer map2;
    QCPCurveDataContainer map3;
    QCPCurveDataContainer map4;
    QCPCurveDataContainer map5;
    QCPCurveDataContainer map6;
    QCPCurveDataContainer map7;
    for(double i = 0; i < ROUND_DOTS_NUM; ++i)
    {
        map1.add(QCPCurveData(i, (6 * qCos(i/57.02)), (6 * qSin(i/57.02))));
        map2.add(QCPCurveData(i, (1 + 5 * qCos(i/57.02)), (5 * qSin(i/57.02))));
        map3.add(QCPCurveData(i, (2 + 4 * qCos(i/57.02)), (4 * qSin(i/57.02))));
        map4.add(QCPCurveData(i, (3 + 3 * qCos(i/57.02)), (3 * qSin(i/57.02))));
        map5.add(QCPCurveData(i, (4 + 2 * qCos(i/57.02)), (2 * qSin(i/57.02))));
        map6.add(QCPCurveData(i, (5 + 1 * qCos(i/57.02)), (1 * qSin(i/57.02))));
        map7.add(QCPCurveData(i, (2 * qCos(i/57.02)), (2 * qSin(i/57.02))));
    }
    round1->setData(QSharedPointer<QCPCurveDataContainer>::create(map1));
    round1->setBrush(QBrush(QColor(0, 0, 255, 20)));
    round7->setData(QSharedPointer<QCPCurveDataContainer>::create(map7));
    round7->setBrush(QBrush(QColor(255, 255, 255, 255)));
    m_smithInnerCircle = round7;
    round2->setData(QSharedPointer<QCPCurveDataContainer>::create(map2));
    round3->setData(QSharedPointer<QCPCurveDataContainer>::create(map3));
    round4->setData(QSharedPointer<QCPCurveDataContainer>::create(map4));
    round5->setData(QSharedPointer<QCPCurveDataContainer>::create(map5));
    round6->setData(QSharedPointer<QCPCurveDataContainer>::create(map6));


    QCPCurve *round8 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round9 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurveDataContainer map8;
    QCPCurveDataContainer map9;
    for(double i = 0; i < 90; ++i)//1 line
    {
        map8.add(QCPCurveData(i, (6 + 6 * qCos((i+179.15)/57.02)), (6 + 6 * qSin((i+179.15)/57.02))));
        map9.add(QCPCurveData(i, (6 + 6 * qCos((i+179.15)/57.02)), (-1)*(6 + 6 * qSin((i+179.15)/57.02))));
    }
    round8->setData(QSharedPointer<QCPCurveDataContainer>::create(map8));
    round9->setData(QSharedPointer<QCPCurveDataContainer>::create(map9));

    QCPCurve *round10 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round11 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurveDataContainer map10;
    QCPCurveDataContainer map11;
    for(double i = 0; i < 53; ++i)//0.5 line
    {
        map10.add(QCPCurveData(i, (6 + 12 * qCos((i+215.85)/57.02)), (12 + 12 * qSin((i+215.85)/57.02))));
        map11.add(QCPCurveData(i, (6 + 12 * qCos((i+215.85)/57.02)), (-1)*(12 + 12 * qSin((i+215.85)/57.02))));
    }
    round10->setData(QSharedPointer<QCPCurveDataContainer>::create(map10));
    round11->setData(QSharedPointer<QCPCurveDataContainer>::create(map11));

    QCPCurve *round12 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round13 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurveDataContainer map12;
    QCPCurveDataContainer map13;
    for(double i = 0; i < 127; ++i)//2 line
    {
        map12.add(QCPCurveData(i, (6 + 3 * qCos((i+142.45)/57.02)), (3 + 3 * qSin((i+142.45)/57.02))));
        map13.add(QCPCurveData(i, (6 + 3 * qCos((i+142.45)/57.02)), (-1)*(3 + 3 * qSin((i+142.45)/57.02))));
    }
    round12->setData(QSharedPointer<QCPCurveDataContainer>::create(map12));
    round13->setData(QSharedPointer<QCPCurveDataContainer>::create(map13));

    QCPCurve *round14 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round15 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurveDataContainer map14;
    QCPCurveDataContainer map15;
    for(double i = 0; i < 151; ++i)// 5 line
    {
        map14.add(QCPCurveData(i, (6 + 1.2 * qCos((i+112)/57.02)), (1.2 + 1.2 * qSin((i+112)/57.02))));//117.5
        map15.add(QCPCurveData(i, (6 + 1.2 * qCos((i+112)/57.02)), (-1)*(1.2 + 1.2 * qSin((i+112)/57.02))));
    }
    round14->setData(QSharedPointer<QCPCurveDataContainer>::create(map14));
    round15->setData(QSharedPointer<QCPCurveDataContainer>::create(map15));

    QCPCurve *round16 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurve *round17 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurveDataContainer map16;
    QCPCurveDataContainer map17;
    for(double i = 0; i < 23; ++i)//0.2 line
    {
        map16.add(QCPCurveData(i, (6 + 30 * qCos((i+246.19)/57.02)), (30 + 30 * qSin((i+246.19)/57.02))));
        map17.add(QCPCurveData(i, (6 + 30 * qCos((i+246.19)/57.02)), (-1)*(30 + 30 * qSin((i+246.19)/57.02))));
    }
    round16->setData(QSharedPointer<QCPCurveDataContainer>::create(map16));
    round17->setData(QSharedPointer<QCPCurveDataContainer>::create(map17));


    // 0 line
    QCPCurve *round18 = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    QCPCurveDataContainer map18;
    map18.add(QCPCurveData(0, -6, 0));
    map18.add(QCPCurveData(1, 6, 0));
    round18->setData(QSharedPointer<QCPCurveDataContainer>::create(map18));


    round1->setPen(pen);
    round2->setPen(pen);
    round3->setPen(pen);
    round4->setPen(pen);
    round5->setPen(pen);
    round6->setPen(pen);
    round7->setPen(pen);
    round8->setPen(pen);
    round9->setPen(pen);
    round10->setPen(pen);
    round11->setPen(pen);
    round12->setPen(pen);
    round13->setPen(pen);
    round14->setPen(pen);
    round15->setPen(pen);
    round16->setPen(pen);
    round17->setPen(pen);
    round18->setPen(pen);

    m_smithArcs = {round1, round2, round3, round4, round5, round6, round7,
                   round8, round9, round10, round11, round12, round13,
                   round14, round15, round16, round17, round18};

    QFont serifFont("Times", 12, QFont::Bold);
    QCPItemText *center5 = new QCPItemText(m_smithWidget);
    QCPItemText *center2 = new QCPItemText(m_smithWidget);
    QCPItemText *center1 = new QCPItemText(m_smithWidget);
    QCPItemText *center05 = new QCPItemText(m_smithWidget);
    QCPItemText *center02 = new QCPItemText(m_smithWidget);
    QCPItemText *center0 = new QCPItemText(m_smithWidget);

    QCPItemText *up5 = new QCPItemText(m_smithWidget);
    QCPItemText *up2 = new QCPItemText(m_smithWidget);
    QCPItemText *up1 = new QCPItemText(m_smithWidget);
    QCPItemText *up05 = new QCPItemText(m_smithWidget);
    QCPItemText *up02 = new QCPItemText(m_smithWidget);

    QCPItemText *down5 = new QCPItemText(m_smithWidget);
    QCPItemText *down2 = new QCPItemText(m_smithWidget);
    QCPItemText *down1 = new QCPItemText(m_smithWidget);
    QCPItemText *down05 = new QCPItemText(m_smithWidget);
    QCPItemText *down02 = new QCPItemText(m_smithWidget);

    center5->position->setCoords(4.2, -0.3);
    center5->setText("5");
    center5->setFont(serifFont);
    center5->setColor(QColor(0, 0, 0, 150));

    center2->position->setCoords(2.2, -0.3);
    center2->setText("2");
    center2->setFont(serifFont);
    center2->setColor(QColor(0, 0, 0, 150));

    center1->position->setCoords(0.2, -0.3);
    center1->setText("1");
    center1->setFont(serifFont);
    center1->setColor(QColor(0, 0, 0, 150));

    center05->position->setCoords(-2.3, -0.3);
    center05->setText("0.5");
    center05->setFont(serifFont);
    center05->setColor(QColor(0, 0, 0, 150));

    center02->position->setCoords(-4.3, -0.3);
    center02->setText("0.2");
    center02->setFont(serifFont);
    center02->setColor(QColor(0, 0, 0, 150));

    center0->position->setCoords(-6.5, 0);
    center0->setText("0");
    center0->setFont(serifFont);
    center0->setColor(QColor(0, 0, 0, 150));

    up5->position->setCoords(6, 2.5);
    up5->setText("5");
    up5->setFont(serifFont);
    up5->setColor(QColor(0, 0, 0, 150));

    up2->position->setCoords(3.8, 5.4);
    up2->setText("2");
    up2->setFont(serifFont);
    up2->setColor(QColor(0, 0, 0, 150));

    up1->position->setCoords(0, 6.5);
    up1->setText("1");
    up1->setFont(serifFont);
    up1->setColor(QColor(0, 0, 0, 150));

    up05->position->setCoords(-4, 5.4);
    up05->setText("0.5");
    up05->setFont(serifFont);
    up05->setColor(QColor(0, 0, 0, 150));

    up02->position->setCoords(-6.5, 2.5);
    up02->setText("0.2");
    up02->setFont(serifFont);
    up02->setColor(QColor(0, 0, 0, 150));

    down5->position->setCoords(6, -2.5);
    down5->setText("-5");
    down5->setFont(serifFont);
    down5->setColor(QColor(0, 0, 0, 150));

    down2->position->setCoords(3.8, -5.4);
    down2->setText("-2");
    down2->setFont(serifFont);
    down2->setColor(QColor(0, 0, 0, 150));

    down1->position->setCoords(0, -6.5);
    down1->setText("-1");
    down1->setFont(serifFont);
    down1->setColor(QColor(0, 0, 0, 150));

    down05->position->setCoords(-4, -5.4);
    down05->setText("-0.5");
    down05->setFont(serifFont);
    down05->setColor(QColor(0, 0, 0, 150));

    down02->position->setCoords(-6.5, -2.5);
    down02->setText("-0.2");
    down02->setFont(serifFont);
    down02->setColor(QColor(0, 0, 0, 150));

    m_smithLabels = {center5, center2, center1, center05, center02, center0,
                      up5, up2, up1, up05, up02,
                      down5, down2, down1, down05, down02};
}

void Measurements::setSmithBackgroundColor(QColor color)
{
    if (m_smithInnerCircle == nullptr || !color.isValid())
        return;
    m_smithInnerCircle->setBrush(QBrush(color));
}

// drawSmithImage() paints the resistance/reactance arcs and their number
// labels with hardcoded black (see the QPen/QColor(0, 0, 0, ...) literals
// there) since that was the only theme the chart ever had. Now that the
// background can be light or dark (see MainWindow::setChartBackground(),
// the only caller of this), those need to track the same foreground color
// the axes get, or they lose all contrast against a dark background.
//
// Recolored via m_smithArcs/m_smithLabels rather than
// m_smithWidget->plottableCount()/itemCount(): drawSmithImage() never
// registers these curves/items with QCustomPlot::addPlottable()/addItem(),
// so those lists are empty here -- QCPLayerable-derived objects add
// themselves to their layer for rendering independently of that
// bookkeeping, which is why they draw at all despite never being added.
void Measurements::setSmithForegroundColor(QColor color)
{
    if (!color.isValid())
        return;

    for (QCPCurve* curve : m_smithArcs) {
        QPen pen = curve->pen();
        pen.setColor(color);
        curve->setPen(pen);
    }

    QColor labelColor = color;
    labelColor.setAlpha(150); // matches drawSmithImage()'s original QColor(0, 0, 0, 150)
    for (QCPItemText* text : m_smithLabels)
        text->setColor(labelColor);
}

//Cable-------------------------------------------------------------------------
void Measurements::redrawSWR(bool _incrementally)
{
    if (m_measurements.isEmpty())
        return;
    int i = _incrementally ? (m_measurements.length()-1) : 0;
    if (m_calibration->getCalibrationEnabled())
    {
        if(m_farEndMeasurement != 0)
        {
            for(; i < m_measurements.length(); ++i)
            {
                m_swrWidget->graph(i+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_farEndMeasurement == 1 ? m_farEndMeasurementsSub[i].swrGraph : m_farEndMeasurementsAdd[i].swrGraph));
            }
        }else
        {
            for(; i < m_measurements.length(); ++i)
            {
                m_swrWidget->graph(i+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements[i].swrGraphCalib));
            }
        }
    } else {
        for(; i < m_measurements.length(); ++i)
        {
            if (!_incrementally)
                m_viewMeasurements[i].swrGraph.clear();

            QCPGraphDataContainer *map;
            switch (m_farEndMeasurement) {
            case 1:
                map = &m_farEndMeasurementsSub[i].swrGraph;
                break;
            case 2:
                map = &m_farEndMeasurementsAdd[i].swrGraph;
                break;
            default:
                map = &m_measurements[i].swrGraph;
                break;
            }
            // No .keys() on QCPGraphDataContainer (2026-08-25 QCustomPlot
            // 2.x port) -- map->at(n) replaces list.at(n)/map->value(...)
            // directly, same self-referencing walk as the Rs/Rp blocks
            // above.
            if (map->isEmpty())
                continue;
            QCPGraphData data;
            QCPGraphData viewData;
            double maxSwr = MAX_SWR;//m_swrWidget->yAxis->range().upper;
            int n = _incrementally ? (map->size()-1) : 0;
            for(; n < map->size(); ++n)
            {
                data.key = map->at(n)->key;
                viewData = *map->at(n);
                if( viewData.value > maxSwr || viewData.value < 1)
                {
                    data.value = maxSwr;
                }else
                {
                    data.value = viewData.value;
                }
                m_viewMeasurements[i].swrGraph.add(data);
            }
            if (!_incrementally) {
                m_swrWidget->graph(i+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_viewMeasurements[i].swrGraph));
            } else {
                m_swrWidget->graph(i+1)->addData(m_viewMeasurements[i].swrGraph.at(m_viewMeasurements[i].swrGraph.size()-1)->key, m_viewMeasurements[i].swrGraph.at(m_viewMeasurements[i].swrGraph.size()-1)->value);
            }
        }
    }
    replot();
}



void Measurements::redrawPhase(bool _incrementally)
{
    if (m_measurements.isEmpty())
        return;
    bool calibr = m_calibration->getCalibrationEnabled();
    int i = _incrementally ? (m_measurements.length()-1) : 0;

    if(m_farEndMeasurement != 0)
    {
        for(; i < m_measurements.length(); ++i)
        {
            m_phaseWidget->graph(i+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_farEndMeasurement == 1 ? m_farEndMeasurementsSub[i].phaseGraph : m_farEndMeasurementsAdd[i].phaseGraph));
        }
    }else
    {
        for(; i < m_measurements.length(); ++i)
        {
            m_phaseWidget->graph(i+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(calibr ? m_measurements[i].phaseGraphCalib : m_measurements[i].phaseGraph));
        }
    }
    replot();
}

void Measurements::redrawRs(bool _incrementally)
{
    if (m_measurements.isEmpty())
        return;
    bool calibr = m_calibration->getCalibrationEnabled();
    int i = _incrementally ? (m_measurements.length()-1) : 0;

    double maxVal = m_rsWidget->yAxis->range().upper;
    double minVal = m_rsWidget->yAxis->range().lower;

    QCPGraphDataContainer rMap;
    QCPGraphDataContainer xMap;
    QCPGraphDataContainer zMap;
    for (; i<m_measurements.length(); i++) {
        if (!_incrementally) {
            m_viewMeasurements[i].rsrGraph.clear();
            m_viewMeasurements[i].rsxGraph.clear();
            m_viewMeasurements[i].rszGraph.clear();
        }

        if(m_farEndMeasurement)
        {
            if(m_farEndMeasurement == 1)
            {
                rMap = m_farEndMeasurementsSub[i].rsrGraph;
                xMap = m_farEndMeasurementsSub[i].rsxGraph;
                zMap = m_farEndMeasurementsSub[i].rszGraph;
            }else if (m_farEndMeasurement == 2)
            {
                rMap = m_farEndMeasurementsAdd[i].rsrGraph;
                xMap = m_farEndMeasurementsAdd[i].rsxGraph;
                zMap = m_farEndMeasurementsAdd[i].rszGraph;
            }
        }else
        {
            rMap = calibr ? m_measurements[i].rsrGraphCalib : m_measurements[i].rsrGraph;
            xMap = calibr ? m_measurements[i].rsxGraphCalib : m_measurements[i].rsxGraph;
            zMap = calibr ? m_measurements[i].rszGraphCalib : m_measurements[i].rszGraph;
        }
        // QCPGraphDataContainer has no .keys() (2026-08-25 QCustomPlot 2.x
        // port). The original rKeys/xKeys/zKeys.at(n) calls were each
        // reading a map's *own* key at index n (rKeys = rMap.keys(), etc.)
        // -- i.e. already three self-referencing, same-index walks (R/X/Z
        // populated in lockstep, one point each per scan sample), not a
        // cross-container lookup by a shared key -- so .at(n) directly on
        // each container is the exact equivalent, not an approximation.
        if (rMap.isEmpty())
            continue;
        QCPGraphData data;
        int n = _incrementally ? (rMap.size()-1) : 0;
        for( ; n < rMap.size(); ++n)
        {
            data = *rMap.at(n);
            restrictData(minVal, maxVal, data);
            m_viewMeasurements[i].rsrGraph.add(data);

            data = *xMap.at(n);
            restrictData(minVal, maxVal, data);
            m_viewMeasurements[i].rsxGraph.add(data);

            data = *zMap.at(n);
            restrictData(minVal, maxVal, data);
            m_viewMeasurements[i].rszGraph.add(data);
        }
        if (!_incrementally) {
            m_rsWidget->graph(i*3+3)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_viewMeasurements[i].rszGraph));
            m_rsWidget->graph(i*3+2)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_viewMeasurements[i].rsxGraph));
            m_rsWidget->graph(i*3+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_viewMeasurements[i].rsrGraph));
        } else {
            m_rsWidget->graph(i*3+3)->addData(m_viewMeasurements[i].rszGraph.at(m_viewMeasurements[i].rszGraph.size()-1)->key, m_viewMeasurements[i].rszGraph.at(m_viewMeasurements[i].rszGraph.size()-1)->value);
            m_rsWidget->graph(i*3+2)->addData(m_viewMeasurements[i].rsxGraph.at(m_viewMeasurements[i].rsxGraph.size()-1)->key, m_viewMeasurements[i].rsxGraph.at(m_viewMeasurements[i].rsxGraph.size()-1)->value);
            m_rsWidget->graph(i*3+1)->addData(m_viewMeasurements[i].rsrGraph.at(m_viewMeasurements[i].rsrGraph.size()-1)->key, m_viewMeasurements[i].rsrGraph.at(m_viewMeasurements[i].rsrGraph.size()-1)->value);
        }
    }
    replot();
}

void Measurements::redrawRp(bool _incrementally)
{
    if (m_measurements.isEmpty())
        return;

    //qint64 t0 = QDateTime::currentMSecsSinceEpoch();

    bool calibr = m_calibration->getCalibrationEnabled();
    int i = _incrementally ? (m_measurements.length()-1) : 0;

    double maxVal = m_rpWidget->yAxis->range().upper;
    double minVal = m_rpWidget->yAxis->range().lower;

    QCPGraphDataContainer rMap;
    QCPGraphDataContainer xMap;
    QCPGraphDataContainer zMap;
    for (; i<m_measurements.length(); i++) {
        if (!_incrementally) {
            m_viewMeasurements[i].rprGraph.clear();
            m_viewMeasurements[i].rpxGraph.clear();
            m_viewMeasurements[i].rpzGraph.clear();
        }

        if(m_farEndMeasurement)
        {
            if(m_farEndMeasurement == 1)
            {
                rMap = m_farEndMeasurementsSub[i].rprGraph;
                xMap = m_farEndMeasurementsSub[i].rpxGraph;
                zMap = m_farEndMeasurementsSub[i].rpzGraph;
            }else if (m_farEndMeasurement == 2)
            {
                rMap = m_farEndMeasurementsAdd[i].rprGraph;
                xMap = m_farEndMeasurementsAdd[i].rpxGraph;
                zMap = m_farEndMeasurementsAdd[i].rpzGraph;
            }
        }else
        {
            rMap = calibr ? m_measurements[i].rprGraphCalib : m_measurements[i].rprGraph;
            xMap = calibr ? m_measurements[i].rpxGraphCalib : m_measurements[i].rpxGraph;
            zMap = calibr ? m_measurements[i].rpzGraphCalib : m_measurements[i].rpzGraph;
        }
        // See the matching Rs-widget block above for why .at(n) on each
        // container directly is the exact equivalent of the old
        // rKeys/xKeys/zKeys.at(n) walk, not an approximation.
        if (rMap.isEmpty())
            continue;
        QCPGraphData data;
        int n = _incrementally ? (rMap.size()-1) : 0;
        for( ; n < rMap.size(); ++n)
        {
            data = *rMap.at(n);
            restrictData(minVal, maxVal, data);
            m_viewMeasurements[i].rprGraph.add(data);

            data = *xMap.at(n);
            restrictData(minVal, maxVal, data);
            m_viewMeasurements[i].rpxGraph.add(data);

            data = *zMap.at(n);
            restrictData(minVal, maxVal, data);
            m_viewMeasurements[i].rpzGraph.add(data);
        }
        if (!_incrementally) {
            m_rpWidget->graph(i*3+3)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_viewMeasurements[i].rpzGraph));
            m_rpWidget->graph(i*3+2)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_viewMeasurements[i].rpxGraph));
            m_rpWidget->graph(i*3+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_viewMeasurements[i].rprGraph));
        } else {
            m_rpWidget->graph(i*3+3)->addData(m_viewMeasurements[i].rpzGraph.at(m_viewMeasurements[i].rpzGraph.size()-1)->key, m_viewMeasurements[i].rpzGraph.at(m_viewMeasurements[i].rpzGraph.size()-1)->value);
            m_rpWidget->graph(i*3+2)->addData(m_viewMeasurements[i].rpxGraph.at(m_viewMeasurements[i].rpxGraph.size()-1)->key, m_viewMeasurements[i].rpxGraph.at(m_viewMeasurements[i].rpxGraph.size()-1)->value);
            m_rpWidget->graph(i*3+1)->addData(m_viewMeasurements[i].rprGraph.at(m_viewMeasurements[i].rprGraph.size()-1)->key, m_viewMeasurements[i].rprGraph.at(m_viewMeasurements[i].rprGraph.size()-1)->value);
        }
    }
    replot();
}

void Measurements::redrawRl(bool _incrementally)
{
    if (m_measurements.isEmpty())
        return;
    bool calibr = m_calibration->getCalibrationEnabled();
    int i = _incrementally ? (m_measurements.length()-1) : 0;

    if(m_farEndMeasurement != 0)
    {
        for(; i < m_measurements.length(); ++i)
        {
            m_rlWidget->graph(i+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_farEndMeasurement == 1 ? m_farEndMeasurementsSub[i].rlGraph : m_farEndMeasurementsAdd[i].rlGraph));
        }
    }else
    {
        for(; i < m_measurements.length(); ++i)
        {
            m_rlWidget->graph(i+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(calibr ? m_measurements[i].rlGraphCalib : m_measurements[i].rlGraph));
        }
    }
    replot();
}

void Measurements::redrawS21(bool _incrementally)
{
    if (m_measurements.isEmpty())
        return;
//    bool calibr = m_calibration->getCalibrationEnabled();
    int i = _incrementally ? (m_measurements.length()-1) : 0;

    // 4 graphs per measurement (S21/S12 magnitude+phase, from a real
    // 2-port import -- see populateSParamData()), not the old 2
    // (live-only magnitude+"stage", s21Graph/s21StageGraph -- untouched,
    // just no longer plotted here; see SParamPoint's own comment).
    bool rescaledPhaseAxis = false;
    for(; i < m_measurements.length(); ++i)
    {
        if (m_measurements[i].visible) {
            m_s21Widget->graph(i*4+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements[i].s21MagGraph));
            m_s21Widget->graph(i*4+2)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements[i].s21PhaseGraph));
            m_s21Widget->graph(i*4+3)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements[i].s12MagGraph));
            m_s21Widget->graph(i*4+4)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements[i].s12PhaseGraph));

            // yAxis2 (phase, degrees) has no fixed sensible range the way
            // yAxis (dB) does -- unwrapped phase across a wide sweep can
            // span anywhere from a few tens to several thousand degrees
            // depending on the file. Rescale to fit whatever's actually
            // there instead of leaving it at its static default (was a
            // fixed 0-3 leftover from the old "Stage" axis -- see
            // mainwindow.cpp's S21 Widget setup -- which clipped nearly
            // all real phase data off-screen).
            m_s21Widget->graph(i*4+2)->rescaleValueAxis(rescaledPhaseAxis);
            rescaledPhaseAxis = true;
            m_s21Widget->graph(i*4+4)->rescaleValueAxis(rescaledPhaseAxis);
        }
    }

    replot();
}

void Measurements::redrawSmith(bool _incrementally)
{
    if (m_measurements.isEmpty())
        return;
    bool calibr = m_calibration->getCalibrationEnabled();
    int i = _incrementally ? (m_measurements.length()-1) : 0;

    if(m_farEndMeasurement)
    {
        if(m_farEndMeasurement == 1)
        {
            for(; i < m_measurements.length(); ++i)
            {
                if (m_measurements[i].visible) {
                    m_measurements[i].smithCurve->setData(QSharedPointer<QCPCurveDataContainer>::create(calibr ? m_farEndMeasurementsSub[i].smithGraphCalib : m_farEndMeasurementsSub[i].smithGraph));
                }
            }
        }else if(m_farEndMeasurement == 2)
        {
            for(; i < m_measurements.length(); ++i)
            {
                if (m_measurements[i].visible) {
                    m_measurements[i].smithCurve->setData(QSharedPointer<QCPCurveDataContainer>::create(calibr ? m_farEndMeasurementsAdd[i].smithGraphCalib : m_farEndMeasurementsAdd[i].smithGraph));
                }
            }
        }
    }else
    {
        for(; i < m_measurements.length(); ++i)
        {
            if (m_measurements[i].visible) {
                m_measurements[i].smithCurve->setData(QSharedPointer<QCPCurveDataContainer>::create(m_measurements[i].smithGraphViewCalib));
                m_measurements[i].smithCurve->setData(QSharedPointer<QCPCurveDataContainer>::create(calibr ? m_measurements[i].smithGraphViewCalib : m_measurements[i].smithGraphView));
            }
        }
    }
    replot();
}

void Measurements::redrawUser(bool _incrementally)
{
    if (m_measurements.isEmpty())
        return;
    int i = _incrementally ? (m_measurements.length()-1) : 0;

    for( ; i < m_measurements.length(); ++i)
    {
        int count = m_viewMeasurements[i].userGraphs.size();
        for (int idx=0; idx<count; idx++) {
            if (!_incrementally)
                m_viewMeasurements[i].userGraphs[idx]->clear();

            QCPGraphData data;
            QCPGraphData value;
            double maxVal = m_userWidget->yAxis->range().upper;
            double minVal = m_userWidget->yAxis->range().lower;

            QCPGraphDataContainer* map = m_measurements[i].userGraphs.at(idx);
            QCPGraphDataContainer* vmap = m_viewMeasurements[i].userGraphs.at(idx);

            // See the SWR-widget block above (2026-08-25 QCustomPlot 2.x
            // port) -- same self-referencing map->at(n) walk.
            if (map->isEmpty())
                continue;
            int n = _incrementally ? (map->size()-1) : 0;
            for( ; n < map->size(); ++n)
            {
                QCPGraphData data = *map->at(n);
                restrictData(minVal, maxVal, data);
                vmap->add(data);
            }
            int index = getBaseUserGraphIndex(i)+idx;
            m_userWidget->graph(index)->setData(QSharedPointer<QCPGraphDataContainer>::create(*m_viewMeasurements[i].userGraphs[idx]));
        }
    }
}

// Used to re-color m_graphHint here (background/text following the app
// theme, independent of the plot's own chart-background -- see setHintColor(),
// removed). m_graphHint is a plain docked, normally-themed QGroupBox with a
// QFormLayout of QLabel rows now (m_graphHintBox/m_graphHintNameLabels/
// m_graphHintValueLabels -- see setGraphHintWidgets()) with nothing left
// for a theme change to touch here; MainWindow's own changeColorTheme()
// already re-skins it like every other widget via its own
// qApp->setStyleSheet(Style::globalStyleSheet()) call. This function (and
// its call sites) is gone along with the last of its work.

void Measurements::redrawMultiGraph(bool _incrementally)
{
#ifndef NO_MULTITAB
    QString old_m_currentTab = m_currentTab;
    const QList<QString>& tabs = MainWindow::m_mainWindow->multiTabs();
    foreach (const QString& tab, tabs) {
        m_currentTab = tab;
        on_redrawGraphs(_incrementally);
    }
    m_currentTab = old_m_currentTab;
#endif
}

