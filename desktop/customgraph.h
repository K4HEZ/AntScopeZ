#ifndef CUSTOMGRAPH_H
#define CUSTOMGRAPH_H

#include <QObject>
#include "qcustomplot.h"

// Used to also carry an incremental XOR-draw pair (drawLine()/restoreLine()/
// getClipRect()/getLineData()) for cheaply moving a live-scan tracer without
// a full replot -- confirmed 2026-08-25 (grep across the whole app, plus a
// full git-history search for its would-be orchestrator,
// CustomPlot::drawIncrementally(), which has never existed as actual code
// in this repo) to have been fully dead: zero callers, getLineData() never
// even had a definition. Removed outright rather than ported against 2.x's
// QCPGraphDataContainer -- see [[qcustomplot-2x-upgrade-deferred]] memory
// for the investigation. Only the checked()-gated draw() override (used to
// hide/show a graph's line without removing it) is real, load-bearing
// behavior.
class CustomGraph : public QCPGraph
{
    Q_OBJECT

    bool m_checked = true;

public:
    explicit CustomGraph(QCPAxis *keyAxis, QCPAxis *valueAxis);

    bool checked() const { return m_checked; }
    void setChecked(bool _state) { m_checked = _state; }

protected:
    virtual void draw(QCPPainter *painter);
};

#endif // CUSTOMGRAPH_H
