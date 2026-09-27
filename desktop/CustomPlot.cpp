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

  // 2.1.1: plottables self-register with their parent plot in their own
  // constructor (QCPLayerable's), unlike 1.3.1 where addPlottable() had to
  // be called as a separate, fallible second step -- see QCustomPlot::
  // addGraph()'s own 2.1.1 implementation, which this mirrors.
  QCPGraph *newGraph = new CustomGraph(keyAxis, valueAxis);
  // Needs tr() attention: generic fallback legend name, used only if the
  // caller doesn't set a real one right after addGraph() (every actual
  // graph in this app does -- see Measurements::setWidgets()), so this
  // is rarely if ever what a user actually sees.
  newGraph->setName(QLatin1String("Graph ")+QString::number(mGraphs.size()));
  return newGraph;
}

CustomGraph *CustomPlot::graph(int index) const
{
    // addGraph() isn't virtual (neither here nor in QCustomPlot), so any
    // ->addGraph() call made through a QCustomPlot*-typed pointer (several
    // exist in measurements.cpp) silently creates a plain QCPGraph instead
    // of a CustomGraph. A blind C-style cast on one of those then reads
    // CustomGraph-only members out of an object that doesn't have them --
    // undefined behavior, seen as segfaults at small/bogus addresses on
    // essentially any mouse move over the plot. dynamic_cast returns
    // nullptr instead, which callers already handle.
    return dynamic_cast<CustomGraph*>(QCustomPlot::graph(index));
}

CustomGraph *CustomPlot::graph() const
{
    return dynamic_cast<CustomGraph*>(QCustomPlot::graph());
}
