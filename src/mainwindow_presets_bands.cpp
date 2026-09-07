#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "popupindicator.h"
#include "analyzer/customanalyzer.h"
#include "analyzer/nanovna_analyzer.h"
#include "Notification.h"
#include "glwidget.h"
#include "CustomPlot.h"
#include "selectdevicedialog.h"
#include "printmulti.h"
#include "style.h"
#include "filedialog.h"
#include <QWindow>

extern QString appendSpaces(const QString& number);
extern bool g_usbOnly;
extern int g_maxMeasurements; // see measurements.cpp
extern QMap<QString, QString> g_mapTabPlotNames; // see mainwindow.cpp
extern void setAbsoluteFqMaximum();
extern bool g_bAA55modeNewProtocol;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original mainwindow.cpp (still in
// mainwindow.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of MainWindow.

void MainWindow::on_presetsAddBtn_clicked()
{
    QString from = QString::number(getFqFrom(),'f',0);
    QString to = QString::number(getFqTo(),'f',0);
    m_presets->addNewRow(from, to, QString::number(m_dotsNumber));
}

// Autoconnects to cellActivated (not cellDoubleClicked) so this also fires
// on Return/Enter when a row is current, not just on double-click -- see
// QAbstractItemView::activated().
void MainWindow::on_tableWidget_presets_cellActivated(int row, int column)
{
    Q_UNUSED(column)
    QStringList list = m_presets->getRow(row);
    QCPRange range;
    // Clamped, not the raw stored text -- a preset saved before the
    // Start/Stop clamp existed (or hand-edited into the presets file)
    // could otherwise still hand the plots/device an out-of-range value.
    range.lower = clampFqKhz(list.at(0).toDouble());
    range.upper = clampFqKhz(list.at(1).toDouble());
    setDotsNumber(list.at(2).toInt());

    if(!m_isRange)
    {
        setFqFrom(list.at(0));
        setFqTo(list.at(1));
    }else
    {
        setFqFrom((range.upper + range.lower)/2);
        setFqTo((range.upper - range.lower)/2);
    }
    m_swrWidget->xAxis->setRange(range);
    m_phaseWidget->xAxis->setRange(range);
    m_rsWidget->xAxis->setRange(range);
    m_rpWidget->xAxis->setRange(range);
    m_rlWidget->xAxis->setRange(range);
    m_s21Widget->xAxis->setRange(range);
#if USER_DEFINED_FEATURE
    m_userWidget->xAxis->setRange(range);
#endif
    updateGraph();
}

void MainWindow::on_presetsDeleteBtn_clicked()
{
    QList <QTableWidgetItem *> list = ui->tableWidget_presets->selectedItems();
    if(list.length() > 0)
    {
        QTableWidgetItem * item = list.at(0);
        int rowNumber = item->row();
        m_presets->deleteRow(rowNumber);
    }
}

void MainWindow::on_pressetsUpBtn_clicked()
{
    QList <QTableWidgetItem *> list = ui->tableWidget_presets->selectedItems();
    if(list.length() > 0)
    {
        QTableWidgetItem * item = list.at(0);
        int rowNumber = item->row();
        m_presets->moveRowUp(rowNumber);
    }
}

bool MainWindow::loadBands()
{
    QString ituPath = Settings::localDataPath("itu-regions.txt");
    QFile file(ituPath);
    if (!file.exists()) {
        file.setFileName(Settings::programDataPath("itu-regions-defaults.txt"));
    }

    bool res = file.open(QFile::ReadOnly);
    if(!res)
        return false;

    // Was never cleared before reloading -- insert() on an already-existing
    // region title just replaced the map's pointer without freeing the old
    // QStringList, leaking one per region every reload (harmless-ish at
    // startup, real once Settings' ITU Bands tab's Save can trigger a
    // reload mid-session). A region renamed/removed in the text also used
    // to linger in the map forever instead of actually disappearing.
    qDeleteAll(m_BandsMap);
    m_BandsMap.clear();

    QTextStream stream(&file);
    QString str;
    QStringList* list = nullptr;
    while(!stream.atEnd())
    {
        str = stream.readLine().trimmed();
        if (str.isEmpty())
            continue;
        if (str.indexOf('[') != -1)
        {
            int pos = str.indexOf(']');
            QString title = str.mid(1, pos-1);
            list = new QStringList();
            m_BandsMap.insert(title, list);
        } else if (list != nullptr) {
            list->append(str);
        }
    }
    file.close();
    return true;
}

// Single source of truth for "current_band" -- was read with two different
// hardcoded defaults across 5+ call sites ("ITU Region 1 - Europe, Africa"
// in most; the Band Highlighting menu builder used "", so on a fresh ini
// nothing ever matched and no menu item got checked, even though the
// charts *did* show that first default's bands). Also self-heals a
// current_band that no longer names a real region (renamed/deleted band,
// or the old "issue #21" drift) by falling back to the first region --
// alphabetically first, same order m_BandsMap (a QMap) already iterates
// in -- and persisting that choice immediately instead of silently
// recomputing the same fallback every time. Requires m_BandsMap already
// populated (loadBands() called) by the caller.
QString MainWindow::currentBandRegion()
{
    m_settings->beginGroup("Settings");
    QString band = m_settings->value("current_band", QString()).toString();
    m_settings->endGroup();

    if (!m_BandsMap.contains(band)) {
        band = m_BandsMap.isEmpty() ? QString() : m_BandsMap.firstKey();
        m_settings->beginGroup("Settings");
        m_settings->setValue("current_band", band);
        m_settings->endGroup();
    }
    return band;
}

// View > Band Highlighting: one exclusive/checkable action per region in
// m_BandsMap. Was built once, in the constructor, from a local (non-member)
// QActionGroup -- fine as long as m_BandsMap never changed after that, but
// Settings' ITU Bands tab Save now reloads it mid-session, so this needs
// to be re-callable. ui->menuBandHighlighting->clear() deletes the actions
// (the menu owns them); the group itself isn't owned by the menu, so it's
// deleted and recreated here explicitly.
void MainWindow::rebuildBandHighlightingMenu()
{
    ui->menuBandHighlighting->clear();
    delete m_bandHighlightingGroup;
    m_bandHighlightingGroup = new QActionGroup(this);
    m_bandHighlightingGroup->setExclusive(true);

    QString currentBand = currentBandRegion();
    const QStringList bandNames = m_BandsMap.keys();
    for (const QString& bandName : bandNames) {
        QAction* action = ui->menuBandHighlighting->addAction(bandName);
        action->setCheckable(true);
        action->setChecked(bandName == currentBand);
        m_bandHighlightingGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, bandName]() {
            m_settings->beginGroup("Settings");
            m_settings->setValue("current_band", bandName);
            m_settings->endGroup();
            on_bandChanged(bandName);
        });
    }
}

