#include "mainwindow.h"
#include "ui_mainwindow.h"

// Tools menu (mainwindow.ui's menuTools) lives here, matching this
// project's existing tier-1 split of mainwindow.cpp by feature area.

void MainWindow::on_actionMarkerComparison_triggered()
{
    if (m_markerComparisonDialog == nullptr) {
        m_markerComparisonDialog = new MarkerComparisonDialog(m_markers, m_measurements, m_measureSystemMetric, this);
        m_markerComparisonDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_markerComparisonDialog, &QObject::destroyed, this, [this](){
            m_markerComparisonDialog = nullptr;
        });
        // Tracks a live Continuous scan the same way the chart/markers
        // themselves do -- see MarkerComparisonDialog::refresh(). Torn down
        // automatically when m_markerComparisonDialog is destroyed
        // (WA_DeleteOnClose above), no manual disconnect needed.
        connect(m_analyzer, &AnalyzerPro::measurementComplete,
                m_markerComparisonDialog, &MarkerComparisonDialog::refresh);
        // A marker added/removed on the plots doesn't produce a new sweep
        // (no measurementComplete above), so the combos need their own hook
        // to pick it up without the user having to reopen this dialog.
        connect(m_markers, &Markers::markersChanged,
                m_markerComparisonDialog, &MarkerComparisonDialog::refresh);
    }
    m_markerComparisonDialog->refresh();
    if (!m_markerComparisonDialog->isVisible())
        m_markerComparisonDialog->show();
    m_markerComparisonDialog->raise();
    m_markerComparisonDialog->activateWindow();
}
