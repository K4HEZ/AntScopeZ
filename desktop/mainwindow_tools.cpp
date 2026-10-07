#include "mainwindow.h"
#include "appconfig.h"
#include "ui_mainwindow.h"
#include "analyzer/customanalyzer.h"
#include <QScrollArea>
#include "Notification.h"

// Tools menu (mainwindow.ui's menuTools) lives here, matching this
// project's existing tier-1 split of mainwindow.cpp by feature area.

void MainWindow::on_actionMarkerComparison_triggered()
{
    if (m_markerComparisonDialog == nullptr) {
        m_markerComparisonDialog = new MarkerComparisonDialog(m_markers, m_measurements, AppConfig::get().measureSystemMetric, this);
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
        // NanoVNA connections never fire measurementComplete() at a real
        // completion (see the identical fix/comment for TdrScanPanel::
        // refreshResult() in setupTdrPanel() below) -- without this, the combos/values here never updated
        // live for a NanoVNA scan while this dialog was open. Fixes #39.
        connect(m_analyzer, &AnalyzerPro::measurementCompleteNano,
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

// TDR mode's left-column panel: scan setup and, after a scan, the peak
// analysis. Built once, into modeStack's second page.
void MainWindow::setupTdrPanel()
{
    m_tdrPanel = new TdrScanPanel(this);

    QScrollArea* scroll = new QScrollArea(ui->modeStack);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_tdrPanel);
    ui->modeStack->addWidget(scroll);

    QActionGroup* modeGroup = new QActionGroup(this);
    modeGroup->addAction(ui->actionModeSweep);
    modeGroup->addAction(ui->actionModeTdr);

    connect(m_tdrPanel, &TdrScanPanel::scanRequested,
            this, &MainWindow::on_tdrScanRequested);
    // Live re-plot from already-captured data, no rescan.
    connect(m_tdrPanel, &TdrScanPanel::windowChanged,
            this, [this](TdrWindow window, double beta) {
        m_measurements->setTdrWindowType(window);
        m_measurements->setTdrKaiserBeta(beta);
        m_measurements->redrawTDR(-1, false);
    });
    // The Result section refreshes after any scan, NanoVNA included.
    connect(m_analyzer, &AnalyzerPro::measurementComplete,
            m_tdrPanel, &TdrScanPanel::refreshResult);
    connect(m_analyzer, &AnalyzerPro::measurementCompleteNano,
            m_tdrPanel, &TdrScanPanel::refreshResult);
    connect(m_tdrPanel, &TdrScanPanel::applyVelocityFactorAsCustom,
            this, [this](double vf) {
        AppConfig::get().cable.velFactor = vf;
        AppConfig::get().cable.resistance = 50.0;
        AppConfig::get().cable.lossConductive = 0.0;
        AppConfig::get().cable.lossDielectric = 0.0;
        AppConfig::get().cable.lossUnits = 0;
        AppConfig::get().cable.lossAtAnyFq = true;
        AppConfig::get().cableIsPreset = false;
        m_measurements->setCableVelFactor(vf);
        m_measurements->setCableResistance(AppConfig::get().cable.resistance);
        m_measurements->setCableLossConductive(AppConfig::get().cable.lossConductive);
        m_measurements->setCableLossDielectric(AppConfig::get().cable.lossDielectric);
        m_measurements->setCableLossUnits(AppConfig::get().cable.lossUnits);
        m_measurements->setCableLossAtAnyFq(AppConfig::get().cable.lossAtAnyFq);
        m_measurements->redrawTDR();

        if (m_settingsDialog != nullptr) {
            m_settingsDialog->setCableVelFactor(AppConfig::get().cable.velFactor);
            m_settingsDialog->setCableResistance(AppConfig::get().cable.resistance);
            m_settingsDialog->setCableLossConductive(AppConfig::get().cable.lossConductive);
            m_settingsDialog->setCableLossDielectric(AppConfig::get().cable.lossDielectric);
            m_settingsDialog->setCableLossUnits(AppConfig::get().cable.lossUnits);
            m_settingsDialog->setCableLossAtAnyFq(AppConfig::get().cable.lossAtAnyFq);
            m_settingsDialog->setCableIsPreset(false);
        }
    });
    m_tdrPanel->setMeasurements(m_measurements);
    m_tdrPanel->setConnected(m_analyzerConnected);
}

// Device limits, velocity factor and units can change while the panel is
// hidden; refresh when entering TDR mode and on connect.
void MainWindow::refreshTdrPanelLimits()
{
    if (m_tdrPanel == nullptr)
        return;
    AnalyzerParameters* param = AnalyzerParameters::current();
    qint64 minFqKHz = param == nullptr ? 100 : param->minFq().toULongLong();
    qint64 maxFqKHz = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toULongLong();
    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr) {
            minFqKHz = ca->minFq().toULongLong();
            maxFqKHz = ca->maxFq().toULongLong();
        }
    }
    m_tdrPanel->setFrequencyLimits(minFqKHz, maxFqKHz);
    m_tdrPanel->setVelocityFactor(m_measurements->cableVelFactor());
    m_tdrPanel->setMeasureSystemMetric(AppConfig::get().measureSystemMetric);
}

