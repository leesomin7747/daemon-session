#include "OverlayWindow.h"
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QScreen>
#include <QTimer>

int main(int argc, char* argv[])
{
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Daemon Overlay Probe");
    const bool smoke = app.arguments().contains("--smoke-test");
#ifdef Q_OS_LINUX
    if (!smoke && !QGuiApplication::platformName().startsWith("wayland")) {
        qCritical("This probe requires native Wayland. Set QT_QPA_PLATFORM=wayland.");
        return 1;
    }
#endif
    const auto* screen = app.primaryScreen();
    if (!screen)
        return 1;
    OverlayWindow overlay(app.primaryScreen());
    overlay.setTitle("Daemon overlay");
    overlay.rootContext()->setContextProperty("overlayWindow", &overlay);
    overlay.setSource(QUrl("qrc:/probe/Probe.qml"));
    if (overlay.status() != QQuickView::Ready)
        return 1;

    QQuickView workbench;
    workbench.setTitle("Daemon Overlay Probe — Workbench");
    workbench.setResizeMode(QQuickView::SizeRootObjectToView);
    workbench.setGeometry(screen->availableGeometry().adjusted(0, 30, 0, 0));
    workbench.rootContext()->setContextProperty("scene", overlay.rootObject());
    workbench.rootContext()->setContextProperty("environment", QString("Qt %1 · %2 · DPR %3")
        .arg(qVersion(), QGuiApplication::platformName()).arg(overlay.devicePixelRatio()));
    workbench.setSource(QUrl("qrc:/probe/Workbench.qml"));
    if (workbench.status() != QQuickView::Ready)
        return 1;
    QObject::connect(workbench.engine(), &QQmlEngine::quit, &app, &QCoreApplication::quit);
    QObject::connect(overlay.engine(), &QQmlEngine::quit, &app, &QCoreApplication::quit);
    QObject::connect(&workbench, &QQuickWindow::closing, &app, &QCoreApplication::quit);
    if (smoke)
        QTimer::singleShot(0, workbench.engine(), &QQmlEngine::quit);
    workbench.show();
    overlay.show();
    qInfo() << "PROBE" << qVersion() << QGuiApplication::platformName()
            << "DPR" << overlay.devicePixelRatio() << "available" << screen->availableGeometry();
    QObject::connect(screen, &QScreen::availableGeometryChanged, &app, [](const QRect& area) {
        qInfo() << "AVAILABLE" << area;
    });
    return app.exec();
}
