#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("AntScopeZ");
    app.setApplicationName("AntScopeZ");
    app.setApplicationVersion(ANTSCOPEZ_VER);

    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("AntScopeZ.Mobile", "Main");
    return app.exec();
}
