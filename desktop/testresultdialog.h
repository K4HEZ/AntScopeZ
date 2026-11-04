#ifndef TESTRESULTDIALOG_H
#define TESTRESULTDIALOG_H

#include <QDialog>

class Calibration;
class QCustomPlot;
class QPushButton;

// Settings OSL tab's "View Plot..." button -- redisplays the last committed
// Test result (cal_test.s1p) without re-running the Calibration Wizard.
// Reloads from disk on open via Calibration::reloadTestResult() rather than
// trusting whatever a since-cancelled wizard run left in memory. Also offers
// "Re-Test": the only way to re-verify the calibration currently in effect
// without redoing the whole Open/Short/Load wizard -- the plot otherwise
// shown here can be arbitrarily stale (whenever Test last ran during the
// wizard that produced the active calibration).
class TestResultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TestResultDialog(Calibration* calibration, QWidget* parent = nullptr);

private slots:
    void onReTestClicked();
    void onTestCaptureFinished();

private:
    Calibration* m_calibration;
    QCustomPlot* m_plot;
    QPushButton* m_reTestBtn;
};

#endif // TESTRESULTDIALOG_H
