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

void MainWindow::on_singleStart_clicked()
{
    m_measurements->setContinuous(false);

    if (isMeasuring())
    {
        m_bInterrupted = true;
        emit stopMeasure();
        ui->singleStart->setChecked(false);
        ui->fullBtn->setEnabled(true);
        ui->fullBtn->setChecked(true);
        ui->continuousStartBtn->setChecked(false);
        if (g_developerMode) {
            m_measurements->hideOneFqWidget();
        }
        return;
    }

    ui->singleStart->setChecked(true);
    ui->continuousStartBtn->setChecked(false);
    ui->fullBtn->setEnabled(false);
    ui->fullBtn->setChecked(false);

    //if (g_developerMode)
    {
        ui->singleStart->setChecked(true);
        quint64 fqFrom = ui->lineEdit_fqFrom->text().remove(' ').toLongLong();
        quint64 fqTo = ui->lineEdit_fqTo->text().remove(' ').toLongLong();
        bool oneFq = m_isRange ? (fqTo==0) : (fqTo==fqFrom);
        if (oneFq) {
            on_startOneFq(fqFrom, m_dotsNumber);
            return;
        }
    }


    double start;
    double stop;

    AnalyzerParameters* param = AnalyzerParameters::current();
    qint64 minFreq = param == nullptr ? 100 : param->minFq().toULongLong();
    qint64 maxFreq = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toULongLong();

    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr) {
            QString strMin = ca->minFq();
            minFreq = strMin.toULongLong();
            QString strMax = ca->maxFq();
            maxFreq = strMax.toULongLong();
        }
        getEnteredFq(start, stop);
    } else {
        getEnteredFq(start, stop);
        if (m_fqRestrict)
        {
            AnalyzerParameters::normalizeFq(start, stop);
            if(!m_isRange)
            {
                setFqFrom(start);
                setFqTo(stop);
            }else
            {
                setFqTo((stop-start)/2);
                setFqFrom((stop+start)/2);
            }
        }
    }

    if(m_fqRestrict && (stop > static_cast<double>(maxFreq)))
    {
        stop = maxFreq;
        if(!m_isRange)
        {
            setFqFrom(start);
            setFqTo(stop);
        }else
        {
            setFqTo((stop-start)/2);
            setFqFrom((stop+start)/2);
        }
    }
    if (m_fqRestrict) {
        if((start > static_cast<double>(maxFreq)) || (start < static_cast<double>(minFreq)))
        {
            start = minFreq;
            if(!m_isRange)
            {
                setFqFrom(start);
            }else
            {
                setFqTo((stop-start)/2);
                setFqFrom((stop+start)/2);
            }
        }
    }
    QCPRange range(start, stop);
    m_swrWidget->xAxis->setRange(range);
    m_phaseWidget->xAxis->setRange(range);
    m_rsWidget->xAxis->setRange(range);
    m_rpWidget->xAxis->setRange(range);
    m_rlWidget->xAxis->setRange(range);
    m_s21Widget->xAxis->setRange(range);
    if (g_developerMode) {
        m_userWidget->xAxis->setRange(range);
    }
    m_settings->beginGroup("MainWindow");
    if (!m_isRange) {
        m_settings->setValue("rangeLower", start);
        m_settings->setValue("rangeUpper", stop);
    } else {
        m_settings->setValue("rangeLower", (stop-start)/2);
        m_settings->setValue("rangeUpper", (stop+start)/2);
    }
    m_settings->setValue("dotsNumber", m_dotsNumber);
    m_settings->endGroup();

    if(ui->tabWidget->currentWidget()->objectName() == "tab_tdr")
    {
        AnalyzerParameters* param = AnalyzerParameters::current();
        qint64 minFq_ = param == nullptr ? 100 : param->minFq().toULongLong()*1000;
        qint64 maxFq_ = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toULongLong()*1000;
        if (CustomAnalyzer::customized()) {
            CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
            if (ca != nullptr) {
                minFq_ = ca->minFq().toULongLong();
                maxFq_ = ca->maxFq().toULongLong();
            }
        }
        int dots = m_dotsNumber;
        if(dots < TDR_MINPOINTS)
            dots = TDR_MINPOINTS;
        if(dots > TDR_MAXPOINTS)
            dots = TDR_MAXPOINTS;

        emit measure(minFq_, maxFq_, dots);
        m_measurements->startTDRProgress(this, dots);
    }
    else if(ui->tabWidget->currentWidget()->objectName() == "tab_user")
    {
        emit measureUser(start*1000, stop*1000, m_dotsNumber);
    }
    else if(ui->tabWidget->currentWidget()->objectName() == "tab_s21")
    {
        emit measureS21(start*1000, stop*1000, m_dotsNumber);
    }
    else
    {
        emit measure(start*1000, stop*1000, m_dotsNumber);
    }
    ui->measurmentsSaveBtn->setEnabled(true);
    ui->actionExport->setEnabled(true);
    ui->measurmentsDeleteBtn->setEnabled(!m_analyzer->isMeasuring());
    ui->measurmentsClearBtn->setEnabled(!m_analyzer->isMeasuring());

    dtStartMeasurement = QDateTime::currentDateTime();
}

