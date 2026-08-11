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

void MainWindow::on_exportBtn_clicked()
{
    QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
    if(!list.isEmpty())
    {
        QTableWidgetItem * item = list.at(0);
        m_exportDialog = new Export(this);
        m_exportDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_exportDialog->setWindowTitle(tr("Export"));
        m_exportDialog->setMeasurements(m_measurements, item->row());
        m_exportDialog->exec();
    }
}

void MainWindow::on_measurmentsDeleteBtn_clicked()
{
    if(m_analyzer->isMeasuring())
    {
        return;
    }
    int columns = ui->tableWidget_measurments->columnCount();
    QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
    for(int i = 0; i < list.length(); i+=columns)
    {
        QTableWidgetItem * item = list.at(i);
        int rowNumber = item->row();
        m_measurements->deleteRow(rowNumber);
    }

    if(ui->tableWidget_measurments->rowCount() == 0)
    {
        //onFullRange(true);
        //{ Fedoseev's request 2022-11-11
        //qint64 from = m_lastEnteredFqFrom;
        //qint64 to =  m_lastEnteredFqTo;
        //qint64 range = (to - from);
        //on_dataChanged(from + range/2, range, m_dotsNumber);
        //}
        ui->measurmentsSaveBtn->setEnabled(false);
        ui->measurmentsDeleteBtn->setEnabled(false);
        ui->measurmentsClearBtn->setEnabled(false);
        ui->exportBtn->setEnabled(false);
    }
    else
    {
        on_tableWidget_measurments_cellClicked(ui->tableWidget_measurments->rowCount()-1, 0);
    }
    if(m_markers)
    {
        m_markers->changeMarkersHint();
        m_markers->redraw();
    }
    m_measurements->replot();
}


void MainWindow::measurementsClearBtn_clicked(bool)
{
    if(m_analyzer->isMeasuring())
    {
        return;
    }

    m_measurements->on_measurementComplete();
    while(ui->tableWidget_measurments->rowCount() != 0)
    {
        QTableWidgetItem * item = ui->tableWidget_measurments->item(0, 0);
        int rowNumber = item->row();
        m_measurements->deleteRow(rowNumber);
    }

    //{ Antonov's request: keep user's values
    //onFullRange(true);
    double from = m_lastEnteredFqFrom;
    double to =  m_lastEnteredFqTo;
    AnalyzerParameters::normalizeFq(from, to);
    //{ Fedoseev's request 2022-11-11
    //qint64 range = (to - from);
    //on_dataChanged(from + range/2, range, m_dotsNumber);
    //}
    //}

    if(ui->tableWidget_measurments->rowCount() == 0)
    {
        ui->measurmentsSaveBtn->setEnabled(false);
        ui->measurmentsDeleteBtn->setEnabled(false);
        ui->measurmentsClearBtn->setEnabled(false);
        ui->exportBtn->setEnabled(false);
    }
    if(m_markers)
    {
        m_markers->changeMarkersHint();
        m_markers->redraw();
    }
    m_measurements->replot();
}

void MainWindow::on_tableWidget_measurments_cellClicked(int row, int column)
{
    Q_UNUSED(column)
    int count = m_swrWidget->graphCount();

    if(count > 0)
    {
        for(int i = 1; i < count; ++i)
        {
            int pen_width = ((i-1) == row) ? ACTIVE_GRAPH_PEN_WIDTH : INACTIVE_GRAPH_PEN_WIDTH;
            int j = (i-1)*3 + 1;
            {
                QPen pen = m_swrWidget->graph(i)->pen();
                pen.setWidth(pen_width);
                m_swrWidget->graph(i)->setPen(pen);
                m_phaseWidget->graph(i)->setPen(pen);
                m_rlWidget->graph(i)->setPen(pen);
                m_s21Widget->graph(i)->setPen(pen);
                m_measurements->getMeasurement(count - 2 - (i-1))->smithCurve->setPen(pen);

                for (int ii=0; ii<3; ii++) {
                    pen = m_rpWidget->graph(j+ii)->pen();
                    pen.setWidth(pen_width);
                    m_rpWidget->graph(j+ii)->setPen(pen);

                    pen = m_rsWidget->graph(j+ii)->pen();
                    pen.setWidth(pen_width);
                    m_rsWidget->graph(j+ii)->setPen(pen);

                    pen = m_tdrWidget->graph(j+ii)->pen();
                    pen.setWidth(pen_width);
                    m_tdrWidget->graph(j+ii)->setPen(pen);
                }

                if (g_developerMode) {
                    measurement* mm = m_measurements->getMeasurement(count - i-1);
                    int index = m_measurements->getBaseUserGraphIndex(i-1);
                    int cnt = mm->userGraphs.size();
                    for (int ii=0; ii<cnt; ii++) {
                        pen = m_userWidget->graph(index + ii)->pen();
                        pen.setWidth(pen_width);
                        m_userWidget->graph(index + ii)->setPen(pen);
                    }
                }
            }
        }
        updateGraph();
    }
}

