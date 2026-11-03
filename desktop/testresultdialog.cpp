#include "testresultdialog.h"
#include "testresultplot.h"
#include <calibration.h>
#include <qcustomplot.h>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

TestResultDialog::TestResultDialog(Calibration* calibration, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("OSL Calibration Test Result"));
    setMinimumSize(520, 360);

    calibration->reloadTestResult();

    m_plot = createTestResultPlot();
    populateTestResultPlot(m_plot, calibration, calibration->calibZ0());

    QPushButton* closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    row->addWidget(closeBtn);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_plot);
    layout->addLayout(row);
}
