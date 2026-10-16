#include "OverlayWindow.h"
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickItem>
#include <QTest>

class OverlayQmlTest : public QObject
{
    Q_OBJECT

private slots:
    void editorMaskFollowsMovedPet()
    {
        OverlayWindow window(QGuiApplication::primaryScreen());
        window.resize(800, 180);
        window.rootContext()->setContextProperty("overlayWindow", &window);
        window.setSource(QUrl::fromLocalFile(QStringLiteral(PROBE_QML_FILE)));
        QCOMPARE(window.status(), QQuickView::Ready);
        QVERIFY(QMetaObject::invokeMethod(window.rootObject(), "edit"));
        QCoreApplication::processEvents();
        QVERIFY(window.mask().contains(QPoint(200, 40)));

        QVERIFY(QMetaObject::invokeMethod(window.rootObject(), "movePet"));
        QCoreApplication::processEvents();
        QVERIFY2(window.mask().contains(QPoint(700, 40)), "New editor close-button area must receive input");
        QVERIFY2(!window.mask().contains(QPoint(200, 40)), "Vacated editor area must pass input through");
    }
};

QTEST_MAIN(OverlayQmlTest)
#include "tst_overlayqml.moc"
