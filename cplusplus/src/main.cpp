#include "viewercontroller.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("LSL Viewer"));
    QGuiApplication::setOrganizationName(QStringLiteral("LSLViewer"));

    ViewerController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("viewer"), &controller);
    engine.loadFromModule("LslViewer", "Main");
    if (engine.rootObjects().isEmpty()) {
        controller.shutdown();
        return 1;
    }
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &controller, &ViewerController::shutdown);
    return app.exec();
}