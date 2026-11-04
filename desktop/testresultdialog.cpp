#include "testresultdialog.h"
#include "testresultplot.h"
#include <calibration.h>
#include <qcustomplot.h>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

TestResultDialog::TestResultDialog(Calibration* calibration, QWidget* parent)
    : QDialog(parent), m_calibration(calibration)
{
    setWindowTitle(tr("OSL Calibration Test Result"));
    setMinimumSize(520, 360);

    calibration->reloadTestResult();

    m_plot = createTestResultPlot();
    populateTestResultPlot(m_plot, calibration, calibration->calibZ0());

    connect(m_calibration, &Calibration::testCaptureFinished,
            this, &TestResultDialog::onTestCaptureFinished);

    m_reTestBtn = new QPushButton(tr("Re-Test"), this);
    m_reTestBtn->setToolTip(tr("Measure the connected load again and refresh this plot --\n"
                                "the only way to re-verify the active calibration without\n"
                                "redoing the whole Calibration Wizard."));
    m_reTestBtn->setEnabled(m_calibration->isAnalyzerConnected());
    connect(m_reTestBtn, &QPushButton::clicked, this, &TestResultDialog::onReTestClicked);

    QPushButton* closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(m_reTestBtn);
    row->addStretch();
    row->addWidget(closeBtn);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_plot);
    layout->addLayout(row);
}

void TestResultDialog::onReTestClicked()
{
    m_reTestBtn->setEnabled(false);
    m_calibration->on_testCalibration();
}

void TestResultDialog::onTestCaptureFinished()
{
    // on_testCalibration() leaves the fresh sweep in memory only --
    // commit it to cal_test.s1p ourselves (same file the wizard's Finish
    // step would write) so Settings' "Completed <date/time>" status and
    // a later "View Plot..." both see this run, not just this dialog.
    m_calibration->commitTestResult();
    populateTestResultPlot(m_plot, m_calibration, m_calibration->calibZ0());
    m_reTestBtn->setEnabled(m_calibration->isAnalyzerConnected());
}
