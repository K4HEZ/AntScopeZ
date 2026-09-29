#ifndef MOBILESETTINGS_H
#define MOBILESETTINGS_H

#include <QSettings>

// Deliberately not the default QSettings() -- main.cpp sets the app
// identity to "AntScopeZ"/"AntScopeZ", same as the desktop app. A distinct
// organization/application here keeps mobile's persisted settings in their
// own file instead of sharing (and risking key collisions with) the
// desktop app's real config.
inline QSettings mobileSettings()
{
    return QSettings("AntScopeZ", "AntScopeZMobile");
}

#endif // MOBILESETTINGS_H