void MainWindow::on_bandChanged(QString band)
{
    // Was gated behind `if (m_BandsMap.contains(band))` -- skipped this
    // whole clear+redraw block whenever the region wasn't found (including
    // a legitimately empty itu-regions.txt), so whatever bands were
    // already drawn from before just stayed on the charts, never actually
    // clearing. Runs unconditionally now; setBands() already does nothing
    // when passed a null bands list (region not found, or has none).

    // Was a raw `delete` -- QCPAbstractItem::~QCPAbstractItem() never
    // deregisters itself from its QCustomPlot's own mItems list (only
    // registerItem()/removeItem() maintain that; QCustomPlot's own doc
    // comments say as much: "do not delete it manually but use
    // QCustomPlot::removeItem() instead"). Left a dangling entry per
    // item in whichever widget owned it, forever -- registerItem()'s
    // next "item already added" qDebug() (TODO 7b) was that stale
    // bookkeeping surfacing, not a live-item hazard. parentPlot() is
    // whichever of swr/phase/rs/rp/rl/user actually owns this one item
    // (m_itemRectList spans all of them), so removeItem() has to be
    // called per-item, not once per widget.
    while (!m_itemRectList.isEmpty()) {
        QCPAbstractItem* item = m_itemRectList.takeFirst();
        item->parentPlot()->removeItem(item);
    }

    QStringList* bands = m_BandsMap.value(band, nullptr);
    setBands(m_swrWidget, bands, MIN_SWR, MAX_SWR);
    setBands(m_phaseWidget, bands, -190, 190); // see setWidgetsSettings()'s comment (issue #4)
    setBands(m_rsWidget, bands, -5000, 5000); // see setWidgetsSettings()'s comment (issue #5)
    setBands(m_rpWidget, bands, -5000, 5000);
    setBands(m_rlWidget, bands, 0, 50);
#if USER_DEFINED_FEATURE
    setBands(m_userWidget, bands, MIN_USER_RANGE, MAX_USER_RANGE);
#endif
    // setBands()/addBand() only build/position the new items -- nothing
    // in that path replots, so the chart kept showing the *old* bands
    // (or none, right after a raw-delete clear) until some unrelated
    // interaction (e.g. a mouse move) happened to trigger QCustomPlot's
    // own repaint (TODO 7a).
    m_swrWidget->replot();
    m_phaseWidget->replot();
    m_rsWidget->replot();
    m_rpWidget->replot();
    m_rlWidget->replot();
#if USER_DEFINED_FEATURE
    m_userWidget->replot();
#endif

    // Independent of whether the region was found above: keep the Presets
    // band-selector combo (the per-band, not per-region, dropdown) in sync
    // with whatever region is actually active now.
    populateBandSelector(band);
}