void MainWindow::on_tableWidget_measurments_cellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    if (m_measurements->isEmpty())
        return;
    qint32 count = m_measurements->getMeasurementLength();
    measurement* mm = m_measurements->getMeasurement(count - row - 1);
    if (!mm->visible)
        return;
    qint64 from = mm->qint64From/1000;
    qint64 to = mm->qint64To/1000;
    double sw = (to-from)/2.0;
    double center = from + sw;
    if(!m_isRange)
    {
        setFqFrom(from);
        setFqTo(to);
    }else
    {
        setFqFrom(center);
        setFqTo(sw);
    }
    QCPRange range;
    range.lower = from;
    range.upper = to;

    m_swrWidget->xAxis->setRange(range);
    m_phaseWidget->xAxis->setRange(range);
    m_rsWidget->xAxis->setRange(range);
    m_rpWidget->xAxis->setRange(range);
    m_rlWidget->xAxis->setRange(range);
    m_s21Widget->xAxis->setRange(range);
    if (g_developerMode)
        m_userWidget->xAxis->setRange(range);

    QString str = ui->tabWidget->currentWidget()->objectName();
    if (str == "tab_tdr") {
        int dist = m_measurements->calcTdrDist(&mm->dataRX);
        if (dist != 0) {
            range.lower = 0;
            range.upper = dist;
            QWidget::setCursor(Qt::WaitCursor);
            m_measurements->redrawTDR(row);
            m_tdrZRange = m_measurements[row].tdrZRange();
            QWidget::setCursor(Qt::ArrowCursor);
        }
    }

    updateGraph();
}

void MainWindow::on_screenshot_clicked()
{
    QString path = m_lastScreenshotPath;
    if (path.isEmpty()) {
        QDateTime datetime = QDateTime::currentDateTime();
        path = "Images/" + datetime.toString("dd.MM.yyyy_hh.mm.ss");
    }
    QString str = FileDialog::getSaveFileName(this, tr("Export PNG"), path, "*.png");
    if(str.isEmpty())
    {
        return;
    }
    if(str.indexOf(".png") == -1)
    {
        str += ".png";
    }
    m_lastScreenshotPath = str;

    on_pressCtrlC();
    QPixmap pixmap = QApplication::clipboard()->pixmap();
    if (!pixmap.isNull()) {
        pixmap.save(str, "PNG", 80);
    }
}

