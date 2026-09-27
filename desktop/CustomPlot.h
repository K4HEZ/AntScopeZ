#ifndef CUSTOMPLOT_H
#define CUSTOMPLOT_H

#include "qcustomplot.h"
#include "customgraph.h"

class CustomPlot : public QCustomPlot
{
    Q_OBJECT

    int m_graphCount;

public:
    explicit CustomPlot(int _numGraphs, QWidget *parent = 0);
    QCPGraph *addGraph(QCPAxis *keyAxis=0, QCPAxis *valueAxis=0);
    CustomGraph *graph(int index) const;
    CustomGraph *graph() const;
    void setGraphCount(int _count) { m_graphCount = _count; }
};

// Replaces two separate pieces of 1.x-only code the SWR y-axis used to
// need (2026-08-25, QCustomPlot 2.x port): setAutoTicks()/setAutoSubTicks()/
// setTickStep()/setAutoTickCount() (removed by 2.0's QCPAxisTicker
// refactor -- setTickStep(0.1) was already dead in the 1.x app itself,
// since setAutoTickStep() was never disabled so it never took effect;
// setAutoTicks/setAutoSubTicks(true) is just 2.x's unconfigured default
// behavior, needs no replacement) and Measurements::replot()'s SWR-only
// special case (measurements_redraw.cpp), which used to replot twice --
// once normally, then read back the auto-computed tick labels, blank out
// any below 1 (not a physically meaningful SWR value), and replot again
// with those forced in via setAutoTickLabels(false)/setTickVectorLabels(),
// both also removed in 2.x. A ticker subclass gets the same "labels below
// 1 come out blank" result in one pass, and preserves the one genuinely
// live setting from the old setup block, the ~8 tick-count hint
// (setAutoTickCount(8) -> setTickCount(8)).
class SwrAxisTicker : public QCPAxisTicker
{
public:
    SwrAxisTicker() { setTickCount(8); }

protected:
    QString getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision) Q_DECL_OVERRIDE
    {
        if (tick < 1)
            return QString();
        return QCPAxisTicker::getTickLabel(tick, locale, formatChar, precision);
    }
};

#endif // CUSTOMPLOT_H
