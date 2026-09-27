#include "customgraph.h"

CustomGraph::CustomGraph(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : QCPGraph(keyAxis, valueAxis)
{
}

void CustomGraph::draw(QCPPainter *painter)
{
    if (checked()) {
        QCPGraph::draw(painter);
    }
}
