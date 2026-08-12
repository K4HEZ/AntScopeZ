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

void MainWindow::on_analyzerFound(int index)
{
    QString name = AnalyzerParameters::byIndex(index)->name();
    on_analyzerNameFound(name);
}

void MainWindow::on_analyzerNameFound(QString name)
{
    QString name1 = "AntScopeZ v." + QString(ANTSCOPEZ_VER);
    setWindowTitle(name1 + " - " + name);

    ui->checkBoxCalibration->setCheckState(Qt::Unchecked);
    bool zeroII = name.contains("Zero II");
    ui->singleStart->setEnabled(true);
    ui->continuousStartBtn->setEnabled(true);
    ui->fullBtn->setEnabled(true);
    if (g_bAA55modeNewProtocol) {
        ui->actionAnalyzerData->setEnabled(true);
        ui->actionScreenshotAA->setEnabled(false);
    } else if (!NanovnaAnalyzer::isConnected() && !zeroII) {
        ui->actionAnalyzerData->setEnabled(true);
        ui->actionScreenshotAA->setEnabled(true);
    } else {
        ui->actionAnalyzerData->setEnabled(false);
        ui->actionScreenshotAA->setEnabled(false);
    }
    if (analyzer()->getModelString().contains("Match") && analyzer()->connectionType() == ReDeviceInfo::BLE) {
        ui->actionAnalyzerData->setEnabled(false);
        ui->actionScreenshotAA->setEnabled(false);
    }
    if (analyzer()->getModelString().contains("MATCH U") && analyzer()->connectionType() == ReDeviceInfo::BLE) {
        ui->actionAnalyzerData->setEnabled(false);
        ui->actionScreenshotAA->setEnabled(false);
    }

    // This used to also auto-fire the AntScope2 software-update check, the
    // firmware-update check, and the scrolling "marquee" ad banner fetch --
    // all silent network calls to rigexpert.com on every analyzer connect.
    // This build isn't vendor-distributed, so none of those should happen
    // without the user asking for them: the software-update check and the
    // marquee (MarqueeLabel, its "ad banner" purpose gone with the fetch)
    // are gone outright, and the firmware-update check now only runs from
    // the "Check for firmware updates" button in Settings
    // (Settings::checkUpdatesBtn() -> AnalyzerPro::on_checkUpdatesBtn_clicked(),
    // wired up below).

    if (name.contains("NanoVNA", Qt::CaseInsensitive)) {
        ui->lineEdit_points->setEnabled(false);
        ui->speedAccuracySlider->setEnabled(false);
        setDotsNumber(100);
    } else {
        ui->lineEdit_points->setEnabled(true);
        ui->speedAccuracySlider->setEnabled(true);
    }
    m_calibration->init(m_analyzer->getSerialNumber());
}

void MainWindow::on_deviceDisconnected()
{
    QWidget::setCursor(Qt::ArrowCursor);
    QString name = "AntScopeZ v." + QString(ANTSCOPEZ_VER);
    name += tr(" - Analyzer not connected");
    setWindowTitle(name);
    ui->singleStart->setEnabled(false);
    ui->continuousStartBtn->setEnabled(false);
    ui->actionAnalyzerData->setEnabled(false);
    ui->actionScreenshotAA->setEnabled(false);
    ui->lineEdit_points->setEnabled(true);
    ui->speedAccuracySlider->setEnabled(true);
    ui->fullBtn->setEnabled(false);
    ui->fullBtn->setChecked(false);

    PopUpIndicator::hideIndicator(this);
    m_analyzer->setIsMeasuring(false);
    ui->singleStart->setChecked(false);
    ui->continuousStartBtn->setChecked(false);
    m_measurements->setContinuous(false);
    m_bInterrupted = true;
    if (m_calibration != nullptr)
        m_calibration->setSerial(QString());

    if (m_analyzer != nullptr)
        m_analyzer->searchAnalyzer();
}

