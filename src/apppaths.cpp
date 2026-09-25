#include "apppaths.h"
#include "appconfig.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>


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
    if (!AppConfig::get().raspbian) {
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
    if (AppConfig::get().raspbian)
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
    if (AppConfig::get().raspbian)
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

// Read-only data shipped with the app: cables.txt, itu-regions-defaults.txt,
// the .qm translation files. A .deb (or plain `cmake --install`) ships
// these under ANTSCOPE_SHARED_DATA_DIR -- CMAKE_INSTALL_FULL_DATADIR at
// build time, i.e. wherever CMAKE_INSTALL_PREFIX actually resolved to
// (/usr/share/antscopez for the packaging default of /usr, but this stays
// correct even if someone installs to a different prefix). Prefer that if
// it's there, otherwise fall back to sitting next to the binary, which is
// how an un-installed dev build (build-debug/build-release) stages them.
QString AppPaths::sharedDataFolder()
{
#ifdef ANTSCOPE_SHARED_DATA_DIR
    if (QDir(ANTSCOPE_SHARED_DATA_DIR).exists())
        return ANTSCOPE_SHARED_DATA_DIR;
#endif
    return QCoreApplication::applicationDirPath();
}

QString AppPaths::languageDataFolder()
{
#ifdef Q_OS_LINUX
    if (AppConfig::get().raspbian)
    {
        return "/usr/share/AntScopeZ";
    }
#endif
    // Was: return localDataFolder() on non-raspbian Linux, which resolves to
    // *one directory above* the binary -- correct for user data (ini/
    // calibration files), which is deliberately kept outside any specific
    // build directory, but wrong here: the .qm translation files are staged
    // directly next to the binary by CMake, i.e. in applicationDirPath()
    // itself (which is what every other platform already used). This only
    // "worked" for build layouts exactly one directory below the repo root
    // (which also happens to hold checked-in .qm copies) -- e.g. a plain
    // `build-debug/`. Qt Creator's default shadow-build layout
    // (build/<kit>/AntScopeZ) sits one directory deeper, so "one directory
    // up" landed on the empty build/ folder instead, QTranslator::load()
    // failed silently, and the UI stayed untranslated regardless of the
    // Language setting.
    //
    // Now prefers the installed /usr/share/antscopez (see
    // sharedDataFolder()) so a .deb-installed copy finds its .qm files
    // there instead of needing them next to /usr/bin/AntScopeZ.
    return sharedDataFolder();
}

QString AppPaths::programDataPath(const QString &_fileName)
{
// Linux and macOS -- read-only data shipped with the app (cables.txt,
// itu-regions-defaults.txt). itu-regions.txt is *not* one of these: it's
// the user's own band edits, and lives in localDataPath() instead (see
// Settings::loadItuBands()/saveItuBands()). On macOS CMake stages these in
// Contents/MacOS next to the binary, which sharedDataFolder() returns.
#if defined(Q_OS_LINUX) || defined(Q_OS_DARWIN)
    QDir dir0 = sharedDataFolder();
    return dir0.absoluteFilePath(_fileName);
#endif

    QString configDataDirString = QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation).at(1);
    QDir dir1(configDataDirString); // "C:/ProgramData/<APPNAME>"
    dir1.cdUp(); // cd ..
    return dir1.absoluteFilePath("AntScopeZ/" + _fileName);
}

QList<QPair<QString, QString>> AppPaths::availableLanguages()
{
    QList<QPair<QString, QString>> result;
    // English is always offered: it's the source language every tr() call
    // is written in, so there's no QtLanguage_en.qm to discover below.
    result << qMakePair(QString("English"), QString("en"));

    // Every other entry is discovered from whatever QtLanguage_<code>.qm
    // files actually exist, rather than a fixed compiled-in list -- a
    // language becomes selectable just by dropping its .qm into either
    // folder, no rebuild needed. localDataFolder() (per-user, e.g.
    // ~/.config/AntScopeZ) and languageDataFolder() (shared/installed
    // copy) are both scanned so an override in the former still shows up
    // even if the code isn't among the ones shipped in the latter;
    // loadLanguage() (mainwindow.cpp) is what actually prefers the user
    // copy at load time if a code exists in both.
    QStringList codes;
    for (const QString& folder : {localDataFolder(), languageDataFolder()}) {
        QDir dir(folder);
        const QStringList files = dir.entryList(QStringList() << "QtLanguage_*.qm", QDir::Files);
        for (const QString& fileName : files) {
            QString code = fileName.mid(QStringLiteral("QtLanguage_").length());
            code.chop(QStringLiteral(".qm").length());
            if (!code.isEmpty() && code != "en" && !codes.contains(code))
                codes << code;
        }
    }
    std::sort(codes.begin(), codes.end());

    for (const QString& code : codes) {
        // The .qm/.ts format has no human-readable name field of its own
        // (QTranslator::language() just returns this same code back) --
        // QLocale supplies the display name instead, in the language's
        // own script (matching how "English"/"Українська"/"日本語" looked
        // before this was discovery-based). Falls back to the bare code
        // for one QLocale doesn't recognize, rather than dropping it.
        QString name = QLocale(code).nativeLanguageName();
        result << qMakePair(name.isEmpty() ? code : name, code);
    }
    return result;
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
