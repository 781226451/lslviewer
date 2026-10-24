#include "viewercontroller.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("LSL Viewer"));
    QGuiApplication::setOrganizationName(QStringLiteral("LSLViewer"));

    ViewerController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("viewer"), &controller);
    // Embedded from ../qml/Main.qml at build time; see qt_add_resources in CMakeLists.txt.
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        controller.shutdown();
        return 1;
    }
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &controller, &ViewerController::shutdown);
    return app.exec();
}
