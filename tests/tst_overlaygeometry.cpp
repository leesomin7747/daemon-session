#include "OverlayWindow.h"

#include <QGuiApplication>
#include <QTest>
#include <limits>

class OverlayGeometryTest : public QObject
{
    Q_OBJECT

private slots:
    void clickRegionFollowsPet()
    {
        OverlayWindow window(QGuiApplication::primaryScreen());
        window.resize(800, 180);
        window.setInteractiveRegions({QRectF(20, 90, 72, 72)});
        QVERIFY(window.mask().contains(QPoint(40, 120)));
        QVERIFY(!window.mask().contains(QPoint(300, 120)));

        window.setInteractiveRegions({QRectF(300, 90, 72, 72)});
        QVERIFY(!window.mask().contains(QPoint(40, 120)));
        QVERIFY(window.mask().contains(QPoint(320, 120)));
    }

    void boundsAt125Percent()
    {
        OverlayWindow window(QGuiApplication::primaryScreen());
        window.resize(800, 180);
        window.setInteractiveRegions({QRectF(10.25, 10.25, 72.5, 72.5)});
        QCOMPARE(window.mask(), QRegion(QRect(10, 10, 73, 73)));
        QVERIFY(window.mask().contains(QPoint(82, 82)));
        QVERIFY(!window.mask().contains(QPoint(83, 83)));
        if (qEnvironmentVariable("QT_SCALE_FACTOR") == "1.25") {
            QCOMPARE(window.devicePixelRatio(), 1.25);
        }
    }

    void regionsAreClippedAndInvalidAreasIgnored()
    {
        OverlayWindow window(QGuiApplication::primaryScreen());
        window.resize(800, 180);
        window.setInteractiveRegions({QRectF(-10, 10, 20, 20), QRectF(790, 170, 30, 30),
                                      QRectF(200, 20, -10, 10),
                                      QRectF(std::numeric_limits<double>::quiet_NaN(), 0, 10, 10)});
        QVERIFY(window.mask().contains(QPoint(0, 10)));
        QVERIFY(window.mask().contains(QPoint(799, 179)));
        QVERIFY(!window.mask().contains(QPoint(-1, 10)));
        QVERIFY(!window.mask().contains(QPoint(800, 179)));
        QVERIFY(!window.mask().contains(QPoint(195, 25)));
    }

    void emptyRegionPassesAllInput()
    {
        OverlayWindow window(QGuiApplication::primaryScreen());
        window.resize(800, 180);
        window.setInteractiveRegions({QRectF(20, 90, 72, 72)});
        QVERIFY(!window.flags().testFlag(Qt::WindowTransparentForInput));
        window.setInteractiveRegions({});
        QVERIFY(window.flags().testFlag(Qt::WindowTransparentForInput));
        window.setInteractiveRegions({QRectF(300, 90, 72, 72)});
        QVERIFY(!window.flags().testFlag(Qt::WindowTransparentForInput));
    }

    void passiveWindowOnlyAcceptsKeyboardWhenEditing()
    {
        OverlayWindow window(QGuiApplication::primaryScreen());
        QVERIFY(window.flags().testFlag(Qt::WindowDoesNotAcceptFocus));
        window.setEditorActive(true);
        QVERIFY(!window.flags().testFlag(Qt::WindowDoesNotAcceptFocus));
        window.setEditorActive(false);
        QVERIFY(window.flags().testFlag(Qt::WindowDoesNotAcceptFocus));
    }

    void projectXStaysInBounds()
    {
        QCOMPARE(projectX(0, 800, 72, 12), 12.0);
        QCOMPARE(projectX(1, 800, 72, 12), 716.0);
        QCOMPARE(projectX(0.5, 800, 72, 12), 364.0);
        QCOMPARE(projectX(-1, 800, 72, 12), 12.0);
        QCOMPARE(projectX(2, 800, 72, 12), 716.0);
        QCOMPARE(projectX(1, 40, 72, 12), 0.0);
        QCOMPARE(projectX(0.5, 0, 72, 12), 0.0);
        QCOMPARE(projectX(std::numeric_limits<double>::quiet_NaN(), 800, 72, 12), 364.0);
    }

    void editorReturnsFocusToPreviousWindow()
    {
        QWindow work;
        work.show();
        work.requestActivate();
        QTRY_VERIFY(work.isActive());
        OverlayWindow overlay(QGuiApplication::primaryScreen());
        overlay.setInteractiveRegions({QRectF(0, 0, 300, 80)});
        overlay.show();
        overlay.setEditorActive(true);
        QTRY_VERIFY(overlay.isActive());
        overlay.setEditorActive(false);
        QTRY_VERIFY_WITH_TIMEOUT(work.isActive(), 1000);
    }

    void explicitEditCanRegainFocus()
    {
        QWindow work;
        work.show();
        work.requestActivate();
        QTRY_VERIFY(work.isActive());
        OverlayWindow overlay(QGuiApplication::primaryScreen());
        overlay.setInteractiveRegions({QRectF(0, 0, 300, 80)});
        overlay.show();
        overlay.setEditorActive(true);
        QTRY_VERIFY(overlay.isActive());
        work.requestActivate();
        QTRY_VERIFY(work.isActive());
        overlay.setEditorActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(overlay.isActive(), 1000);
    }
};

QTEST_MAIN(OverlayGeometryTest)
#include "tst_overlaygeometry.moc"
