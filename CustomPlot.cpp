#include "CustomPlot.h"


CustomPlot::CustomPlot(int _numGraphs, QWidget *parent)
    : m_graphCount(_numGraphs), QCustomPlot(parent)
{
}

QCPGraph *CustomPlot::addGraph(QCPAxis *keyAxis, QCPAxis *valueAxis)
{
  if (!keyAxis) keyAxis = xAxis;
  if (!valueAxis) valueAxis = yAxis;
  if (!keyAxis || !valueAxis)
  {
    qDebug() << Q_FUNC_INFO << "can't use default QCustomPlot xAxis or yAxis, because at least one is invalid (has been deleted)";
    return 0;
  }
  if (keyAxis->parentPlot() != this || valueAxis->parentPlot() != this)
  {
    qDebug() << Q_FUNC_INFO << "passed keyAxis or valueAxis doesn't have this QCustomPlot as parent";
    return 0;
  }

  QCPGraph *newGraph = new CustomGraph(keyAxis, valueAxis);
  if (addPlottable(newGraph))
  {
    newGraph->setName(QLatin1String("Graph ")+QString::number(mGraphs.size()));
    return newGraph;
  } else
  {
    delete newGraph;
    return 0;
  }
}

CustomGraph *CustomPlot::graph(int index) const
{
    // addGraph() isn't virtual (neither here nor in QCustomPlot), so any
    // ->addGraph() call made through a QCustomPlot*-typed pointer (several
    // exist in measurements.cpp) silently creates a plain QCPGraph instead
    // of a CustomGraph. A blind C-style cast on one of those then reads
    // CustomGraph-only members (e.g. m_prevLineCoords) out of an object
    // that doesn't have them -- undefined behavior, seen as segfaults at
    // small/bogus addresses on essentially any mouse move over the plot.
    // dynamic_cast returns nullptr instead, which callers already handle.
    return dynamic_cast<CustomGraph*>(QCustomPlot::graph(index));
}

void CustomPlot::setIncremental(bool _mode)
{
    m_incrementalDraw = _mode;
}


CustomGraph *CustomPlot::graph() const
{
    return dynamic_cast<CustomGraph*>(QCustomPlot::graph());
}

void CustomPlot::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  QPainter painter(this);
  painter.drawPixmap(event->rect().topLeft(), mPaintBuffer, event->rect());
}

