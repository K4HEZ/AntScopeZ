#ifndef TESTRESULTPLOT_H
#define TESTRESULTPLOT_H

class QCustomPlot;
class Calibration;

// Builds and fills the OSL Test's ΔR/ΔX-vs-frequency scatter plot --
// shared by the Calibration Wizard's inline Test page and the standalone
// "View Plot" dialog launched from the Settings OSL tab, so the two never
// drift apart.
QCustomPlot* createTestResultPlot();
// z0 is the reference impedance the plotted data was captured against --
// pass Calibration::getZ0() for a just-captured, not-yet-committed result
// (the live wizard preview) or Calibration::calibZ0() for anything read
// back off disk, since the live system impedance may have changed since.
void populateTestResultPlot(QCustomPlot* plot, Calibration* calibration, double z0);

#endif // TESTRESULTPLOT_H