void MainWindow::on_printBtn_clicked()
{
    QString name = ui->tabWidget->currentWidget()->objectName();
    if (name == "tab_multi") {
        return;
        //m_print = new Printmulti(m_multiTabData.tabs);
    } else {
        m_print = new Print();
    }
    m_print->setAttribute(Qt::WA_DeleteOnClose);
    m_print->updateMarkers(m_markers->getMarkersCount(), m_measurements->getMeasurementLength(),
                           m_markers->updateInfo(m_markers->markersHint()->getColumns()));

    m_settings->beginGroup("Settings");
    QString band = m_settings->value("current_band", "ITU Region 1 - Europe, Africa").toString();
    m_settings->endGroup();
    QStringList* bands = nullptr;
    if (m_BandsMap.contains(band))
    {
        bands = m_BandsMap[band];
    }

    AnalyzerParameters* param = AnalyzerParameters::current();
    QString model = CustomAnalyzer::customized() ?
                CustomAnalyzer::currentPrototype() : (param == nullptr ? "" : param->name());
    QString string;
    if (!model.isEmpty())
        string += model + ", ";
    QDateTime datetime = QDateTime::currentDateTime();
    string += datetime.toString("dd.MM.yyyy-hh:mm, ");

    if(name == "tab_swr")
    {
        m_print->setName("SWR");
        string += tr("SWR graph");
        m_print->drawBands(bands, MIN_SWR, MAX_SWR);
        //m_print->setRange(m_swrWidget->xAxis->range(),m_swrWidget->yAxis->range());
        m_print->setRange(m_swrWidget);
        m_print->setLabel(m_swrWidget->xAxis->label(), m_swrWidget->yAxis->label());
        int cnt = m_swrWidget->graphCount();
        for(int i = 1; i < cnt; ++i)
        {
            QModelIndex myIndex = ui->tableWidget_measurments->model()->
                    index( i-1, COL_NAME, QModelIndex());
            if (m_swrWidget->graph(i)->visible()) {
                QPen pen = m_swrWidget->graph(i)->pen();
                pen.setWidth(INACTIVE_GRAPH_PEN_WIDTH);
                m_print->setData(m_swrWidget->graph(i)->data(), pen, myIndex.data().toString());
            }
        }
    }else if(name == "tab_phase")
    {
        string += tr("Phase graph");
        m_print->drawBands(bands, m_phaseWidget->yAxis->range().lower, m_phaseWidget->yAxis->range().upper);
        //m_print->setRange(m_phaseWidget->xAxis->range(),m_phaseWidget->yAxis->range());
        m_print->setRange(m_phaseWidget);
        m_print->setLabel(m_phaseWidget->xAxis->label(), m_phaseWidget->yAxis->label());
        for(int i = 1; i < m_phaseWidget->graphCount(); ++i)
        {
            QModelIndex myIndex = ui->tableWidget_measurments->model()->
                    index( i-1, COL_NAME, QModelIndex());
            if (m_phaseWidget->graph(i)->visible()) {
                QPen pen = m_phaseWidget->graph(i)->pen();
                pen.setWidth(INACTIVE_GRAPH_PEN_WIDTH);
                m_print->setData(m_phaseWidget->graph(i)->data(), pen, myIndex.data().toString());
            }
        }
    }else if(name == "tab_rs")
    {
        string += tr("RXZ graph");
        m_print->drawBands(bands, m_rsWidget->yAxis->range().lower, m_rsWidget->yAxis->range().upper);
        //m_print->setRange(m_rsWidget->xAxis->range(),m_rsWidget->yAxis->range());
        m_print->setRange(m_rsWidget);
        QCPRange rr(0, 230000);
        m_print->setRange_yAxis2(rr);
        m_print->setLabel(m_rsWidget->xAxis->label(), m_rsWidget->yAxis->label());
        for(int i = 1; i < m_rsWidget->graphCount(); ++i)
        {
            if (m_rsWidget->graph(i)->visible()) {
                QPen pen = m_rsWidget->graph(i)->pen();
                pen.setWidth(INACTIVE_GRAPH_PEN_WIDTH);
                m_print->setData(m_rsWidget->graph(i)->data(), pen, m_rsWidget->graph(i)->name());
            }
        }
    }else if(name == "tab_rp")
    {
        string += tr("RXZ parallel graph");
        m_print->drawBands(bands, m_rpWidget->yAxis->range().lower, m_rpWidget->yAxis->range().upper);
        //m_print->setRange(m_rpWidget->xAxis->range(),m_rpWidget->yAxis->range());
        m_print->setRange(m_rpWidget);
        QCPRange rr(0, 230000);
        m_print->setRange_yAxis2(rr);
        m_print->setLabel(m_rpWidget->xAxis->label(), m_rpWidget->yAxis->label());
        for(int i = 1; i < m_rpWidget->graphCount(); ++i)
        {
            if (m_rpWidget->graph(i)->visible()) {
                QPen pen = m_rpWidget->graph(i)->pen();
                pen.setWidth(INACTIVE_GRAPH_PEN_WIDTH);
                m_print->setData(m_rpWidget->graph(i)->data(), pen, m_rpWidget->graph(i)->name());
            }
        }
    }else if(name == "tab_rl")
    {
        string += tr("RL graph");
        m_print->drawBands(bands, m_rlWidget->yAxis->range().lower, m_rlWidget->yAxis->range().upper);
        //m_print->setRange(m_rlWidget->xAxis->range(),m_rlWidget->yAxis->range());
        m_print->setRange(m_rlWidget);
        m_print->setLabel(m_rlWidget->xAxis->label(), m_rlWidget->yAxis->label());
        for(int i = 1; i < m_rlWidget->graphCount(); ++i)
        {
            QModelIndex myIndex = ui->tableWidget_measurments->model()->
                                  index( i-1, COL_NAME, QModelIndex());
            if (m_rlWidget->graph(i)->visible()) {
                QPen pen = m_rlWidget->graph(i)->pen();
                pen.setWidth(INACTIVE_GRAPH_PEN_WIDTH);
                m_print->setData(m_rlWidget->graph(i)->data(), pen, myIndex.data().toString());
            }
        }
    }else if(name == "tab_s21")
    {
        string += tr("S21 graph");
        m_print->drawBands(bands, m_s21Widget->yAxis->range().lower, m_s21Widget->yAxis->range().upper);
        m_print->setRange(m_s21Widget);
        m_print->setLabel(m_s21Widget->xAxis->label(), m_s21Widget->yAxis->label());
        for(int i = 1; i < m_s21Widget->graphCount(); ++i)
        {
            QModelIndex myIndex = ui->tableWidget_measurments->model()->
                                  index( i-1, COL_NAME, QModelIndex());
            if (m_s21Widget->graph(i)->visible()) {
                QPen pen = m_s21Widget->graph(i)->pen();
                pen.setWidth(INACTIVE_GRAPH_PEN_WIDTH);
                m_print->setData(m_s21Widget->graph(i)->data(), pen, myIndex.data().toString());
            }
        }
    }else if(name == "tab_tdr")
    {
        m_print->setName("TDR");
        string += tr("TDR graph");
        m_print->drawBands(bands, m_tdrWidget->yAxis->range().lower, m_tdrWidget->yAxis->range().upper);
        //m_print->setRange(m_tdrWidget->xAxis->range(),m_tdrWidget->yAxis->range());
        m_print->setRange(m_tdrWidget);
        QCPRange rr(0, m_tdrZRange);
        m_print->setRange_yAxis2(rr);
        m_print->setLabel(m_tdrWidget->xAxis->label(), m_tdrWidget->yAxis->label());
        for(int i = 1; i < m_tdrWidget->graphCount(); ++i)
        {
            //int i = 3;
            if (m_tdrWidget->graph(i)->visible()) {
                QPen pen = m_tdrWidget->graph(i)->pen();
                pen.setWidth(INACTIVE_GRAPH_PEN_WIDTH);
                m_print->setData(m_tdrWidget->graph(i)->data(), pen, m_tdrWidget->graph(i)->name());
            }
        }
    }else if(name == "tab_smith")
    {
        string += tr("Smith graph");
        m_print->drawSmithImage();
        m_print->setRange(m_smithWidget);
        m_print->setLabel(m_smithWidget->xAxis->label(), m_smithWidget->yAxis->label());
        QTimer::singleShot(1, this, [this]() { // fix elipse smith bug
            QSize sz = m_print->size();
            sz.rwidth() += 1;
            m_print->resize(sz);
            sz.rwidth() -= 1;
            m_print->resize(sz);
        });
        for(int i = 0; i < m_measurements->getMeasurementLength(); ++i)
        {
            QModelIndex myIndex = ui->tableWidget_measurments->model()->
                                index( m_smithWidget->graphCount()-i-1, COL_NAME, QModelIndex());
            measurement& mm = *m_measurements->getMeasurement(i);
            if (mm.smithCurve->visible()) {
                QPen pen = m_measurements->getMeasurement(i)->smithCurve->pen();
                pen.setWidth(INACTIVE_GRAPH_PEN_WIDTH);
                m_print->setSmithData(&m_measurements->getMeasurement(i)->smithGraph, pen, myIndex.data().toString());
            }
        }
    }else if(name == "tab_user")
    {
        string += tr("User defined");
        m_print->drawBands(bands, m_userWidget->yAxis->range().lower, m_userWidget->yAxis->range().upper);
        //m_print->setRange(m_userWidget->xAxis->range(),m_userWidget->yAxis->range());
        m_print->setRange(m_userWidget);
        m_print->setLabel(m_userWidget->xAxis->label(), m_userWidget->yAxis->label());
        for(int i = 1; i < m_userWidget->graphCount(); ++i)
        {
            m_print->setData(m_userWidget->graph(i)->data(), m_userWidget->graph(i)->pen(), m_userWidget->graph(i)->name());
        }
    }
#ifndef NO_MULTITAB
    else if(name == "tab_multi") {
        string += tr("Multi");
        foreach (auto tab, m_multiTabData.tabs) {
            QCustomPlot* plot = plotForTab(tab);
            if (plot != nullptr) {
                m_print->drawBands(bands, plot->yAxis->range().lower, plot->yAxis->range().upper);
                ((Printmulti*)m_print)->setRange(tab, plot);
                m_print->setLabel(plot->xAxis->label(), plot->yAxis->label());
                for(int i = 1; i < plot->graphCount(); ++i)
                {
                    QPen pen = plot->graph(i)->pen();
                    pen.setWidth(INACTIVE_GRAPH_PEN_WIDTH);
                    ((Printmulti*)m_print)->setData(tab, plot->graph(i)->data(), pen, plot->graph(i)->name());
                }
            }
        }
    }
#endif
    if(name != "tab_smith")
    {
        qint32 markersCount = m_markers->getMarkersCount();
        for(int i = 0; i < markersCount; ++i)
        {
            m_print->addMarker(m_markers->getMarker(i).frequency, i+1);
        }
    }

    m_print->setHead(string);

    m_print->exec();
}