void MainWindow::populateBandSelector(const QString& band)
{
    ui->presetsBandComboBox->blockSignals(true);
    ui->presetsBandComboBox->clear();
    ui->presetsBandComboBox->addItem(tr("Select a band"));

    QStringList* bands = m_BandsMap.value(band, nullptr);
    if (bands != nullptr) {
        foreach (const QString& line, *bands) {
            QStringList fields = line.split(',');
            if (fields.size() != 3)
                continue; // custom 2-field entry (see Settings' ITU Bands tab) -- no name to label it with
            QString name = fields[2].trimmed();
            if (name.isEmpty())
                continue;
            QString start = fields[0].trimmed();
            QString stop = fields[1].trimmed();
            ui->presetsBandComboBox->addItem(
                tr("%1 (%2 - %3 kHz)").arg(name, start, stop),
                QVariant(QStringList{start, stop}));
        }
    }
    ui->presetsBandComboBox->setCurrentIndex(0);
    ui->presetsBandComboBox->blockSignals(false);

    // Visibility is not this function's concern -- it's decided once, by
    // whoever owns "band-selector-enabled" (MainWindow's constructor and
    // actionBandSelector's toggle handler, mainwindow.cpp). A region with
    // no labeled bands still shows the selector, just with nothing but
    // "Select a band" in it -- that's fine, not an error.
}

void MainWindow::on_presetsBandComboBox_currentIndexChanged(int index)
{
    if (index <= 0)
        return;

    QStringList range = ui->presetsBandComboBox->itemData(index).toStringList();
    if (range.size() == 2) {
        // Clamped, not the raw itu-regions text -- a hand-edited
        // itu-regions.txt (see Settings' ITU Bands tab) could otherwise
        // still hand the plots an out-of-device-range value.
        double start = clampFqKhz(range.at(0).toDouble());
        double stop = clampFqKhz(range.at(1).toDouble());

        if (!m_isRange) {
            setFqFrom(range.at(0));
            setFqTo(range.at(1));
        } else {
            setFqFrom((start + stop) / 2);
            setFqTo((stop - start) / 2);
        }

        // Single range-then-redraw pass across every plot, same as picking
        // a preset row (on_tableWidget_presets_cellActivated) -- one
        // updateGraph() call at the end, not one per field.
        QCPRange plotRange(start, stop);
        m_swrWidget->xAxis->setRange(plotRange);
        m_phaseWidget->xAxis->setRange(plotRange);
        m_rsWidget->xAxis->setRange(plotRange);
        m_rpWidget->xAxis->setRange(plotRange);
        m_rlWidget->xAxis->setRange(plotRange);
        m_s21Widget->xAxis->setRange(plotRange);
#if USER_DEFINED_FEATURE
        m_userWidget->xAxis->setRange(plotRange);
#endif
        updateGraph();
    }

    ui->presetsBandComboBox->blockSignals(true);
    ui->presetsBandComboBox->setCurrentIndex(0);
    ui->presetsBandComboBox->blockSignals(false);
}


