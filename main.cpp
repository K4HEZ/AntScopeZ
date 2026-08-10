                                                                                                                                    #include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QAbstractNativeEventFilter>
#include <QIcon>
#include "analyzer/customanalyzer.h"
#include "settings.h"
#include "style.h"
#include <QSettings>

bool g_developerMode = false;
bool g_usbOnly = false;
bool g_raspbian = false;
bool g_bAA55modeNewProtocol = false;
MainWindow* g_mainWindow;

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbt.h>

//#ifndef _DEBUG
//#define LOG_TO_FILE
//#endif

#ifdef LOG_TO_FILE
QString logFilePath = "antscopez";
bool firstLog = true;
void customMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (type != QtInfoMsg)
        return;
    QHash<QtMsgType, QString> msgLevelHash({{QtDebugMsg, "Debug"}, {QtInfoMsg, "Info"}, {QtWarningMsg, "Warning"}, {QtCriticalMsg, "Critical"}, {QtFatalMsg, "Fatal"}});
    QTime time = QTime::currentTime();
    QString formattedTime = time.toString("hh:mm:ss.zzz");
    QString sufix = QDateTime::currentDateTime().toString("-yyyyMMdd_hhmmss.log");
    QString logLevelName = "";//msgLevelHash[type];

    QString txt = QString("%1 %2: %3 (%4:%5, %6)")
            .arg(formattedTime, logLevelName, msg,  context.file)
            .arg(context.line)
            .arg(context.function);
    if (firstLog) {
        firstLog = false;
        QDir dir = QDir::tempPath();
        logFilePath = dir.absoluteFilePath(logFilePath + sufix);
    }
    QFile outFile(logFilePath);
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream ts(&outFile);
    ts << txt << "\n";
    ts.flush();
}
#endif

class MyNativeEventFilter : public QAbstractNativeEventFilter {
public :
    virtual bool nativeEventFilter( const QByteArray &eventType, void *message, long * /*result*/ )
    //Q_DECL_OVERRIDE
    {
        if (eventType == "windows_generic_MSG")
        {
          MSG *msg = static_cast<MSG *>(message);
          static int i = 0;

              msg = (MSG*)message;
                  //qDebug() << "message: " << msg->message << " wParam: " << msg->wParam
                    //  << " lParam: " << msg->lParam;
              if (msg->message == WM_DEVICECHANGE)
              {
                  qDebug() << "WM_DEVICECHANGE: " <<
                              (msg->wParam==DBT_DEVICEARRIVAL?"DBT_DEVICEARRIVAL":
                              (msg->wParam==DBT_DEVICEREMOVECOMPLETE?"DBT_DEVICEREMOVECOMPLETE":QString::number(msg->wParam)));
              }
            }
        return false;
    }
};
#endif


void setAbsoluteFqMaximum()
{
    int fqMax = 0;

    if (CustomAnalyzer::customized() && CustomAnalyzer::getCurrent() != nullptr) {
            fqMax = CustomAnalyzer::getCurrent()->maxFq().toInt();
    } else {
        foreach (AnalyzerParameters* param, AnalyzerParameters::analyzers()) {
            QString str = param->maxFq();
            int fq = str.toInt();
            fqMax = qMax(fqMax, fq);
        }
    }
    ABSOLUTE_MAX_FQ = fqMax;
}

int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                      QString title, QString text,
                      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
{
    QMessageBox msgBox;
    msgBox.setIcon(icon);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setStandardButtons(buttons);
    msgBox.setDefaultButton(defaultButton);
    msgBox.setStyleSheet(Style::messageBox());
    return msgBox.exec();
}

