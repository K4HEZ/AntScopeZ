#include "vnaaircalibrationwizarddialog.h"
#include <vnaair_analyzer.h>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

VnaAirCalibrationWizardDialog::VnaAirCalibrationWizardDialog(VnaAirAnalyzer* analyzer, int targetSlot,
                                                               qint64 startHz, qint64 stopHz, int points,
                                                               QWidget* parent)
    : QDialog(parent)
    , m_analyzer(analyzer)
    , m_targetSlot(targetSlot)
    , m_startHz(startHz)
    , m_stopHz(stopHz)
    , m_points(points)
{
    setWindowTitle(tr("VNA Air Calibration"));
    setMinimumWidth(420);

    m_promptLabel = new QLabel(this);
    m_promptLabel->setWordWrap(true);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: grey;"));
    m_actionBtn = new QPushButton(this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    // Without this, QDialog's implicit-default-button rule picks whichever
    // autoDefault button got focus first -- Cancel, added first below --
    // so Enter cancelled instead of advancing. setDefault(true) pins
    // Enter to the action button regardless of focus; Escape already
    // calls reject() unconditionally (QDialog's own built-in behavior, not
    // tied to any button here).
    m_actionBtn->setDefault(true);
    m_cancelBtn->setAutoDefault(false);

    connect(m_actionBtn, &QPushButton::clicked, this, &VnaAirCalibrationWizardDialog::onActionClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &VnaAirCalibrationWizardDialog::onCancelClicked);

    QHBoxLayout* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    buttonRow->addWidget(m_cancelBtn);
    buttonRow->addWidget(m_actionBtn);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_promptLabel);
    layout->addWidget(m_statusLabel);
    layout->addStretch();
    layout->addLayout(buttonRow);

    // Connected before the first enterStep() so StepReset's immediate
    // cancelCalibrationCapture() call can't finish before we're listening.
    connect(m_analyzer, &VnaAirAnalyzer::calibrationOpOk, this, &VnaAirCalibrationWizardDialog::onOpOk);
    connect(m_analyzer, &VnaAirAnalyzer::calibrationOpRunning, this, &VnaAirCalibrationWizardDialog::onOpRunning);
    connect(m_analyzer, &VnaAirAnalyzer::calibrationOpError, this, &VnaAirCalibrationWizardDialog::onOpError);

    enterStep(StepReset);
}

void VnaAirCalibrationWizardDialog::enterStep(Step step)
{
    m_step = step;
    switch (step) {
    case StepReset:
        // A previous attempt that errored or was force-closed (app crash,
        // Settings re-opened without a clean Cancel) can leave a
        // begin/standard/done session open bridge-side with nothing here
        // to know it -- the next begin would then fail "session_active"
        // for no reason visible to this dialog, confirmed against real
        // hardware 2026-11-05. Unconditionally end whatever might be open
        // before ever sending begin; onOpError() treats this step's own
        // "no_session" reply as success, not a real error, since that's
        // exactly the expected case when nothing was open to begin with.
        m_promptLabel->setText(tr("Preparing..."));
        m_statusLabel->clear();
        m_actionBtn->setText(tr("Retry"));
        m_actionBtn->setEnabled(false);
        m_analyzer->cancelCalibrationCapture();
        break;
    case StepBegin:
        m_promptLabel->setText(tr("Starting a calibration session..."));
        m_statusLabel->clear();
        m_actionBtn->setText(tr("Retry"));
        m_actionBtn->setEnabled(false);
        m_analyzer->beginCalibrationCapture(m_startHz, m_stopHz, m_points);
        break;
    case StepOpen:
        m_promptLabel->setText(tr("Connect the OPEN standard, then click Measure."));
        m_statusLabel->clear();
        m_actionBtn->setText(tr("Measure"));
        m_actionBtn->setEnabled(true);
        break;
    case StepShort:
        m_promptLabel->setText(tr("Connect the SHORT standard, then click Measure."));
        m_statusLabel->clear();
        m_actionBtn->setText(tr("Measure"));
        m_actionBtn->setEnabled(true);
        break;
    case StepLoad:
        m_promptLabel->setText(tr("Connect the LOAD (50 ohm/Z0) standard, then click Measure."));
        m_statusLabel->clear();
        m_actionBtn->setText(tr("Measure"));
        m_actionBtn->setEnabled(true);
        break;
    case StepDone:
        m_promptLabel->setText(tr("Computing calibration..."));
        m_statusLabel->clear();
        m_actionBtn->setText(tr("Retry"));
        m_actionBtn->setEnabled(false);
        m_analyzer->finishCalibrationCapture();
        break;
    case StepSave:
        // +1: see the header's own comment on m_targetSlot's 0-based vs.
        // displayed numbering.
        m_promptLabel->setText(tr("Save this calibration into slot %1?").arg(m_targetSlot + 1));
        m_statusLabel->clear();
        m_actionBtn->setText(tr("Save"));
        m_actionBtn->setEnabled(true);
        break;
    case StepFinished:
        m_promptLabel->setText(tr("Calibration saved to slot %1.").arg(m_targetSlot + 1));
        m_statusLabel->clear();
        m_actionBtn->setEnabled(false);
        m_cancelBtn->setText(tr("Close"));
        break;
    }
}

