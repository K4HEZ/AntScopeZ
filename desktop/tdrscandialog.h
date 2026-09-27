#ifndef TDRSCANDIALOG_H
#define TDRSCANDIALOG_H

#include <QDialog>
#include <QCloseEvent>
#include "tdrscanpanel.h"

// Thin container for TdrScanPanel -- Tools > TDR Measurement
// (MainWindow::on_actionTDRMeasurement_triggered()), same non-modal,
// single-instance tool-dialog shape as MarkerComparisonDialog. Holds no
// real logic of its own -- everything else lives in TdrScanPanel (which
// absorbed TDRAnalysisDialog's own content 2026-08-21 -- see the
// tdr-scan-rework-plan memory), so moving this content into a docked
// QGroupBox later is a container swap, not a rewrite. The two exceptions
// are closing() below (forwarding a signal, not owning any scan state
// itself) and the Close button added in the constructor -- a dialog-level
// concern that wouldn't make sense on TdrScanPanel itself if it ever ends
// up docked instead of in a dialog.
class TdrScanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TdrScanDialog(QWidget* parent = nullptr);

    TdrScanPanel* panel() const { return m_panel; }

signals:
    // Emitted from both reject() (Esc -- QDialog's default reject() just
    // hide()s, doesn't fire closeEvent()) and closeEvent() (the window's
    // close button, or any real close()/WA_DeleteOnClose teardown) so
    // MainWindow can stop an in-progress scan regardless of which of the
    // two the user actually triggers. See MainWindow::on_tdrStopRequested().
    void closing();

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    TdrScanPanel* m_panel;
};

#endif // TDRSCANDIALOG_H
