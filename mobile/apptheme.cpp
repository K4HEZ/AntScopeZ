#include "apptheme.h"

#include "mobilesettings.h"

AppTheme::AppTheme(QObject* parent)
    : QObject(parent)
{
    const QString mode = mobileSettings().value("ui/themeMode", "System").toString();
    if (mode == "Light" || mode == "Dark" || mode == "System")
        m_themeMode = mode;
}

void AppTheme::setThemeMode(const QString& mode)
{
    if (mode != "Light" && mode != "Dark" && mode != "System")
        return;
    if (m_themeMode == mode)
        return;
    m_themeMode = mode;
    mobileSettings().setValue("ui/themeMode", mode);
    emit themeModeChanged();
}