void MainWindow::on_continuousStartBtn_clicked(bool checked)
{
    if (isMeasuring())
    {
        m_bInterrupted = true;
        emit stopMeasure();
        ui->singleStart->setChecked(false);
        ui->continuousStartBtn->setChecked(false);
        ui->fullBtn->setEnabled(true);
        ui->fullBtn->setChecked(true);
        m_isContinuos = false;
        m_measurements->setContinuous(false);

        if (g_developerMode) {
            m_measurements->hideOneFqWidget();
        }
        return;
    }
    if(ui->tabWidget->currentWidget()->objectName() == "tab_tdr") {
        ui->continuousStartBtn->setChecked(false);
        return;
    }

    if (g_developerMode) {
        quint64 fqFrom = ui->lineEdit_fqFrom->text().remove(' ').toLongLong();
        quint64 fqTo = ui->lineEdit_fqTo->text().remove(' ').toLongLong();
        bool oneFq = m_isRange ? (fqTo==0) : (fqTo==fqFrom);
        if (oneFq) {
            ui->continuousStartBtn->setChecked(true);
            on_startOneFq(fqFrom, m_dotsNumber);
            return;
        }
    }

    ui->singleStart->setChecked(false);
    m_isContinuos = checked;
    m_analyzer->setContinuos(m_isContinuos);
    if(m_isContinuos)
    {
        m_bInterrupted = false;
        double start;
        double stop;

        // 20210423
        //start = getFqFrom();
        //stop = getFqTo();
        getEnteredFq(start, stop);

    AnalyzerParameters* param = AnalyzerParameters::current();
    qint64 minFreq = param == nullptr ? 100 : param->minFq().toULongLong();
    qint64 maxFreq = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toULongLong();
        if (CustomAnalyzer::customized()) {
            CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
            if (ca != nullptr) {
                minFreq = ca->minFq().toULongLong();
                maxFreq = ca->maxFq().toULongLong();
            }
        } else {
            AnalyzerParameters::normalizeFq(start, stop);
        }

        if(m_fqRestrict && (stop > static_cast<double>(maxFreq)))
        {
            stop = maxFreq;
            if(!m_isRange)
            {
                setFqTo(stop);
            }else
            {
                setFqTo((stop-start)/2);
            }
        }
        if (m_fqRestrict) {
            if((start > static_cast<double>(maxFreq)) || (start < static_cast<double>(minFreq)))
            {
                start = minFreq;
                if(!m_isRange)
                {
                    setFqFrom(start);
                }else
                {
                    setFqFrom((stop+start)/2);
                }
            }
        }
        QCPRange range(start, stop);
        m_swrWidget->xAxis->setRange(range);
        m_phaseWidget->xAxis->setRange(range);
        m_rsWidget->xAxis->setRange(range);
        m_rpWidget->xAxis->setRange(range);
        m_rlWidget->xAxis->setRange(range);
        m_s21Widget->xAxis->setRange(range);
        if (g_developerMode)
            m_userWidget->xAxis->setRange(range);

        m_settings->beginGroup("MainWindow");
        if (!m_isRange) {
            m_settings->setValue("rangeLower", start);
            m_settings->setValue("rangeUpper", stop);
        } else {
            m_settings->setValue("rangeLower", (stop-start)/2);
            m_settings->setValue("rangeUpper", (stop+start)/2);
        }
        m_settings->setValue("dotsNumber", m_dotsNumber);
        m_settings->endGroup();

        if(ui->tabWidget->currentWidget()->objectName() == "tab_user")
        {
            emit measureUser(start*1000, stop*1000, m_dotsNumber);
        } else {
            emit measure(start*1000, stop*1000, m_dotsNumber);
        }
        ui->measurmentsSaveBtn->setEnabled(true);
        ui->actionExport->setEnabled(true);
        ui->measurmentsDeleteBtn->setEnabled(false);
        ui->measurmentsClearBtn->setEnabled(false);
    }else
    {
        m_bInterrupted = true;
        m_analyzer->setContinuos(false);
    }
    m_measurements->setContinuous(m_isContinuos);
}