void VnaAirCalibrationWizardDialog::onActionClicked()
{
    switch (m_step) {
    case StepReset:     m_actionBtn->setEnabled(false); m_analyzer->cancelCalibrationCapture(); break;
    case StepBegin:     m_actionBtn->setEnabled(false); m_analyzer->beginCalibrationCapture(m_startHz, m_stopHz, m_points); break;
    case StepOpen:      m_actionBtn->setEnabled(false); m_analyzer->measureCalibrationStandard(0); break;
    case StepShort:     m_actionBtn->setEnabled(false); m_analyzer->measureCalibrationStandard(1); break;
    case StepLoad:      m_actionBtn->setEnabled(false); m_analyzer->measureCalibrationStandard(2); break;
    case StepDone:      m_actionBtn->setEnabled(false); m_analyzer->finishCalibrationCapture(); break;
    case StepSave:      m_actionBtn->setEnabled(false); m_analyzer->saveCalibrationSlot(m_targetSlot); break;
    case StepFinished:  break;
    }
}

void VnaAirCalibrationWizardDialog::onOpOk(const QString& op)
{
    // Guarded by m_step, not just op name -- "standard" ok replies arrive
    // identically for open/short/load, so which step we're actually on is
    // what decides where to advance to.
    if (op == QStringLiteral("end") && m_step == StepReset)
        enterStep(StepBegin);
    else if (op == QStringLiteral("begin") && m_step == StepBegin)
        enterStep(StepOpen);
    else if (op == QStringLiteral("standard") && m_step == StepOpen)
        enterStep(StepShort);
    else if (op == QStringLiteral("standard") && m_step == StepShort)
        enterStep(StepLoad);
    else if (op == QStringLiteral("standard") && m_step == StepLoad)
        enterStep(StepDone);
    else if (op == QStringLiteral("done") && m_step == StepDone)
        enterStep(StepSave);
    else if (op == QStringLiteral("save") && m_step == StepSave)
        enterStep(StepFinished);
}

void VnaAirCalibrationWizardDialog::onOpRunning(const QString& op, int deadlineMs)
{
    Q_UNUSED(op);
    m_statusLabel->setText(tr("Working... (up to %1s)").arg(deadlineMs / 1000.0, 0, 'f', 1));
}

void VnaAirCalibrationWizardDialog::onOpError(const QString& op, const QString& error)
{
    Q_UNUSED(op);
    // Expected, not a failure -- see StepReset's own comment: this is
    // what a clean start (nothing was open) looks like.
    if (m_step == StepReset && error == QStringLiteral("no_session")) {
        enterStep(StepBegin);
        return;
    }
    m_statusLabel->setText(tr("Error: %1 -- try again.").arg(error));
    m_actionBtn->setEnabled(true);
}

void VnaAirCalibrationWizardDialog::onCancelClicked()
{
    // StepReset is already doing exactly this; nothing left to abort once
    // Save has already succeeded.
    if (m_step != StepReset && m_step != StepFinished)
        m_analyzer->cancelCalibrationCapture();
    reject();
}
