#include "tdrscandialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>

TdrScanDialog::TdrScanDialog(QWidget* parent) :
    QDialog(parent),
    m_panel(new TdrScanPanel(this))
{
    setWindowTitle(tr("TDR Measurement"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_panel);
    // Belt-and-suspenders alongside TdrScanPanel's own verticalLayout
    // (SetMinimumSize there) -- this is the layout that actually governs
    // the resizable window's minimum size. Never lets the dialog be resized
    // smaller than what its content currently needs, so nothing (e.g. the
    // reverse-solve controls) squishes when the window shrinks, or when a
    // window-function explanation's wrapped text grows/shrinks after a
    // combobox change (reported 2026-08-21).
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    // Close button, lower right -- same standalone-Close pattern the
    // now-merged TDRAnalysisDialog used. Routed through this->reject()
    // (not close() directly) so it goes through the same closing() ->
    // on_tdrStopRequested() safety net as Esc/the window's own close
    // button, not a separate path.
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &TdrScanDialog::reject);
    layout->addWidget(buttonBox);

    // Modeless -- see MainWindow::on_actionTDRMeasurement_triggered()
    // (show(), never exec()). No WA_DeleteOnClose here; the caller sets
    // that, same as MarkerComparisonDialog/TDRAnalysisDialog.
}

void TdrScanDialog::closeEvent(QCloseEvent* event)
{
    emit closing();
    QDialog::closeEvent(event);
}

void TdrScanDialog::reject()
{
    emit closing();
    QDialog::reject();
}
