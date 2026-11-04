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
    setWindowTitle(tr("Calibration Test Result"));
    setMinimumSize(520, 360);

    QCustomPlot* plot = createTestResultPlot();
    // getZ0(), not calibZ0() -- this isn't specific to AntScopeZ's own OSL
    // calibration (which calibZ0() describes), it's whatever the live
    // system impedance is right now, since Test verifies whatever
    // correction is actually active.
    populateTestResultPlot(plot, calibration, calibration->getZ0());

    QPushButton* closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch();
    row->addWidget(closeBtn);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(plot);
    layout->addLayout(row);
}