// Tuning mode: left-column controls plus a "Tuning" tab. Readings never
// become measurements; Measurements::oneFqData() feeds the panel directly.
void MainWindow::setupTuning()
{
    m_tuningControls = new TuningControls(this);
    ui->modeStack->addWidget(m_tuningControls);
    m_tuningPanel = new TuningPanel(this);
    ui->tabWidget->addTab(m_tuningPanel, tr("Tuning"));
    // Readings sit under Cursor Details' spot, which Tuning has no use for.
    ui->verticalLayout_Left->addWidget(m_tuningPanel->parametersWidget());
    m_tuningPanel->parametersWidget()->setVisible(false);
    ui->actionModeSweep->actionGroup()->addAction(ui->actionModeTuning);

    m_settings->beginGroup("Settings");
    m_tuningControls->setStepKHz(m_settings->value("tuningStepKHz", 10.0).toDouble());
    m_tuningControls->setRateSeconds(m_settings->value("tuningRateSec", 0).toInt());
    m_tuningControls->setFrequencyKHz(m_settings->value("tuningFreqKHz", 7100.0).toDouble(), false);
    m_settings->endGroup();
    m_tuningPanel->setFrequencyKHz(m_tuningControls->frequencyKHz());
    m_tuningControls->setConnected(m_analyzerConnected);
    refreshTuningLimits();

    connect(m_tuningControls, &TuningControls::frequencyChanged, this, [this](double khz) {
        m_tuningPanel->setFrequencyKHz(khz);
        // The reading loop picks this up on its next request.
        m_oneFqFreq = static_cast<quint64>(qRound64(khz * 1000.0));
        m_settings->beginGroup("Settings");
        m_settings->setValue("tuningFreqKHz", khz);
        m_settings->endGroup();
    });
    connect(m_tuningControls, &TuningControls::stepChanged, this, [this](double khz) {
        m_settings->beginGroup("Settings");
        m_settings->setValue("tuningStepKHz", khz);
        m_settings->endGroup();
    });
    connect(m_tuningControls, &TuningControls::rateChanged, this, [this](int seconds) {
        m_settings->beginGroup("Settings");
        m_settings->setValue("tuningRateSec", seconds);
        m_settings->endGroup();
    });
    m_tuningTimer.setSingleShot(true);
    connect(&m_tuningTimer, &QTimer::timeout, this, [this]() {
        if (!m_bInterrupted && m_measurements->isOneFqMode())
            on_startOneFq(m_oneFqFreq, 0, true);
    });
    connect(m_tuningPanel, &TuningPanel::frequencyPicked, this, [this](double khz) {
        m_tuningControls->setFrequencyKHz(khz);
    });
    connect(m_markers, &Markers::markerActivated, this, &MainWindow::on_markerActivated);
    connect(m_tuningControls, &TuningControls::startRequested, this, &MainWindow::on_tuningStart);
    connect(m_tuningControls, &TuningControls::stopRequested, this, &MainWindow::on_tuningStop);
    connect(m_measurements, &Measurements::oneFqData, m_tuningPanel, &TuningPanel::addData);
}

// Called with the active region's band list (kHz "from,to,name" lines).
void MainWindow::refreshTuningBands(const QStringList* bands)
{
    if (m_tuningControls == nullptr)
        return;
    QList<BandPreset> list;
    if (bands != nullptr) {
        for (const QString& line : *bands) {
            QStringList f = line.split(',');
            if (f.size() < 2)
                continue;
            BandPreset b;
            b.fromKHz = f.at(0).trimmed().toDouble();
            b.toKHz = f.at(1).trimmed().toDouble();
            b.label = f.size() > 2 ? f.at(2).trimmed() : QString();
            list << b;
        }
    }
    m_tuningControls->setBands(list);
    m_tuningPanel->setBands(list);
}

void MainWindow::refreshTuningLimits()
{
    if (m_tuningControls == nullptr)
        return;
    AnalyzerParameters* param = AnalyzerParameters::current();
    double minKHz = param == nullptr ? 0.1 : param->minFq().toDouble();
    double maxKHz = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toDouble();
    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr) {
            minKHz = ca->minFq().toDouble();
            maxKHz = ca->maxFq().toDouble();
        }
    }
    m_tuningControls->setFrequencyLimits(minKHz, maxKHz);
}

void MainWindow::on_tuningStart()
{
    if (isMeasuring())
        return;
    // One-frequency readings use the RigExpert FRX command; NanoVNA
    // connections have no equivalent here (same as the old One Fq mode).
    if (m_analyzer->connectionType() == ReDeviceInfo::NANO ||
        m_analyzer->connectionType() == ReDeviceInfo::NANOV2) {
        Notification::showMessage(tr("Tuning isn't available on this analyzer."), this);
        return;
    }
    m_tuningPanel->clear();
    m_tuningControls->setRunning(true);
    on_startOneFq(static_cast<quint64>(qRound64(m_tuningControls->frequencyKHz() * 1000.0)), 0, true);
}

