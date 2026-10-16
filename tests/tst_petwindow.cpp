#include "OverlayWindow.h"
#include "SceneModel.h"
#include "SessionFixture.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QScreen>
#include <QDir>
#include <QQmlComponent>
#include <QFile>
#include <QtTest>

class PetWindowTest : public QObject {
    Q_OBJECT
    QQuickItem* visualChild(QQuickItem* root,const QString& name) {
        if(root->objectName()==name) return root;
        for(auto* child:root->childItems()) if(auto* found=visualChild(child,name)) return found;
        return nullptr;
    }
    void load(OverlayWindow& window, SceneModel& model) {
        window.setContentHeight(380);
        window.resize(800,380);
        window.setInitialProperties({{"model",QVariant::fromValue(&model)},
            {"nativeOverlay",QVariant::fromValue(&window)},{"reducedMotion",true}});
        window.setSource(QUrl::fromLocalFile(PET_MAIN_FILE));
        QCOMPARE(window.status(),QQuickView::Ready);
        window.show(); QCoreApplication::processEvents();
    }
private slots:
    void initTestCase() {
        qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
#ifdef Q_OS_MAC
        QGuiApplication::setFont(QFont("Apple SD Gothic Neo"));
#endif
        qmlRegisterType<SceneModel>("Daemon",1,0,"SceneModel");
    }
    void contentHeightKeepsBottomAnchor() {
        OverlayWindow window(QGuiApplication::primaryScreen());
        const auto bottom=window.geometry().bottom();
        window.setContentHeight(380); QCOMPARE(window.height(),380); QCOMPARE(window.geometry().bottom(),bottom);
        window.setContentHeight(-1); QCOMPARE(window.height(),380);
    }
    void movedPetMaskDropsVacatedArea() {
        SceneModel model; model.replaceSnapshot(connectedSession());
        OverlayWindow window(QGuiApplication::primaryScreen()); load(window,model);
        QVERIFY(window.mask().contains(QPoint(554,300)));
        const SessionEvent e{"e","r","epoch-1",1,Action::Rope,"a","b","가까이","어어",0.37,false,"",false};
        model.applyEvent(e); QCoreApplication::processEvents();
        QVERIFY(window.mask().contains(QPoint(296,300)));
        QVERIFY(!window.mask().contains(QPoint(554,300)));
        QVERIFY(!window.mask().contains(QPoint(20,20)));
    }
    void menuCloseKeepsBothPetsRendered() {
        SceneModel model; model.setLocalPeerId("a"); model.replaceSnapshot(connectedSession());
        OverlayWindow window(QGuiApplication::primaryScreen()); load(window,model);
        const auto pixel=[&](const QImage& image) {
            auto* source=visualChild(window.rootObject(),"pet_a");
            return image.pixelColor(int((source->x()+25)*window.devicePixelRatio()),
                                    int((source->y()+70)*window.devicePixelRatio()));
        };
        QTest::qWait(100); QCOMPARE(pixel(window.grabWindow()),QColor("#bad8ba"));
        QVERIFY(QMetaObject::invokeMethod(window.rootObject(),"openMenu",Q_ARG(QVariant,QVariant("b"))));
        QTest::qWait(100); window.grabWindow();
        QVERIFY(QMetaObject::invokeMethod(window.rootObject(),"closeMenu"));
        const SessionEvent e{"e","r","epoch-1",1,Action::Rope,"a","b","가까이","어어",0.37,false,"",false};
        model.applyEvent(e); QTest::qWait(100);
        const auto image=window.grabWindow();
        QVERIFY(window.mask().contains(QPoint(215,324)));
        QCOMPARE(pixel(image),QColor("#bad8ba"));
    }
    void receivedEventKeepsSeparateWorkbenchFocus() {
        if(QGuiApplication::platformName()=="offscreen") QSKIP("Requires real native application focus");
        const auto path=qEnvironmentVariable("PET_EXISTING_PREVIEW_LOG");
        if(path.isEmpty()) QSKIP("Start a separate preview process and supply its focus log");
        const auto readLog=[&] {QFile file(path); if(!file.open(QIODevice::ReadOnly)) return QByteArray(); return file.readAll();};
        const auto baseline=readLog().size(); QVERIFY(baseline>0);
        QQuickView work;
        QQmlComponent component(work.engine());
        component.setData("import QtQuick\nRectangle { width: 480; height: 200; color: '#203028'; TextInput { objectName: 'input'; anchors.fill: parent; focus: true; color: 'white' } }",QUrl());
        auto* item=qobject_cast<QQuickItem*>(component.create()); QVERIFY(item);
        work.setContent(QUrl(),&component,item); work.setTitle("Daemon QML focus workbench");
        work.show(); work.requestActivate(); QTRY_VERIFY(work.isActive());
        auto* input=work.rootObject()->findChild<QQuickItem*>("input"); QVERIFY(input); input->forceActiveFocus();
        QTest::keyClick(&work,Qt::Key_A);
        QTRY_VERIFY_WITH_TIMEOUT(readLog().size()>baseline,6500);
        QVERIFY(work.isActive()); QTest::keyClick(&work,Qt::Key_B);
        QCOMPARE(input->property("text").toString(),QString("ab"));
        QVERIFY(readLog().mid(baseline).contains("before=false after=false appActive=false"));
    }
    void queuedPropAndPassiveEventHaveNoFocusRequest() {
        auto state=connectedSession(); state.peers["b"].busy=true; state.peers["b"].x=1;
        SceneModel model; model.replaceSnapshot(state);
        OverlayWindow window(QGuiApplication::primaryScreen()); load(window,model); window.resize(320,380);
        const SessionEvent e{"e","r","epoch-1",1,Action::Coffee,"a","b","옆에 두었어요","",1,true,"커피",true};
        model.applyEvent(e); QCoreApplication::processEvents();
        auto* target=visualChild(window.rootObject(),"pet_b"); QVERIFY(target);
        auto* pile=visualChild(target,"pileButton");
        QVERIFY(pile); const auto center=pile->mapToScene(QPointF(pile->width()/2,pile->height()/2));
        QVERIFY(window.mask().contains(center.toPoint()));
        QVERIFY(window.flags().testFlag(Qt::WindowDoesNotAcceptFocus));
    }
    void captureApprovedStates() {
        const auto directory=qEnvironmentVariable("PET_CAPTURE_DIR");
        if(directory.isEmpty()) QSKIP("Optional visual capture is run in a bounded inspection pass");
        QVERIFY(QDir().mkpath(directory));
        auto state=connectedSession(); state.peers["a"].wip="알고리즘 과제"; state.peers["b"].wip="OO프로젝트 UX 점검";
        SceneModel model; model.setLocalPeerId("a"); model.replaceSnapshot(state);
        OverlayWindow window(QGuiApplication::primaryScreen()); load(window,model);
        QTest::qWait(120); QVERIFY(window.grabWindow().save(directory+"/idle.png"));
        QVERIFY(QMetaObject::invokeMethod(window.rootObject(),"openMenu",Q_ARG(QVariant,QVariant("b"))));
        QTest::qWait(120); QVERIFY(window.grabWindow().save(directory+"/menu.png"));
        QVERIFY(QMetaObject::invokeMethod(window.rootObject(),"closeMenu"));
        const SessionEvent rope{"e","r","epoch-1",1,Action::Rope,"a","b","조금만 이쪽으로.","어어, 잠깐만",0.37,false,"",false};
        model.applyEvent(rope); QTest::qWait(120); QVERIFY(window.grabWindow().save(directory+"/rope.png"));
        state.revision=2; state.peers["b"].busy=true; state.peers["b"].x=1;
        state.peers["b"].inbox.append({"gift",Action::Coffee,"커피"}); model.replaceSnapshot(state);
        window.resize(320,380); QTest::qWait(120); QVERIFY(window.grabWindow().save(directory+"/busy-small.png"));
    }
};
QTEST_MAIN(PetWindowTest)
#include "tst_petwindow.moc"