void MainWindow::on_actionAnalyzerData_triggered()
{
    m_analyzerData = new AnalyzerData(m_analyzer->getModel(), this);
    m_analyzerData->setAttribute(Qt::WA_DeleteOnClose);
    m_analyzer->getAnalyzerData();
    connect(m_analyzer,SIGNAL(analyzerDataStringArrived(QString)),m_analyzerData,SLOT(on_analyzerDataStringArrived(QString)));
    connect(m_analyzerData,&AnalyzerData::itemDoubleClick,m_analyzer,&AnalyzerPro::on_itemDoubleClick);
    connect(m_analyzerData,&AnalyzerData::signalSaveFile,this,&MainWindow::on_SaveFile);
    connect(m_analyzerData, &AnalyzerData::dataChanged, this, &MainWindow::on_dataChanged);
    //connect(m_analyzerData,SIGNAL(dialogClosed()),m_analyzer,SLOT(on_dialogClosed()));
    m_analyzerData->exec();
}

void MainWindow::on_actionScreenshotAA_triggered()
{
    AnalyzerParameters* param = AnalyzerParameters::current();
    if (param == nullptr)
        return;
    int wd = param->getWidth();
    int ht = param->getHeight();
    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr) {
            wd = ca->width();
            ht = ca->height();
        }
    }

    if (analyzer()->connectionType() == ReDeviceInfo::Serial && param->name() != "AA-230 ZOOM") {
        g_showMessageBox(nullptr, QMessageBox::Warning, tr("Screen shot"), tr("To get screenshots on this analyzer, you need to use the LCD2Clip utility from the https://rigexpert.com"));
        return;
    }

    m_screenshot = new Screenshot(this, m_analyzer->getModel(), ht, wd);
    m_screenshot->setAttribute(Qt::WA_DeleteOnClose);
    m_screenshot->setWindowTitle(tr("Screenshot"));

    connect(m_analyzer,SIGNAL(analyzerScreenshotDataArrived(QByteArray)),m_screenshot,SLOT(on_newData(QByteArray)));
    connect(m_analyzer,SIGNAL(analyzerScreenPaletteArrived(QByteArray, quint8)),m_screenshot,SLOT(on_fillPalette(QByteArray, quint8)));
    connect(m_screenshot,SIGNAL(screenshotComplete()),m_analyzer,SLOT(on_screenshotComplete()));
    connect(m_screenshot,SIGNAL(newScreenshot()),m_analyzer,SLOT(makeScreenshot()));

    m_analyzer->makeScreenshot();

    m_screenshot->exec();
    m_screenshot = nullptr;
}