int main(int argc, char *argv[])
{
    qputenv("QT_ACCESSIBILITY", "0");
/*
    QString title = windowTitle();
    bool res = m_qtLanguageTranslator->load("QtLanguage_" + locale, Settings::languageDataFolder());
    qApp->installTranslator(m_qtLanguageTranslator);
    ui->retranslateUi(this);
*/


// deprecated
//    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

// Fix for 4K Display Issues Disabled
#ifdef DUMB_Q_OS_WIN
    char** params;
    params = new char*[argc+2];
    int ip=0;
    for (; ip<argc; ip++) {
        params[ip] = argv[ip];
    }
    params[ip++] = (char*)"--platform";
    params[ip] = (char*)"windows:dpiawareness=0";
    int cntp = argc + 2;
    QApplication a(cntp, params);
#else
    QApplication a(argc, argv);
#endif

    // Used by QStandardPaths (Settings::localDataFolder() et al.) to build
    // the per-user config directory -- ~/.config/AntScopeZ on Linux. No
    // organization name (previously the old GitHub username): AntScopeZ is this fork's own
    // identity, distinct enough from "AntScope2"/"RigExpert" on its own that
    // a real RigExpert-shipped AntScope2 install can never share -- or get
    // confused with -- this fork's settings/calibration data, without also
    // needing an extra directory level for it.
    a.setApplicationName("AntScopeZ");

    // Application-wide window icon (taskbar, alt-tab, etc.). Individual
    // dialogs (screenshot.ui, print.ui, ...) already reference this same
    // qrc resource for their own icon, but nothing previously set it at the
    // QApplication level, so the running app fell back to a generic icon
    // regardless of what AntScopeZ.png/.ico/.icns on disk looked like.
    a.setWindowIcon(QIcon(":/new/prefix1/AntScopeZ.png"));

    QStringList args = a.arguments();

#ifdef LOG_TO_FILE
    qInstallMessageHandler(customMessageOutput);
    qInfo() << "                                                         ";
    qInfo() << "*********************************************************";
    qInfo() << "  AntScopeZ " << QString(ANTSCOPEZ_VER) << " STARTED " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    qInfo() << "                                                         ";
#endif

#ifdef Q_OS_WIN
    // TODO DEBUG: catch attach/detach device event
    //MyNativeEventFilter myEventfilter;
    //a.eventDispatcher()->installNativeEventFilter(&myEventfilter);
#endif

    // -developer is intentionally inert as of 2026-08-10, regardless of
    // whether it's passed on the command line. This flag gates the
    // Settings > Customize tab (Custom Analyzer / "User Defined" chart tab),
    // among a few smaller things (the "Don't restrict frequency" checkbox,
    // Ctrl+Alt+Shift+M/N auto-calibration debug shortcuts, a higher
    // MAX_DOTS/spinBoxPoints ceiling). Investigating a "User Defined" tab
    // crash led to actually exercising Custom Analyzer for the first time in
    // a while, which turned up enough problems that shipping it live to
    // anyone who happens to pass -developer isn't safe:
    //
    //   - FIXED this session: AnalyzerPro::slotFullInfo() (analyzerpro.cpp)
    //     null-derefed AnalyzerParameters::byName(getModelString()) --
    //     getModelString() returns CustomAnalyzer::currentPrototype() while
    //     customized, which is never a real model name (defaults to the
    //     literal placeholder "Custom"), so byName() reliably returned
    //     nullptr and the very next line crashed on it. Now reads
    //     AnalyzerParameters::current() instead (the real, physically
    //     connected device, already resolved by serial-number prefix at
    //     connection time) -- license-level bookkeeping is about the real
    //     hardware, not whatever custom override is configured for display.
    //   - FIXED this session: Settings::initCustomizeTab() (settings.cpp)
    //     unconditionally .hide()'d comboBoxPrototype and its label, despite
    //     correctly populating it with every AnalyzerParameters model name
    //     right below -- so the one control that lets you pick a valid
    //     reference model literally couldn't be used. Unhidden.
    //   - STILL BROKEN: the custom min/max frequency override doesn't
    //     survive a scan even once a valid prototype is picked.
    //     AnalyzerParameters::normalizeFq()/normalizeFqRange()
    //     (analyzerparameters.h) unconditionally clamp to
    //     AnalyzerParameters::current()'s real stock range and have no idea
    //     CustomAnalyzer exists; on_dataChanged() (mainwindow.cpp) calls
    //     normalizeFqRange() on every range change, so "Full Range" and a
    //     typed Stop value both get silently clamped straight back down to
    //     the real device's limit. Called from ~8 sites total
    //     (mainwindow.cpp), not just the one -- needs those made
    //     customization-aware, not a single call-site patch.
    //   - STILL BROKEN, not diagnosed: running an actual scan against a real
    //     device (RigExpert Match RFE, this session) with "Use customized
    //     analyzer" checked gets the command rejected at the protocol level
    //     -- HidAnalyzer::sendData() qDebug()s
    //     `***** ERROR:  "Error.Not recognized"` in response to
    //     `07046f66660d0000...` (zero-padded to the fixed HID report size).
    //     Root cause unknown; deliberately not chased this pass.
    //   - Settings::on_addButton() ("New") sets the (no longer hidden)
    //     comboBoxPrototype's current text to the literal string
    //     `"names[0]"` -- dead placeholder, never indexed into a real list.
    //     Harmless now that the combo box is visible and user-selectable,
    //     but still worth fixing properly.
    //   - Reported, not yet diagnosed: Customize tab's controls "not laid
    //     out cleanly" at runtime (no screenshot yet to compare against the
    //     .ui markup, which looks structurally normal on its own).
    //
    // See BUILDINFO.md's "Known issues" for the full writeup (matching the
    // S21-tab entry's depth) -- this comment is the short version. Flip
    // g_developerMode back to `true` here once the two STILL BROKEN items
    // above are actually fixed, not before; the args.contains() check itself
    // is left in place so that's a one-line change.
    if (args.contains("-developer")) {
        //g_developerMode = true;
        //MAX_DOTS = 1000000;
    }
    if (args.contains("-usb-only")) {
        g_usbOnly = true;
    }

    g_raspbian = QSysInfo::productType().contains("raspbian", Qt::CaseInsensitive);

    // Read the persisted theme before building any stylesheet below --
    // MainWindow doesn't exist yet to do this itself, and Style::m_dark
    // otherwise defaults to dark, so a saved Light choice would flash (and
    // partly stick, for the app-wide QMessageBox/QDialog stylesheet below)
    // as dark on startup.
    {
        QSettings settings(Settings::setIniFile(), QSettings::IniFormat);
        settings.beginGroup("Settings");
        Style::setDarkMode(settings.value("darkColorTheme", true).toBool());
        settings.endGroup();
    }

    QString style;
    style = Style::messageBox();
    a.setStyleSheet(style);

    style = Style::dialog();
    style += Style::pushButton();
    style += Style::label();
    style += Style::lineEdit();
    a.setStyleSheet(style);

    MainWindow w;
    g_mainWindow = w.m_mainWindow;

    foreach (QString path, args) {
        if (path.contains(".asd")) {
            w.openFile(path);
            break;
        }
    }
    w.show();

    return a.exec();
}