void MainWindow::on_measurmentsSaveBtn_clicked()
{
    QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();

    if(!list.isEmpty())
    {
        m_lastSaveOpenPath = FileDialog::withExtension(m_lastSaveOpenPath, "asd");

        int row = list.at(0)->row();

        // Suggest the measurement's own name (minus its "NN> " auto-numbering
        // prefix, with filesystem-unsafe characters swapped for "_" -- the
        // rename dialog, Measurements::setupUi()'s QInputDialog handler,
        // accepts any text at all, including "/") as the filename, in the
        // same folder as the last save/open, instead of just reusing
        // whatever filename happened to be typed last time.
        QString suggestedPath = m_lastSaveOpenPath;
        measurement* selectedMm = m_measurements->getMeasurement(m_measurements->getMeasurementLength()-row-1);
        if (selectedMm != nullptr) {
            QString suggestedName = selectedMm->name;
            int namePos = suggestedName.indexOf("> ");
            if (namePos != -1)
                suggestedName = suggestedName.mid(namePos+2);
            suggestedName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
            suggestedName = suggestedName.trimmed();
            if (!suggestedName.isEmpty()) {
                QString dir = QFileInfo(m_lastSaveOpenPath).path();
                suggestedPath = (dir.isEmpty() || dir == ".") ? (suggestedName + ".asd") : (dir + "/" + suggestedName + ".asd");
            }
        }

        // The filter string must exactly match "*.asd" -- a stray trailing
        // space before the closing paren here used to make QFileDialog's
        // own "does the typed name already satisfy the filter" check fail
        // for every normal ".asd" name, so it appended ".asd" again on top
        // of the one suggestedPath/suggestedName already added above,
        // producing "...asd.asd" (issue reported 2026-08-10).
        QString path = FileDialog::getSaveFileName(this, tr("Save file"), suggestedPath, "AntScopeZ (*.asd)");
        if(!path.isEmpty())
        {
            m_lastSaveOpenPath = path;
            m_measurements->saveData(row, path);
            QFileInfo fi(path);
            QString fname = fi.baseName();

            measurement* mm = m_measurements->getMeasurement(m_measurements->getMeasurementLength()-row-1);
            QString mmName = mm->name;
            int pos = mmName.indexOf("> ");
            if (pos != -1)
                mmName = mmName.left(pos+2);
            mm->name = mmName + fname;
            ui->tableWidget_measurments->setColumnWidth(COL_NAME, COL_NAME_WD);

            QTableWidgetItem* itm = ui->tableWidget_measurments->item(row, COL_NAME);
            QFontMetrics fm(itm->font());
            int width = COL_NAME_WD;
            QString elided = fm.elidedText(mm->name, Qt::ElideRight, width);
            ui->tableWidget_measurments->item(row, COL_NAME)->setText(elided);
            ui->tableWidget_measurments->resizeColumnToContents(COL_NAME);

        }
    }
}

