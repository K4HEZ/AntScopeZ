#ifndef VNAAIRCALIBRATIONWIZARDDIALOG_H
#define VNAAIRCALIBRATIONWIZARDDIALOG_H

#include <QDialog>

class VnaAirAnalyzer;
class QLabel;
class QPushButton;

// Runs a fresh VNA Air bridge-side (or relayed NanoVNA-device-side,
// depending on the bridge's active calibration engine) Open/Short/Load
// calibration into one slot, over the Calibration characteristic's
// begin/standard/done/save op sequence (see research/vnaair-protocol.md).
// Deliberately NOT built on CalibrationWizardDialog/Calibration
// (core/calibration.h) -- that one drives AntScopeZ's own file-based OSL
// math by sweeping the analyzer itself page by page; here the bridge (or
// the NanoVNA it's attached to) does the actual measuring and math, so
// this dialog only sends small BLE writes and waits for the bridge's own
// "ok"/"error"/"running" replies.
class VnaAirCalibrationWizardDialog : public QDialog
{
    Q_OBJECT

public:
    // targetSlot: the slot number Save commits into, 0-based -- matches
    // the Settings combo's itemData and the wire protocol's own slot
    // numbering. Displayed to the user as targetSlot+1 (1-based, matching
    // the combo's visible "Slot 1"/"Slot 2"/... labels) -- was showing
    // the raw 0-based number ("slot 0") until this, disagreeing with the
    // combo right next to it.
    explicit VnaAirCalibrationWizardDialog(VnaAirAnalyzer* analyzer, int targetSlot,
                                            qint64 startHz, qint64 stopHz, int points,
                                            QWidget* parent = nullptr);

private slots:
    void onOpOk(const QString& op);
    void onOpRunning(const QString& op, int deadlineMs);
    void onOpError(const QString& op, const QString& error);
    void onActionClicked();
    void onCancelClicked();

private:
    enum Step { StepReset, StepBegin, StepOpen, StepShort, StepLoad, StepDone, StepSave, StepFinished };
    void enterStep(Step step);

    VnaAirAnalyzer* m_analyzer;
    int m_targetSlot;
    qint64 m_startHz;
    qint64 m_stopHz;
    int m_points;
    Step m_step = StepBegin;

    QLabel* m_promptLabel;
    QLabel* m_statusLabel;
    QPushButton* m_actionBtn;
    QPushButton* m_cancelBtn;
};

#endif // VNAAIRCALIBRATIONWIZARDDIALOG_H