void MainWindow::on_selectDeviceDialog()
{
    if (g_usbOnly) {
        SelectionParameters::selected.type = ReDeviceInfo::HID;
        //m_analyzer->createDevice(SelectionParameters::selected, new HidAnalyzer(this));
        return;
    }

    // Guard against a second copy opening on top of one that's already up.
    // dlg.exec() below is application-modal by default, but this window
    // manager doesn't reliably enforce that (see the focus-stealing note
    // further down) -- and this function has two independent triggers,
    // Settings' "Connect analyzer" button (settings.cpp) and the startup
    // auto-reconnect-or-prompt fallback (on_refreshConnection(), via a
    // QTimer::singleShot), which can otherwise race each other or a fast
    // double-click into stacking two instances.
    //
    // Was: checking QApplication::activeModalWidget() instead of a new
    // member flag, since that's already the thing Qt itself tracks for
    // exactly this -- except it doesn't actually get set here. dlg.show()
    // below runs before dlg.exec(), and SelectDeviceDialog never calls
    // setModal()/setWindowModality() itself, so exec()'s modal-widget
    // registration (which happens as part of making the widget visible)
    // finds it already visible from that earlier show() and likely never
    // runs. m_selectDeviceDialogOpen sidesteps relying on that Qt-internal
    // bookkeeping at all.
    if (m_selectDeviceDialogOpen)
        return;
    m_selectDeviceDialogOpen = true;

    // Note which window is actually on top right now (e.g. the Settings
    // dialog, when this is opened via its "Connect analyzer" button) so its
    // WM_TRANSIENT_FOR can point at it below -- otherwise the window manager
    // can leave focus on that still-modal window instead of granting it to
    // this one.
    //
    // This must NOT be done by passing activeParent as dlg's QObject parent:
    // dlg is stack-allocated, but Settings has Qt::WA_DeleteOnClose, and this
    // window manager (see the focus-stealing issues fixed earlier) does not
    // reliably keep a close request off Settings just because dlg.exec() is
    // logically modal over it. If Settings gets closed while dlg is still
    // open, ~QObject() would walk its children and `delete` this stack
    // object -- instant heap corruption, then a second real destructor call
    // when dlg's scope ends. Keep the real (safe, outlives dlg) parent as
    // `this`, and apply the transient-for hint directly on the native
    // window instead, after the window exists.
    QWidget* activeParent = QApplication::activeWindow();
    SelectDeviceDialog dlg(false, this);
    // This is frequently opened from a QTimer::singleShot callback (see
    // on_refreshConnection()) rather than direct user input, and window
    // managers commonly refuse to grant focus to a window shown outside a
    // direct user interaction. Without focus the dialog renders but silently
    // drops every click. Force it explicitly, and pump the event loop so the
    // window manager has a chance to actually apply the activation request
    // before we block in exec().
    // A single immediate attempt isn't reliable: this dialog can appear as
    // the very first window the app ever shows (cold start, before the user
    // has interacted with anything), which window managers are especially
    // reluctant to auto-focus. Keep retrying for a bit once exec()'s nested
    // loop is actually running, instead of giving up after one attempt.
    dlg.show();
    if (activeParent && activeParent->windowHandle() && dlg.windowHandle())
        dlg.windowHandle()->setTransientParent(activeParent->windowHandle());
    dlg.raise();
    dlg.activateWindow();
    for (int delayMs : {0, 50, 150, 300, 600}) {
        QTimer::singleShot(delayMs, &dlg, [&dlg](){
            if (dlg.isVisible() && !dlg.isActiveWindow()) {
                dlg.raise();
                dlg.activateWindow();
            }
        });
    }
    int execResult = dlg.exec();
    m_selectDeviceDialogOpen = false;
    if (execResult == QDialog::Accepted) {
        SelectionParameters sel_par = SelectionParameters::selected;
        AnalyzerParameters* selected = AnalyzerParameters::current();
        if (selected != nullptr) {
            m_analyzer->on_connectDevice(dlg.analyzer());
            emit m_analyzer->analyzerFound(selected->index());
        }
    }
    // Was: unconditionally closeSettingsDialog() here, which closed the
    // Settings dialog out from under the user just because they selected or
    // canceled a device from its "Connect analyzer" button -- Settings
    // should stay open so they can keep working in it (issue #2). This
    // function is also reached from flows where Settings was never open
    // (m_settingsDialog already null there), so that call was only ever
    // actually closing anything for the Settings-originated case.
    // settingsBtn should only re-enable if Settings isn't still open --
    // otherwise a second "Settings" click while it's still up would reuse
    // the same (non-null) m_settingsDialog and re-run its one-time setup a
    // second time on top of the first. (setLanguages() itself now clears
    // its combo box before repopulating, so it's no longer harmed by that
    // specifically -- but plenty of the rest of this setup isn't as
    // forgiving.)
    ui->actionSettings->setEnabled(m_settingsDialog == nullptr);
}

void MainWindow::on_refreshConnection()
{
    AnalyzerParameters* ap = AnalyzerParameters::byName(SelectionParameters::selected.name);
    if (ap != nullptr) {
        SelectionParameters::selected.modelIndex = ap->index();
         AnalyzerParameters::setCurrent(ap);
         if (m_analyzer->refreshConnection()) {
            return;
         }
    }
    if (! g_usbOnly) {
        m_settings->beginGroup("Settings");
        bool openAtLaunch = m_settings->value("open-connect-analyzer-at-launch", true).toBool();
        m_settings->endGroup();
        // Same "Open 'Connect Analyzer' on launch" setting as the other
        // startup trigger in setWidgetsSettings() -- this function is only
        // ever reached from that one startup QTimer::singleShot, so this
        // fallback is effectively startup-only too, not something a
        // manual "Connect analyzer" click should ever be gated by.
        if (openAtLaunch) {
            QTimer::singleShot(100, this, [=](){
               on_selectDeviceDialog();
            });
        }
    }
}

