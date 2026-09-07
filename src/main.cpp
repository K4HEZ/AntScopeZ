                                                                                                                                    #include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QAbstractNativeEventFilter>
#include <QIcon>
#include "analyzer/customanalyzer.h"
#include "settings.h"
#include "style.h"
#include <QSettings>

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
    return msgBox.exec();
}

int main(int argc, char *argv[])
{
    qputenv("QT_ACCESSIBILITY", "0");

    // Fix for 4K Display Issues Disabled
    QApplication a(argc, argv);

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

    // -developer/g_developerMode removed 2026-09-07 -- Custom Analyzer no
    // longer depends on it and is live now. Debug Logging is reachable via
    // Settings > Developer regardless. See BUILDINFO.md's "Developer mode"
    // and "Known issues" for detail/remaining Custom Analyzer bugs.
    if (args.contains("-usb-only")) {
        g_usbOnly = true;
    }

    // -remote-api-port <n>: force the Remote API on regardless of the
    // persisted Settings toggle (json-tcp-api branch) -- for headless/dev
    // use, e.g. paired with -headless below. Silently ignored if malformed
    // (missing value, non-numeric, out of range) rather than refusing to
    // start the app over a CLI typo; applied once MainWindow exists, below.
    int remoteApiPortOverride = -1; // -1 == no override requested
    int remoteApiPortFlagIndex = args.indexOf("-remote-api-port");
    if (remoteApiPortFlagIndex != -1 && remoteApiPortFlagIndex + 1 < args.size()) {
        bool ok = false;
        int port = args.at(remoteApiPortFlagIndex + 1).toInt(&ok);
        if (ok && port > 0 && port <= 65535)
            remoteApiPortOverride = port;
    }
    // -headless: skip w.show() only (see json-tcp-api's plan doc for why
    // this is deliberately narrower than a full GUI-decoupled headless
    // mode) -- MainWindow/AnalyzerPro are still built normally. Does NOT
    // itself enable the Remote API; pair with -remote-api-port (or an
    // already-persisted "Enable Remote API" setting) or the process runs
    // with nothing visible and nothing reachable.
    bool headless = args.contains("-headless");

    g_raspbian = QSysInfo::productType().contains("raspbian", Qt::CaseInsensitive);

    // Read the persisted theme before building any stylesheet below --
    // MainWindow doesn't exist yet to do this itself, and Style::m_activeIndex
    // otherwise defaults to Light (0), so a saved non-default choice would
    // flash (and partly stick, for the app-wide QMessageBox/QDialog
    // stylesheet below) as Light on startup.
    {
        QSettings settings(Settings::setIniFile(), QSettings::IniFormat);
        settings.beginGroup("Settings");
        Style::setActiveThemeIndex(settings.value("activeTheme", 0).toInt());
        settings.endGroup();
    }

    // qApp->setStyleSheet() *replaces* rather than merges with a previous
    // call -- this used to be two separate calls here (messageBox() alone,
    // then dialog()+pushButton()+label()+lineEdit()), which silently made
    // messageBox() dead from the moment the app started. One combined call
    // now; see Style::globalStyleSheet()'s own comment for why this and
    // MainWindow::changeColorTheme() are the only two places allowed to
    // call qApp->setStyleSheet() at all.
    a.setPalette(Style::palette());
    a.setStyleSheet(Style::globalStyleSheet());

    MainWindow w;
    g_mainWindow = w.m_mainWindow;

    if (remoteApiPortOverride != -1)
        w.setRemoteApiEnabled(true, static_cast<quint16>(remoteApiPortOverride));

    foreach (QString path, args) {
        if (path.contains(".asd")) {
            w.openFile(path);
            break;
        }
    }
    if (!headless)
        w.show();

    return a.exec();
}
