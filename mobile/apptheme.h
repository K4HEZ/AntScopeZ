#ifndef APPTHEME_H
#define APPTHEME_H

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// User-facing light/dark override, persisted separately from the desktop
// app's own settings (see mobilesettings.h). "System" follows the OS -- Main.qml
// maps this string to QtQuick.Controls.Material's own Material.Light/Dark/System
// enum, so this class doesn't need to know anything about that module.
class AppTheme : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // One of "Light", "Dark", "System".
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)

public:
    explicit AppTheme(QObject* parent = nullptr);

    QString themeMode() const { return m_themeMode; }
    void setThemeMode(const QString& mode);

signals:
    void themeModeChanged();

private:
    QString m_themeMode = QStringLiteral("System");
};

#endif // APPTHEME_H
