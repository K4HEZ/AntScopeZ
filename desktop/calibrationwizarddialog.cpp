#include "calibrationwizarddialog.h"
#include "testresultplot.h"
#include <calibration.h>
#include <qcustomplot.h>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

CalibrationWizardDialog::CalibrationWizardDialog(Calibration* calibration, QWidget* parent)
    : QDialog(parent), m_calibration(calibration)
{
    setWindowTitle(tr("OSL Calibration Wizard"));
    setWindowModality(Qt::ApplicationModal);
    setMinimumSize(520, 420);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildIntroPage());
    m_stack->addWidget(buildCapturePage(m_captures[0], tr("Connect the OPEN standard, then click Continue."), false));
    m_stack->addWidget(buildCapturePage(m_captures[1], tr("Connect the SHORT standard, then click Continue."), false));
    m_stack->addWidget(buildCapturePage(m_captures[2], tr("Connect the LOAD standard, then click Continue."), false));
    m_stack->addWidget(buildCapturePage(m_captures[3], tr("Confirm the LOAD standard is still connected "
                                                           "(or connect a reference load), then click "
                                                           "Continue to test it."), true));
    m_stack->addWidget(buildDonePage());
    m_stack->setCurrentIndex(PageIntro);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &CalibrationWizardDialog::onCancelClicked);

    QHBoxLayout* bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    bottomRow->addWidget(m_cancelBtn);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_stack);
    mainLayout->addLayout(bottomRow);

    connect(m_calibration, &Calibration::progress, this, &CalibrationWizardDialog::onProgress);
    connect(m_calibration, &Calibration::testCaptureFinished, this, &CalibrationWizardDialog::onTestCaptureFinished);
}

QWidget* CalibrationWizardDialog::buildIntroPage()
{
    QWidget* page = new QWidget();
    QLabel* text = new QLabel(tr("This wizard captures Open, Short, and Load, then tests the result.\n\n"
                                  "Nothing is saved until you reach Finish -- cancel at any point to "
                                  "leave your existing calibration untouched."));
    text->setWordWrap(true);

    QPushButton* continueBtn = new QPushButton(tr("Continue"));
    connect(continueBtn, &QPushButton::clicked, this, [this](){
        m_stack->setCurrentIndex(PageOpen);
    });

    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    row->addWidget(continueBtn);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(text);
    layout->addStretch();
    layout->addLayout(row);
    return page;
}

QWidget* CalibrationWizardDialog::buildCapturePage(CapturePage& out, const QString& promptText, bool withPlot)
{
    QWidget* page = new QWidget();

    out.page = page;
    out.prompt = new QLabel(promptText);
    out.prompt->setWordWrap(true);
    out.progress = new QProgressBar();
    out.progress->setValue(0);
    out.continueBtn = new QPushButton(tr("Continue"));
    connect(out.continueBtn, &QPushButton::clicked, this, &CalibrationWizardDialog::onContinueClicked);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(out.prompt);
    layout->addWidget(out.progress);

    if (withPlot) {
        out.plot = createTestResultPlot();
        out.plot->hide();
        layout->addWidget(out.plot);
    }

    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    row->addWidget(out.continueBtn);
    layout->addStretch();
    layout->addLayout(row);
    return page;
}

QWidget* CalibrationWizardDialog::buildDonePage()
{
    QWidget* page = new QWidget();
    m_doneSummary = new QLabel();
    m_doneSummary->setWordWrap(true);

    m_finishBtn = new QPushButton(tr("Finish"));
    connect(m_finishBtn, &QPushButton::clicked, this, &CalibrationWizardDialog::onFinishClicked);

    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    row->addWidget(m_finishBtn);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(m_doneSummary);
    layout->addStretch();
    layout->addLayout(row);
    return page;
}

void CalibrationWizardDialog::startCurrentCapture()
{
    switch (m_stack->currentIndex()) {
    case PageOpen:  m_calibration->on_startCalibrationOpen();  break;
    case PageShort: m_calibration->on_startCalibrationShort(); break;
    case PageLoad:  m_calibration->on_startCalibrationLoad();  break;
    case PageTest:
        m_testCaptureReady = false;
        m_captures[PageTest - PageOpen].plot->hide();
        m_calibration->on_testCalibration();
        break;
    default: break;
    }
}

void CalibrationWizardDialog::onContinueClicked()
{
    int idx = m_stack->currentIndex();
    if (idx < PageOpen || idx > PageTest)
        return;
    if (idx == PageTest && m_testCaptureReady) {
        // Second click, after the plot's already up -- just move on.
        m_doneSummary->setText(tr("Open, Short, and Load have been captured.\n\n"
                                   "Finish to save this calibration. You can view "
                                   "the Test plot again afterward from the OSL tab's "
                                   "Test row."));
        m_stack->setCurrentIndex(PageDone);
        return;
    }
    m_captures[idx - PageOpen].continueBtn->setEnabled(false);
    startCurrentCapture();
}

void CalibrationWizardDialog::onProgress(int state, int percent)
{
    int idx = m_stack->currentIndex();
    if (idx < PageOpen || idx > PageTest || state != idx)
        return;
    m_captures[idx - PageOpen].progress->setValue(percent);
    // Test's own page advances from onTestCaptureFinished() instead, once
    // the plot is populated -- the user needs to see it before moving on.
    if (percent >= 100 && idx != PageTest)
        m_stack->setCurrentIndex(idx + 1);
}

void CalibrationWizardDialog::onTestCaptureFinished()
{
    if (m_stack->currentIndex() != PageTest)
        return;

    CapturePage& cp = m_captures[PageTest - PageOpen];
    cp.progress->setValue(100);

    populateTestResultPlot(cp.plot, m_calibration, m_calibration->getZ0());
    cp.plot->show();

    m_testCaptureReady = true;
    cp.continueBtn->setEnabled(true);
}

void CalibrationWizardDialog::onCancelClicked()
{
    m_calibration->on_cancelCalibration();
    reject();
}

void CalibrationWizardDialog::onFinishClicked()
{
    m_calibration->on_wizardCommit();
    accept();
}
