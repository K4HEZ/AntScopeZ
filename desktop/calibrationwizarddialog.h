#ifndef CALIBRATIONWIZARDDIALOG_H
#define CALIBRATIONWIZARDDIALOG_H

#include <QDialog>

class Calibration;
class QStackedWidget;
class QLabel;
class QProgressBar;
class QPushButton;
class QCustomPlot;

// OSL Calibration Wizard -- Settings' "Calibration Wizard..." button opens
// this modally. Walks Open -> Short -> Load -> Test in order; each step's
// capture lands in Calibration's staging buffers (m_wizardOpenData/etc, see
// core/calibration.h), never the live calibration data. Cancel on any page
// discards the whole run via Calibration::on_cancelCalibration(). Nothing
// on disk or in the live calibration changes until Finish on the Done page,
// which calls Calibration::on_wizardCommit() -- that's the one point a
// wizard run actually takes effect, however far a prior, cancelled attempt
// got.
class CalibrationWizardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CalibrationWizardDialog(Calibration* calibration, QWidget* parent = nullptr);

private slots:
    void onContinueClicked();
    void onCancelClicked();
    void onFinishClicked();
    void onProgress(int state, int percent);
    // Test's sweep finished -- unlike Open/Short/Load, this doesn't
    // auto-advance: it plots the per-point error against reference bands
    // and waits for the user to judge it (no pass/fail) and click
    // Continue themselves.
    void onTestCaptureFinished();

private:
    // Index both into m_stack (0 = intro, 1..4 = the four capture steps in
    // Calibration's CALIB_OPEN..CALIB_TEST order, 5 = done) and, for the
    // capture pages, directly usable as a CALIB_* value.
    enum { PageIntro = 0, PageOpen, PageShort, PageLoad, PageTest, PageDone, PageCount };

    struct CapturePage {
        QWidget* page = nullptr;
        QLabel* prompt = nullptr;
        QProgressBar* progress = nullptr;
        QPushButton* continueBtn = nullptr;
        // Only set for the Test page -- see buildCapturePage()'s withPlot.
        QCustomPlot* plot = nullptr;
    };

    QWidget* buildIntroPage();
    QWidget* buildCapturePage(CapturePage& out, const QString& promptText, bool withPlot);
    QWidget* buildDonePage();
    void startCurrentCapture();

    Calibration* m_calibration;
    QStackedWidget* m_stack;

    // Indexed by PageOpen..PageTest (offset by -1, i.e. m_captures[0] is
    // Open's page).
    CapturePage m_captures[4];
    // Once Test has run at least once this wizard session, a second click
    // on its Continue button means "move on to Done", not "capture again".
    bool m_testCaptureReady = false;

    QLabel* m_doneSummary;
    QPushButton* m_cancelBtn;
    QPushButton* m_finishBtn;
};

#endif // CALIBRATIONWIZARDDIALOG_H