void MainWindow::on_measurementsOpenBtn_clicked()
{
    QString path = FileDialog::getOpenFileName(this, tr("Open file"), m_lastSaveOpenPath, "AntScopeZ (*.asd)");
    if(!path.isEmpty())
    {
        m_lastSaveOpenPath = path;

        m_measurements->loadData( path );
        ui->measurmentsSaveBtn->setEnabled(true);
        ui->exportBtn->setEnabled(true);
        ui->measurmentsDeleteBtn->setEnabled(true);
        ui->measurmentsClearBtn->setEnabled(true);
    }
}

void MainWindow::openFile(QString path)
{
    m_measurements->loadData(path);
    ui->measurmentsSaveBtn->setEnabled(true);
    ui->exportBtn->setEnabled(true);
    ui->measurmentsDeleteBtn->setEnabled(true);
    ui->measurmentsClearBtn->setEnabled(true);
}

void MainWindow::on_importBtn_clicked()
{
    if (m_lastExportImportPath.isEmpty()) {
        m_settings->beginGroup("Export");
        m_lastExportImportPath = m_settings->value("lastExportPath", "").toString();
        m_settings->endGroup();
    }
    if (m_lastExportImportPath.isEmpty()) {
        m_lastExportImportPath = m_lastSaveOpenPath;
    }

    QString path = FileDialog::getOpenFileName(this, tr("Open file"), m_lastExportImportPath,  "S1p (*.s1p);;"
                                                                                    "Csv (*.csv);;"
                                                                                    "Nwl (*.nwl);;"
                                                                                    "AntScopeZ (*.asd);;"

                                                                                    "All files (*.*)");
    if (path.isEmpty())
        return;

    m_measurements->loadData(path);
    ui->measurmentsSaveBtn->setEnabled(true);
    ui->exportBtn->setEnabled(true);
    ui->measurmentsDeleteBtn->setEnabled(true);
    ui->measurmentsClearBtn->setEnabled(true);
    m_lastExportImportPath = path;
}

