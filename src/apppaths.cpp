#include "apppaths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

extern bool g_raspbian; // main.cpp

namespace {
// Cached after first load; kept in sync by setUserDataDir() so a Settings
// dialog change (or a save that "follows saves") is visible to the very
// next call without re-reading the ini every time.
QString g_userDataDir;
bool g_userDataDirLoaded = false;
}

QString AppPaths::iniFile()
{
    QString newPath = localDataPath("AntScopeZ.ini");

#ifdef Q_OS_LINUX
    // One-time migration of the pre-2.1.4 layout -- AntScope2.ini/
    // Calibration/itu-regions.txt sitting next to the binary (see the
    // comment on localDataFolder()) -- into the current AntScopeZ location.
    // Idempotent (guarded by "does the new copy already exist"), and cheap
    // enough to just always check since setIniFile() already runs on every
    // Settings/Calibration construction. The 2.1.4-era org-directory layout
    // (~/.config/<old-org-name>/AntScope2, from when
    // QCoreApplication::setOrganizationName() was still set) had its own
    // migration step here too, but that layout's no longer in use by anyone
    // and was removed rather than kept around as dead code.
    // Both "AntScope2.ini" and "antscope2.ini" are checked -- Settings and
    // Calibration briefly used differently-cased filenames that only
    // diverged into two separate files on case-sensitive filesystems (issue
    // #43); by this point any surviving mismatch is rare enough that a
    // plain first-one-found rename is fine rather than the more careful
    // per-key fold this used to do.
    if (!g_raspbian) {
        QString newDirPath = localDataFolder();
        QDir legacyBinaryDir(QCoreApplication::applicationDirPath() + "/..");
        QString oldDirPath = legacyBinaryDir.canonicalPath();
        if (!oldDirPath.isEmpty() && oldDirPath != newDirPath && QDir(oldDirPath).exists()) {
            QDir oldDir(oldDirPath);
            const QStringList legacyFiles = {"AntScope2.ini", "antscope2.ini", "itu-regions.txt"};
            for (const QString& name : legacyFiles) {
                QString oldFile = oldDir.absoluteFilePath(name);
                QString newName = (name == "itu-regions.txt") ? name : "AntScopeZ.ini";
                QString newFile = QDir(newDirPath).absoluteFilePath(newName);
                if (QFile::exists(oldFile) && !QFile::exists(newFile)) {
                    QFile::rename(oldFile, newFile);
                }
            }
            QString oldCalib = oldDir.absoluteFilePath("Calibration");
            QString newCalib = QDir(newDirPath).absoluteFilePath("Calibration");
            if (QDir(oldCalib).exists() && !QDir(newCalib).exists()) {
                QDir().rename(oldCalib, newCalib);
            }
        }
    }
#endif

    return newPath;
}

QString AppPaths::localDataPath(const QString &_fileName)
{
// Mac OS X and iOS
#ifdef Q_OS_DARWIN
    QDir dir_ini3 = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    dir_ini3.mkpath("AntScopeZ"); // fresh install: saving itu-regions.txt needs it
    return dir_ini3.absoluteFilePath("AntScopeZ/" + _fileName);
#endif

// Linux
#ifdef Q_OS_LINUX
    if (g_raspbian)
    {
        return "/usr/share/AntScopeZ/" + _fileName;
    }
    QDir dir = localDataFolder();
    return dir.absoluteFilePath(_fileName);
#endif

// Windows
#ifdef Q_OS_WIN
    // QDir dir_ini1 = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    // return dir_ini1.absoluteFilePath("AntScopeZ/" + _fileName);
    QDir dir_ini1 = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    // return dir_ini1.absoluteFilePath("AntScopeZ/" + _fileName);
    return dir_ini1.absoluteFilePath(_fileName);

#endif
  qDebug("TODO AppPaths::localDataPath");
  return QString();
}

QString AppPaths::localDataFolder()
{
// Mac OS X and iOS
#ifdef Q_OS_DARWIN
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
#endif
// Linux
#ifdef Q_OS_LINUX
    if (g_raspbian)
    {
        return "/usr/share/AntScopeZ/";
    }
    // ~/.config/AntScopeZ (QCoreApplication::setApplicationName() in
    // main.cpp; deliberately no organization name, so there's no extra
    // directory level). Was "next to the binary" (applicationDirPath()/..)
    // -- convenient for a dev build, but wrong for an installed package: no
    // write access, and shared across every user of the machine.
    // AppConfigLocation doesn't create the directory for you (unlike the
    // old path, which always existed), and Calibration::init()'s
    // QDir::mkdir("Calibration") needs its parent to already exist, so
    // create it here. See iniFile() for the one-time migration of older
    // installs' data out of prior locations.
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(path);
    return path;
#endif
// Windows
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif
  qDebug("TODO AppPaths::localDataFolder");
  return QString();
}

QString AppPaths::userDataDir()
{
    if (!g_userDataDirLoaded) {
        QSettings settings(iniFile(), QSettings::IniFormat);
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

void AppPaths::setUserDataDir(const QString &dir)
{
    if (dir.isEmpty() || dir == g_userDataDir)
        return;

    g_userDataDir = dir;
    g_userDataDirLoaded = true;
    QDir().mkpath(g_userDataDir);

    QSettings settings(iniFile(), QSettings::IniFormat);
    settings.beginGroup("General");
    settings.setValue("UserDataDir", g_userDataDir);
    settings.endGroup();
}

bool AppPaths::userDataDirFollowsSaves()
{
    QSettings settings(iniFile(), QSettings::IniFormat);
    settings.beginGroup("General");
    bool follows = settings.value("UserDataDirFollowsSaves", false).toBool();
    settings.endGroup();
    return follows;
}

void AppPaths::setUserDataDirFollowsSaves(bool follows)
{
    QSettings settings(iniFile(), QSettings::IniFormat);
    settings.beginGroup("General");
    settings.setValue("UserDataDirFollowsSaves", follows);
    settings.endGroup();
}