void MainWindow::on_startOneFq(quint64 _fq, int _dots)
{
    m_isContinuos = true;
    m_analyzer->setContinuos(m_isContinuos);
    m_measurements->setContinuous(m_isContinuos);

    emit measureOneFq(this, _fq*1000, _dots);

    ui->measurmentsSaveBtn->setEnabled(false);
    ui->actionExport->setEnabled(false);
    ui->measurmentsDeleteBtn->setEnabled(false);
    ui->measurmentsClearBtn->setEnabled(false);
}

void MainWindow::on_measurementComplete()
{
    if (m_analyzer->connectionType() == ReDeviceInfo::NANO)
        return;
    if (g_developerMode) {
        if (m_measurements->isOneFqMode()) {
            on_continuousStartBtn_clicked(false);
            return;
        }
    }

//{ TODO should be checked for autoclibration
    int autoCalibration = m_measurements->getAutoCalibration();
    if (autoCalibration != 0) {
        m_measurements->stopAutocalibrateProgress();
        autoCalibrate();
    } else {
        m_measurements->stopTDRProgress();
    }
//}

    m_tdrWidget->xAxis->setRangeLower(0);

    QTimer::singleShot(5, m_markers, SLOT(redraw()));
    if(m_isContinuos)
    {
        ui->singleStart->setChecked(false);

        double start;
        double stop;

        // 20210423
        //start = getFqFrom();
        //stop = getFqTo();
        getEnteredFq(start, stop);

    AnalyzerParameters* param = AnalyzerParameters::current();
    qint64 minFreq = param == nullptr ? 100 : param->minFq().toULongLong();
    qint64 maxFreq = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toULongLong();
        if (CustomAnalyzer::customized()) {
            CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
            if (ca != nullptr) {
                minFreq = ca->minFq().toULongLong();
                maxFreq = ca->maxFq().toULongLong();
            }
        } else {
            AnalyzerParameters::normalizeFq(start, stop);
        }
        if(m_fqRestrict && (stop > static_cast<double>(maxFreq)))
        {
            stop = maxFreq;
            if(!m_isRange)
            {
                setFqTo(maxFreq);
            }else
            {
                setFqTo((stop-start)/2);
            }
        }
        if(m_fqRestrict) {
            if((start > static_cast<double>(maxFreq)) || (start < static_cast<double>(minFreq)))
            {
                start = minFreq;
                if(!m_isRange)
                {
                    setFqFrom(start);
                }else
                {
                    setFqFrom((stop+start)/2);
                }
            }
        }
        QCPRange range(start, stop);
        m_swrWidget->xAxis->setRange(range);
        m_phaseWidget->xAxis->setRange(range);
        m_rsWidget->xAxis->setRange(range);
        m_rpWidget->xAxis->setRange(range);
        m_rlWidget->xAxis->setRange(range);
        m_s21Widget->xAxis->setRange(range);
        if (g_developerMode)
            m_userWidget->xAxis->setRange(range);
        if (!m_bInterrupted)
        {
            emit measureContinuous(start*1000, stop*1000, m_dotsNumber);
        } else {
            m_bInterrupted = true;
            ui->measurmentsDeleteBtn->setEnabled(true);
            ui->measurmentsClearBtn->setEnabled(true);
            m_analyzer->setContinuos(false);
            m_analyzer->setIsMeasuring(false);
            PopUpIndicator::setIndicatorVisible(false);
            ui->continuousStartBtn->setChecked(false);
        }
        m_measurements->setContinuous(m_isContinuos);
    } else {
        ui->singleStart->setChecked(false);
        ui->continuousStartBtn->setChecked(false);
        ui->fullBtn->setEnabled(true);
        ui->fullBtn->setChecked(true);
        m_measurements->on_measurementComplete();
        m_bInterrupted = true;
        ui->measurmentsDeleteBtn->setEnabled(true);
        ui->measurmentsClearBtn->setEnabled(true);
        ui->actionExport->setEnabled(true);
        ui->measurmentsSaveBtn->setEnabled(true);
        m_analyzer->setContinuos(false);
        m_analyzer->setIsMeasuring(false);
        PopUpIndicator::setIndicatorVisible(false);
    }

    // Fixes issue #33: Measurements::replot() only replots m_currentTab
    // while a scan streams in, so every other tab's QCustomPlot keeps a
    // stale pixel<->coordinate mapping until something replots it -- the
    // "brief params under cursor" hint would compute garbage (or nothing)
    // on those tabs until the user's own mouse movement there happened to
    // trigger a replot. Catch every tab up once here, now that the scan is
    // fully done.
    foreach (QCustomPlot *plot, m_mapWidgets) {
        // graph(0) on every tab is the live "current scan position" tick
        // (setWidgetsSettings()'s white QPen(255,255,255,150); fed via
        // setData(x,y) as each point streams in -- see the comment on
        // Measurements::replot() for the XOR-buffer mechanism). Nothing
        // ever cleared it once a scan finished, so it stayed drawn at
        // wherever the last point was.
        if (plot->graphCount() > 0) {
            plot->graph(0)->data()->clear();
        }
        plot->replot();
    }
}

