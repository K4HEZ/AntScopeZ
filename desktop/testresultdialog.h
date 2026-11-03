#ifndef TESTRESULTDIALOG_H
#define TESTRESULTDIALOG_H

#include <QDialog>

class Calibration;
class QCustomPlot;

// Settings OSL tab's "View Plot..." button -- redisplays the last committed
// Test result (cal_test.s1p) without re-running the Calibration Wizard.
// Reloads from disk on open via Calibration::reloadTestResult() rather than
// trusting whatever a since-cancelled wizard run left in memory.
class TestResultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TestResultDialog(Calibration* calibration, QWidget* parent = nullptr);

private:
    QCustomPlot* m_plot;
};

#endif // TESTRESULTDIALOG_H
