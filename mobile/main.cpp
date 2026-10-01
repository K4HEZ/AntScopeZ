#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "build-timestamp.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("AntScopeZ");
    app.setApplicationName("AntScopeZ");
    app.setApplicationVersion(ANTSCOPEZ_VER);

    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("buildTimestamp", QStringLiteral(ANTSCOPEZ_BUILD_TIMESTAMP));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("AntScopeZ.Mobile", "Main");
    return app.exec();
}