void MainWindow::on_SaveFile(int row, QString path)
{
    //int row = ui->tableWidget_measurments->rowCount() - 1;
    saveFile(row, path);
    ui->measurmentsSaveBtn->setEnabled(true);
}

void MainWindow::saveFile(int row, QString path)
{
    m_measurements->saveData(row, path);
}

void MainWindow::on_importFinished(double _fqMin_khz, double _fqMax_khz)
{
    double _range = (_fqMax_khz - _fqMin_khz);
    double _center = (_fqMin_khz + _range / 2);

//    measurement* mm = m_measurements->getMeasurement(m_measurements->getMeasurementLength() - 1);
//    if (mm != nullptr)
//        mm->set(_center*1000, _range*1000, mm->dataRX.size()-1);

    on_dataChanged((qint64)_center, (qint64)_range/2, ui->spinBoxPoints->value());

    ui->measurmentsSaveBtn->setEnabled(true);
    ui->exportBtn->setEnabled(true);
    ui->measurmentsDeleteBtn->setEnabled(true);
    ui->measurmentsClearBtn->setEnabled(true);

}

QString appendSpaces(const QString& str) {
    // Thousands-group only the integer part. The naive "group every 3
    // characters counting from the end of the whole string" this used to
    // do treated the decimal point and fractional digits as just more
    // characters to count, so e.g. "135.7" (a valid band edge -- see
    // itu-regions-defaults.txt's 2200m entry) came out as "13 5.7" instead
    // of untouched, and anything with more fractional digits than that
    // came out actively wrong rather than just unnecessarily grouped.
    int dotIdx = str.indexOf('.');
    QString intPart = (dotIdx == -1) ? str : str.left(dotIdx);
    QString fracPart = (dotIdx == -1) ? QString() : str.mid(dotIdx); // includes the '.'

    QString tmp;
    int len = intPart.length();
    for (int idx=0; idx<len; idx++) {
        if (idx != 0 && (idx % 3) == 0)
            tmp.insert(0, ' ');
        tmp.insert(0, intPart[len - 1 - idx]);
    }
    return tmp + fracPart;
}

void MainWindow::on_tableWidgetMeasurmentsContextMenu(const QPoint& pos)
{
    QTableWidgetItem *item = ui->tableWidget_measurments->itemAt(pos);
    if (item != nullptr)
    {
        int row = item->row();
        QPen pen = m_swrWidget->graph(row+1)->pen();
        QColor color = QColorDialog::getColor(pen.color(), this );
        if( color.isValid() )
        {
            changeMeasurmentsColor(row, color);
        }
    }
}

void MainWindow::changeMeasurmentsColor(int _row, QColor& _color)
{
    int count = m_swrWidget->graphCount();
    if(count > 0)
    {
        int i=_row+1;
        QPen pen = m_swrWidget->graph(i)->pen();
        pen.setColor(_color);
        m_swrWidget->graph(i)->setPen(pen);
        m_phaseWidget->graph(i)->setPen(pen);
        m_rlWidget->graph(i)->setPen(pen);
        m_measurements->getMeasurement(count - 2 - (i-1))->smithCurve->setPen(pen);
        updateGraph();
    }
}

