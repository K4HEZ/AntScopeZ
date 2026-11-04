#ifndef TESTRESULTDIALOG_H
#define TESTRESULTDIALOG_H

#include <QDialog>

class Calibration;

// Shown once by Settings::onTestCalibrationCaptureFinished() after the OSL
// tab's "Test Calibration" button finishes a sweep -- plots whatever
// Calibration::on_testCalibration() just captured (Calibration::m_testData,
// in memory only). Nothing is saved, so there's nothing to redisplay later;
// closing this and clicking Test again is the only way to see a new result.
class TestResultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TestResultDialog(Calibration* calibration, QWidget* parent = nullptr);
};

#endif // TESTRESULTDIALOG_H