void MainWindow::on_measurementCompleteNano()
{
//{ TODO should be checked for autoclibration
    int autoCalibration = m_measurements->getAutoCalibration();
    if (autoCalibration != 0) {
        m_measurements->stopAutocalibrateProgress();
        autoCalibrate();
    } else {
        m_measurements->stopTDRProgress();
    }
//}
    m_tdrWidget->xAxis->setRangeLower(0);
    QTimer::singleShot(5, m_markers, SLOT(redraw()));
    if(m_isContinuos)
    {
        ui->singleStart->setChecked(false);
        if (!m_bInterrupted)
        {
            // frequencies were saved by nanjkAnalyzer
            emit measureContinuous(0, 0, m_dotsNumber);
        } else {
            m_bInterrupted = true;
            ui->measurmentsDeleteBtn->setEnabled(true);
            ui->measurmentsClearBtn->setEnabled(true);
            m_analyzer->setContinuos(false);
            m_analyzer->setIsMeasuring(false);
            PopUpIndicator::setIndicatorVisible(false);
            ui->continuousStartBtn->setChecked(false);
        }
        m_measurements->setContinuous(m_isContinuos);
    } else { // single mode
        ui->singleStart->setChecked(false);
        ui->continuousStartBtn->setChecked(false);
        m_measurements->on_measurementComplete();
        m_bInterrupted = true;
        ui->measurmentsDeleteBtn->setEnabled(true);
        ui->measurmentsClearBtn->setEnabled(true);
        ui->actionExport->setEnabled(true);
        ui->measurmentsSaveBtn->setEnabled(true);
        m_analyzer->setContinuos(false);
        m_analyzer->setIsMeasuring(false);
        PopUpIndicator::setIndicatorVisible(false);
    }

    // Fixes issue #33: see the matching comment in on_measurementComplete().
    // This is the NANO analyzer's own end-of-scan path -- on_measurementComplete()
    // returns immediately for NANO connections (see its own early return
    // above) and never reaches its graph(0) clear, so that fix has to be
    // duplicated here rather than shared.
    foreach (QCustomPlot *plot, m_mapWidgets) {
        if (plot->graphCount() > 0) {
            plot->graph(0)->data()->clear();
        }
        plot->replot();
    }
}

void MainWindow::updateGraph ()
{
    QCustomPlot* plot = nullptr;
    try {
        QString str = ui->tabWidget->currentWidget()->objectName();
        if( str == "tab_swr")
        {
            plot = m_swrWidget;
        }else if(str == "tab_phase")
        {
            plot = m_phaseWidget;
        }else if(str == "tab_rs")
        {
            plot = m_rsWidget;
        }else if(str == "tab_rp")
        {
            plot = m_rpWidget;
        }else if(str == "tab_rl")
        {
            plot = m_rlWidget;
        }else if(str == "tab_s21")
        {
            plot = m_s21Widget;
        }else if(str == "tab_tdr")
        {
            plot = m_tdrWidget;
        }else if(str == "tab_smith")
        {
            resizeWnd();
            plot = m_smithWidget;
        }else if(str == "tab_user")
        {
            plot = m_userWidget;
        }
#ifndef NO_MULTITAB
        else if(str == "tab_multi") {
            for (int idx=0; idx<m_multiTabData.tabs.size(); idx++) {
                QString tab_name = m_multiTabData.tabs[idx];
                QString plot_name = g_mapTabPlotNames[tab_name];
                m_mapWidgets[plot_name]->replot();
            }
            return;
        }
#endif
    } catch(...) {
        return;
    }
    if (plot != nullptr)
        plot->replot();
}

