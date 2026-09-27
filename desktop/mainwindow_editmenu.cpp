#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "style.h"
#include <QColorDialog>

// Edit menu (issue #52) -- exposes the same actions as the Measurements,
// Markers, and Presets tables' own right-click menus, as menu-bar items
// with the matching enable/disable behavior. Each handler mirrors its
// context-menu counterpart but acts on the table's current selection
// rather than a right-click position (see mainwindow_measurements_io.cpp,
// markerspanel.cpp, and mainwindow_presets_bands.cpp for those).

// Group headers via QMenu::addSection(), not a disabled QAction -- a
// disabled item rendered dimmed/grey like every other unavailable action,
// which read as "this whole group is unavailable" rather than "this is a
// label" (Harold's call, 2026-09-17, after comparing both in a throwaway
// side-by-side demo app).
void MainWindow::buildEditMenu()
{
    m_editSectionPresets = ui->menuEdit->addSection(tr("Presets"));
    ui->menuEdit->addAction(ui->actionEditPresetsAdd);
    ui->menuEdit->addAction(ui->actionEditPresetsDelete);
    ui->menuEdit->addAction(ui->actionEditPresetsMoveUp);

    m_editSectionMeasurements = ui->menuEdit->addSection(tr("Measurements"));
    ui->menuEdit->addAction(ui->actionEditMeasurementsSelectColor);
    ui->menuEdit->addAction(ui->actionEditMeasurementsRename);
    ui->menuEdit->addAction(ui->actionEditMeasurementsSaveAs);
    ui->menuEdit->addAction(ui->actionEditMeasurementsToggleVisibility);
    ui->menuEdit->addAction(ui->actionEditMeasurementsDelete);
    ui->menuEdit->addAction(ui->actionEditMeasurementsClearAll);

    m_editSectionMarkers = ui->menuEdit->addSection(tr("Markers"));
    ui->menuEdit->addAction(ui->actionEditMarkersClearSelected);
    ui->menuEdit->addAction(ui->actionEditMarkersClearAll);
    ui->menuEdit->addAction(ui->actionEditMarkersClearEmpty);
}

// Connected to menuEdit's aboutToShow() -- recomputed right before the menu
// opens rather than tracked continuously, since nothing else needs to know
// these states change.
void MainWindow::updateEditMenuState()
{
    bool measurementSelected = !ui->tableWidget_measurments->selectedItems().isEmpty();
    bool anyMeasurements = m_measurements->getMeasurementLength() > 0;
    ui->actionEditMeasurementsSelectColor->setEnabled(measurementSelected);
    ui->actionEditMeasurementsRename->setEnabled(measurementSelected);
    ui->actionEditMeasurementsSaveAs->setEnabled(measurementSelected);
    ui->actionEditMeasurementsDelete->setEnabled(measurementSelected);
    ui->actionEditMeasurementsToggleVisibility->setEnabled(measurementSelected);
    ui->actionEditMeasurementsClearAll->setEnabled(anyMeasurements);

    bool markerSelected = m_markers->markersHint()->hasSelectedMarker();
    bool anyMarkers = m_markers->markersHint()->hasMarkers();
    ui->actionEditMarkersClearSelected->setEnabled(markerSelected);
    ui->actionEditMarkersClearAll->setEnabled(anyMarkers);
    ui->actionEditMarkersClearEmpty->setEnabled(anyMarkers);

    QList<QTableWidgetItem*> presetSel = ui->tableWidget_presets->selectedItems();
    bool presetSelected = !presetSel.isEmpty();
    ui->actionEditPresetsDelete->setEnabled(presetSelected);
    ui->actionEditPresetsMoveUp->setEnabled(presetSelected && presetSel.first()->row() != 0);
}

void MainWindow::on_actionEditMeasurementsSelectColor_triggered()
{
    QList<QTableWidgetItem*> sel = ui->tableWidget_measurments->selectedItems();
    if (sel.isEmpty())
        return;
    int row = sel.first()->row();
    QPen pen = m_swrWidget->graph(row + 1)->pen();
    QColor color = QColorDialog::getColor(pen.color(), this);
    if (color.isValid())
        changeMeasurmentsColor(row, color);
}

void MainWindow::on_actionEditMeasurementsRename_triggered()
{
    QList<QTableWidgetItem*> sel = ui->tableWidget_measurments->selectedItems();
    if (sel.isEmpty())
        return;
    m_measurements->renameMeasurement(sel.first()->row());
}

void MainWindow::on_actionEditMeasurementsSaveAs_triggered()
{
    QList<QTableWidgetItem*> sel = ui->tableWidget_measurments->selectedItems();
    if (sel.isEmpty())
        return;
    exportMeasurementRow(sel.first()->row());
}

void MainWindow::on_actionEditMeasurementsDelete_triggered()
{
    QList<QTableWidgetItem*> sel = ui->tableWidget_measurments->selectedItems();
    if (sel.isEmpty())
        return;
    deleteMeasurementRow(sel.first()->row());
}

void MainWindow::on_actionEditMeasurementsToggleVisibility_triggered()
{
    QList<QTableWidgetItem*> sel = ui->tableWidget_measurments->selectedItems();
    if (sel.isEmpty())
        return;
    toggleMeasurementVisibility(sel.first()->row());
}

void MainWindow::on_actionEditMeasurementsClearAll_triggered()
{
    clearAllMeasurements();
}

void MainWindow::on_actionEditMarkersClearSelected_triggered()
{
    m_markers->markersHint()->clearSelectedMarker();
}

void MainWindow::on_actionEditMarkersClearAll_triggered()
{
    m_markers->markersHint()->clearAllMarkers();
}

void MainWindow::on_actionEditMarkersClearEmpty_triggered()
{
    m_markers->markersHint()->on_clearEmptyMarkers();
}

void MainWindow::on_actionEditPresetsAdd_triggered()
{
    on_presetsAddBtn_clicked();
}

void MainWindow::on_actionEditPresetsDelete_triggered()
{
    QList<QTableWidgetItem*> sel = ui->tableWidget_presets->selectedItems();
    if (sel.isEmpty())
        return;
    m_presets->deleteRow(sel.first()->row());
}

void MainWindow::on_actionEditPresetsMoveUp_triggered()
{
    QList<QTableWidgetItem*> sel = ui->tableWidget_presets->selectedItems();
    if (sel.isEmpty())
        return;
    m_presets->moveRowUp(sel.first()->row());
}
