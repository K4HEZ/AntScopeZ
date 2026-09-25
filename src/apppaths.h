#ifndef APPPATHS_H
#define APPPATHS_H

#include <QString>

// UI-free home for the app's on-disk locations, so core code (analyzer/,
// debug log) doesn't need Settings or FileDialog. Those keep their old
// static functions as thin forwards to these.
namespace AppPaths {

QString localDataFolder();
QString localDataPath(const QString &fileName);
// Path to AntScopeZ.ini; runs the one-time legacy-layout migration on Linux.
QString iniFile();

// See FileDialog::userDataDir() and friends for what these mean.
QString userDataDir();
void setUserDataDir(const QString &dir);
bool userDataDirFollowsSaves();
void setUserDataDirFollowsSaves(bool follows);

}

#endif // APPPATHS_H