void MainWindow::on_1secTimerTick()
{
    QString str = ui->tabWidget->currentWidget()->objectName();
    if(str == "tab_tdr" || str == "tab_smith")
    {
        m_measurements->hideGraphBriefHint();
        return;
    }
    // Was manually re-deriving m_swrWidget's global bounding box by adding
    // this->geometry() + ui->tabWidget's offset + m_swrWidget's offset --
    // which skips the offset of m_swrWidget's own parent tab page *within*
    // tabWidget (the tab bar's height and the tab widget's frame border).
    // That missing offset shifted the computed box's top edge up into where
    // the tab bar actually is, so the cursor still read as "inside" while
    // hovering the tab bar itself -- the hint would never be told to hide
    // while the mouse was up there, including while clicking a tab (issue:
    // tab clicks sometimes not registering, worst on TDR/Smith since they
    // also force hideGraphBriefHint() above, adding more show/hide churn
    // right at the tab bar). mapToGlobal() walks the full parent chain
    // correctly instead of re-deriving it by hand.
    QRect plotRect(m_swrWidget->mapToGlobal(QPoint(0, 0)), m_swrWidget->size());
    if (plotRect.contains(QCursor::pos()))
    {
        m_measurements->showHideHints();
    }else
    {
        m_measurements->hideGraphBriefHint();
    }
}

void MainWindow::on_presssCtrlAltShiftM()
{
    if (!g_developerMode)
        return;

    if (!ui->singleStart->isEnabled())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    measurementsClearBtn_clicked(true);

    m_measurements->setAutoCalibration(1);

    QString cmd = "cals\r";
    if (!m_analyzer->sendCommand(cmd)) {
        return;
    }
    QCoreApplication::processEvents();
    QThread::sleep(2);

    cmd = "calt\r";
    if (!m_analyzer->sendCommand(cmd)) {
        return;
    }
    QCoreApplication::processEvents();
    QThread::sleep(2);

    m_measurements->setFarEndMeasurement(0);
    onFullRange(true);
    m_dotsNumber = 200;
    // QString style = "QPushButton:checked{"
    //         "background-color: rgb(255, 1, 52);}";
    // ui->singleStart->setStyleSheet(style);

    on_singleStart_clicked();
    QApplication::processEvents();
}

void MainWindow::autoCalibrate()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QPair<double, double> calibr = m_measurements->autoCalibrate(); // <CableResistance, CableLength>
    QString cmd = QString("calrl%1,%2\r")
            .arg((double)calibr.first, 0, 'f', 8, QLatin1Char(' '))
            .arg((double)calibr.second, 0, 'f', 8, QLatin1Char(' '));
    m_analyzer->sendCommand(cmd);

    // QString style = "QPushButton:checked{"
    //         "background-color: rgb(0, 178, 90);}";
    // ui->singleStart->setStyleSheet(style);
    QApplication::restoreOverrideCursor();

    QString notify = QString("Autocalibration: CableResistance=%1, CableLength=%2")
            .arg((double)calibr.first, 0, 'f', 8, QLatin1Char(' '))
            .arg((double)calibr.second, 0, 'f', 8, QLatin1Char(' '));
    QRect rn(0, 0, rect().width(), 40);
    Notification::showMessage(notify, QColor(Qt::white), rn, 5000, ui->tabWidget->currentWidget());
    return;

}

void MainWindow::onMeasurementError()
{
    QApplication::beep();
    //showErrorPopup(tr("Measurement ERROR!"), 2000);
    on_pressEsc();
}

void MainWindow::on_presssCtrlAltShiftN()
{
    if (!g_developerMode)
        return;

    if (!ui->singleStart->isEnabled())
        return;

    connect(m_analyzer, &AnalyzerPro::updateAutocalibrate5, this, [this](int _dots, QString _msg){
        if (_msg.contains("START")) {
            m_measurements->startAutocalibrateProgress(this, _dots);
            m_measurements->progressDlg()->updateActionInfo("Adjustment of signal scaling factor");
            m_measurements->progressDlg()->setCancelable(false);
            m_measurements->progressDlg()->setValue(0);
        } else {
            int _max = m_measurements->progressDlg()->maxValue();
            m_measurements->progressDlg()->setValue(_max - _dots);
            m_measurements->progressDlg()->updateStatusInfo(QString(tr("Remains %1").arg(_dots)));
        }
    });
    QObject::connect(m_analyzer, &AnalyzerPro::stopAutocalibrate5, this, [this]() {
        QObject::disconnect(m_analyzer, &AnalyzerPro::stopAutocalibrate5, this, nullptr);
        QObject::disconnect(m_analyzer, &AnalyzerPro::updateAutocalibrate5, this, nullptr);
        m_measurements->stopAutocalibrateProgress();
    });

    m_analyzer->setParseState(WAIT_CALFIVEKOHM_START);
    m_analyzer->sendCommand("CALFIVEKOHM\r");
}