// Marker table double-click: Tuning tunes to the marker; Sweep pans the charts
// to center on it, keeping the zoom.
void MainWindow::on_markerActivated(double fqKHz)
{
    if (m_appMode == AppMode::Tuning) {
        m_tuningControls->setFrequencyKHz(fqKHz);
        return;
    }
    if (m_appMode != AppMode::Sweep)
        return;
    QList<QCustomPlot*> plots = {m_swrWidget, m_phaseWidget, m_rsWidget, m_rpWidget, m_rlWidget, m_s21Widget};
#if USER_DEFINED_FEATURE
    plots << m_userWidget;
#endif
    for (QCustomPlot* plot : plots) {
        QCPRange r = plot->xAxis->range();
        double half = r.size() / 2;
        plot->xAxis->setRange(fqKHz - half, fqKHz + half);
        plot->replot();
    }
    QTimer::singleShot(5, m_markers, SLOT(redraw()));
}

void MainWindow::on_tuningStop()
{
    on_pressEsc();
}

void MainWindow::on_actionModeSweep_triggered()
{
    setAppMode(AppMode::Sweep);
}

void MainWindow::on_actionModeTuning_triggered()
{
    setAppMode(AppMode::Tuning);
}

void MainWindow::on_actionModeTdr_triggered()
{
    setAppMode(AppMode::Tdr);
}

void MainWindow::setAppMode(AppMode mode)
{
    bool tdr = (mode == AppMode::Tdr);
    bool tuning = (mode == AppMode::Tuning);
    auto syncMenu = [this](AppMode m) {
        ui->actionModeSweep->setChecked(m == AppMode::Sweep);
        ui->actionModeTuning->setChecked(m == AppMode::Tuning);
        ui->actionModeTdr->setChecked(m == AppMode::Tdr);
    };
    syncMenu(mode);
    if (m_appModeApplied && mode == m_appMode)
        return;
    if (isMeasuring() || (m_tuningControls != nullptr && m_tuningControls->isRunning())) {
        // Not mid-scan; put the menu back.
        syncMenu(m_appMode);
        return;
    }
    m_appMode = mode;
    m_appModeApplied = true;

    // A stacked widget sizes to its biggest page; ignore the hidden ones.
    int page = tdr ? 1 : (tuning ? 2 : 0);
    ui->modeStack->setCurrentIndex(page);
    for (int i = 0; i < ui->modeStack->count(); i++) {
        ui->modeStack->widget(i)->setSizePolicy(QSizePolicy::Preferred,
                            i == page ? QSizePolicy::Expanding : QSizePolicy::Ignored);
    }
    ui->verticalLayout_Left->setStretchFactor(ui->modeStack, tdr ? 3 : 1);
    ui->verticalLayout_Left->setStretchFactor(ui->verticalLayout_5, 1); // Measurements table

    // Tuning records nothing and has no chart to hover.
    for (QWidget* w : {static_cast<QWidget*>(ui->measurementsHeading), static_cast<QWidget*>(ui->measurementsHeadingLine),
                       static_cast<QWidget*>(ui->tableWidget_measurments)})
        w->setVisible(!tuning);
    m_measurements->setGraphHintSuppressed(tuning);
    ui->graphHintHeadingLine->setVisible(!tuning);
    m_tuningPanel->parametersWidget()->setVisible(tuning);

    // Chart area: TDR and Tuning each show only their own tab. Other tabs
    // (S21, User, Multi) already come and go on their own, so remember what
    // was showing and put that back.
    int tdrIndex = ui->tabWidget->indexOf(m_tab_tdr);
    int tuningIndex = ui->tabWidget->indexOf(m_tuningPanel);
    int solo = tdr ? tdrIndex : (tuning ? tuningIndex : -1);
    if (solo >= 0) {
        if (m_tabsShownBeforeSolo.isEmpty()) {
            for (int i = 0; i < ui->tabWidget->count(); i++)
                m_tabsShownBeforeSolo.insert(ui->tabWidget->widget(i), ui->tabWidget->isTabVisible(i));
        }
        for (int i = 0; i < ui->tabWidget->count(); i++)
            ui->tabWidget->setTabVisible(i, i == solo);
        ui->tabWidget->setCurrentIndex(solo);
    } else {
        for (int i = 0; i < ui->tabWidget->count(); i++) {
            QWidget* w = ui->tabWidget->widget(i);
            bool own = (i == tdrIndex || i == tuningIndex);
            bool show = !own && (m_tabsShownBeforeSolo.value(w, false) || ui->tabWidget->isTabVisible(i));
            ui->tabWidget->setTabVisible(i, show);
        }
        m_tabsShownBeforeSolo.clear();
        if (!ui->tabWidget->isTabVisible(ui->tabWidget->currentIndex()))
            ui->tabWidget->setCurrentIndex(0);
    }

    m_measurements->setMode(tdr ? MeasurementKind::Tdr : MeasurementKind::Sweep);
    if (tdr)
        refreshTdrPanelLimits();
    if (tuning)
        refreshTuningLimits();

    m_settings->beginGroup("Settings");
    m_settings->setValue("appMode", tdr ? "tdr" : (tuning ? "tuning" : "sweep"));
    m_settings->endGroup();
}
