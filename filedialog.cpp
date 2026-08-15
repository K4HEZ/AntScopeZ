#include "filedialog.h"
#include "style.h"
#include "settings.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

namespace {
// Cached after first load; kept in sync by setUserDataDir() so a Settings
// dialog change (or a save that "follows saves") is visible to the very
// next call without re-reading the ini every time.
QString g_userDataDir;
bool g_userDataDirLoaded = false;
}

FileDialog::FileDialog(QObject *parent)
    : QObject{parent}
{}

QString FileDialog::getOpenFileName(QWidget *parent,
                               const QString &caption,
                               const QString &dir,
                               const QString &filter,
                               QString *selectedFilter)
{
    QString name;
    QFileDialog dlg(parent);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    dlg.setWindowTitle(caption);
    // `dir` may be a plain directory, or (less commonly for Open, but
    // handled the same way as getSaveFileName() below for consistency) a
    // directory plus a filename to preselect. QFileDialog's own static
    // convenience functions split those apart automatically; the instance
    // API used here (needed for the shared styling applied below) does
    // not -- setDirectory() alone silently fails to navigate anywhere if
    // given a path that isn't literally an existing directory.
    QFileInfo fi(dir);
    if (fi.isDir()) {
        dlg.setDirectory(dir);
    } else {
        dlg.setDirectory(fi.path());
        dlg.selectFile(fi.fileName());
    }
    dlg.setNameFilter(filter);
    QString style;
    style += Style::dialog();
    style += Style::pushButton();
    dlg.setStyleSheet(style);

    if (dlg.exec() == QDialog::Accepted) {
        name = dlg.selectedFiles().constFirst();
    }

    Q_UNUSED(selectedFilter);
    return name;
}

QString FileDialog::getSaveFileName(QWidget *parent,
                                    const QString &caption,
                                    const QString &dir,
                                    const QString &filter,
                                    QString *selectedFilter)
{
    QString name;
    QFileDialog dlg(parent);

    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setWindowTitle(caption);

    // `dir` is normally a directory plus a suggested filename (e.g.
    // FileDialog::userDataDir() + "/" + suggestedName + ".ext"). Passing
    // that whole string straight to setDirectory() -- as this used to do
    // -- doesn't work: setDirectory() expects an actual existing
    // directory, and since the full path (with its nonexistent suggested
    // filename on the end) never literally exists on disk, it silently
    // fails to navigate there at all, leaving the dialog wherever Qt's own
    // fallback happens to be. That's the confirmed cause of every
    // Save-type dialog in the app (Export, Print, Screenshot, .asd Save,
    // main-window screenshot capture) not actually defaulting to
    // UserDataDir despite it being passed in correctly. Split directory
    // and filename apart explicitly instead (isDir() also covers the case
    // of a caller passing a bare directory with nothing to preselect).
    QFileInfo fi(dir);
    if (fi.isDir()) {
        dlg.setDirectory(dir);
    } else {
        dlg.setDirectory(fi.path());
        dlg.selectFile(fi.fileName());
    }
    dlg.setNameFilter(filter);

    QString style;
    style += Style::dialog();
    style += Style::pushButton();
    dlg.setStyleSheet(style);

    if (dlg.exec() == QDialog::Accepted) {
        name = dlg.selectedFiles().constFirst();
    }

    Q_UNUSED(selectedFilter);
    return name;
}

QString FileDialog::getExistingDirectory(QWidget *parent,
                                    const QString &caption,
                                    const QString &dir,
                                    QFileDialog::Options options)
{
    QString name;
    QFileDialog dlg(parent);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    dlg.setWindowTitle(caption);
    dlg.setDirectory(dir);
    dlg.setOptions(options);

    QString style;
    style += Style::dialog();
    style += Style::pushButton();
    dlg.setStyleSheet(style);

    if (dlg.exec() == QDialog::Accepted) {
        name = dlg.selectedFiles().constFirst();
    }

    return name;
}

QString FileDialog::userDataDir()
{
    if (!g_userDataDirLoaded) {
        QSettings settings(Settings::setIniFile(), QSettings::IniFormat);
        settings.beginGroup("General");
        g_userDataDir = settings.value("UserDataDir", "").toString();
        settings.endGroup();

        if (g_userDataDir.isEmpty()) {
            g_userDataDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                             + "/AntScopeZ";
        }
        QDir().mkpath(g_userDataDir);
        g_userDataDirLoaded = true;
    }
    return g_userDataDir;
}

void FileDialog::setUserDataDir(const QString &dir)
{
    if (dir.isEmpty() || dir == g_userDataDir)
        return;

    g_userDataDir = dir;
    g_userDataDirLoaded = true;
    QDir().mkpath(g_userDataDir);

    QSettings settings(Settings::setIniFile(), QSettings::IniFormat);
    settings.beginGroup("General");
    settings.setValue("UserDataDir", g_userDataDir);
    settings.endGroup();
}

void FileDialog::noteUserDataDirIfEnabled(const QString &savedFilePath)
{
    if (savedFilePath.isEmpty() || !userDataDirFollowsSaves())
        return;

    setUserDataDir(QFileInfo(savedFilePath).path());
}

bool FileDialog::userDataDirFollowsSaves()
{
    QSettings settings(Settings::setIniFile(), QSettings::IniFormat);
    settings.beginGroup("General");
    bool follows = settings.value("UserDataDirFollowsSaves", false).toBool();
    settings.endGroup();
    return follows;
}

void FileDialog::setUserDataDirFollowsSaves(bool follows)
{
    QSettings settings(Settings::setIniFile(), QSettings::IniFormat);
    settings.beginGroup("General");
    settings.setValue("UserDataDirFollowsSaves", follows);
    settings.endGroup();
}

QString FileDialog::withExtension(const QString &path, const QString &ext)
{
    if (path.isEmpty())
        return path;

    QFileInfo fi(path);
    QString dir = fi.path();
    QString base = fi.completeBaseName();
    while (base.endsWith("." + ext, Qt::CaseInsensitive))
        base.chop(ext.length() + 1);

    QString result = (dir.isEmpty() || dir == ".") ? base : dir + "/" + base;
    return result + "." + ext;
}
