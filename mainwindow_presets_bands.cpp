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
extern bool g_developerMode; // see main.cpp
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
    if (g_developerMode)
        m_userWidget->xAxis->setRange(range);
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

void MainWindow::on_bandChanged(QString band)
{
    if (m_BandsMap.contains(band))
    {
        while (!m_itemRectList.isEmpty()) {
            delete m_itemRectList.takeFirst();
        }

        QStringList* bands = m_BandsMap[band];
        setBands(m_swrWidget, bands, MIN_SWR, MAX_SWR);
        setBands(m_phaseWidget, bands, -180, 180);
        setBands(m_rsWidget, bands, -2000, 2000);
        setBands(m_rpWidget, bands, -2000, 2000);
        setBands(m_rlWidget, bands, 0, 50);
        if (g_developerMode)
            setBands(m_userWidget, bands, MIN_USER_RANGE, MAX_USER_RANGE);
    }

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
                continue; // custom 2-field entry (see EditBandsDialog) -- no name to label it with
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

    // First-run default: seed "enabled" from whether this region actually
    // has any labeled bands, so a fresh install shows a working control
    // instead of an enabled-but-empty one. Once a value exists (user choice
    // or a prior seed), later region switches never touch it again -- an
    // enabled selector with nothing but "Select a band" in it (e.g. after
    // switching to a region with no named bands) is fine, not an error.
    m_settings->beginGroup("Settings");
    if (!m_settings->contains("band-selector-enabled")) {
        m_settings->setValue("band-selector-enabled", ui->presetsBandComboBox->count() > 1);
    }
    bool enabled = m_settings->value("band-selector-enabled", false).toBool();
    m_settings->endGroup();

    ui->presetsBandComboBox->setVisible(enabled);
}

void MainWindow::on_presetsBandComboBox_currentIndexChanged(int index)
{
    if (index <= 0)
        return;

    QStringList range = ui->presetsBandComboBox->itemData(index).toStringList();
    if (range.size() == 2) {
        // Clamped, not the raw itu-regions text -- a hand-edited
        // itu-regions.txt (see EditBandsDialog) could otherwise still hand
        // the plots an out-of-device-range value.
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
        if (g_developerMode)
            m_userWidget->xAxis->setRange(plotRange);
        updateGraph();
    }

    ui->presetsBandComboBox->blockSignals(true);
    ui->presetsBandComboBox->setCurrentIndex(0);
    ui->presetsBandComboBox->blockSignals(false);
}


