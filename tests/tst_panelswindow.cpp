#include "DesktopTray.h"
#include "OverlayWindow.h"
#include "SceneModel.h"
#include "SessionFixture.h"
#include <QApplication>
#include <QInputMethodEvent>
#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QtTest>

class PanelsWindowTest : public QObject {
    Q_OBJECT
    QQuickItem* item(QQuickItem* root,const QString& name) {
        if(root->objectName()==name) return root;
        for(auto* child:root->childItems()) if(auto* found=item(child,name)) return found;
        return nullptr;
    }
    void load(OverlayWindow& window,SceneModel& model) {
        auto state=connectedSession(); state.peers["a"].wip="";
        model.setLocalPeerId("a"); model.replaceSnapshot(state);
        window.setContentHeight(380); window.resize(800,380);
        window.setInitialProperties({{"model",QVariant::fromValue(&model)},{"nativeOverlay",QVariant::fromValue(&window)},{"reducedMotion",true}});
        window.setSource(QUrl::fromLocalFile(PET_MAIN_FILE)); QCOMPARE(window.status(),QQuickView::Ready);
        window.show(); QCoreApplication::processEvents();
    }
    void open(OverlayWindow& window,const QString& kind,const QString& peer="a") {
        QVERIFY(QMetaObject::invokeMethod(window.rootObject(),"openPanel",Q_ARG(QVariant,QVariant(kind)),Q_ARG(QVariant,QVariant(peer))));
        QCoreApplication::processEvents();
    }
private slots:
    void initTestCase() {
        qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
#ifdef Q_OS_MAC
        QApplication::setFont(QFont("Apple SD Gothic Neo"));
#endif
        qmlRegisterType<SceneModel>("Daemon",1,0,"SceneModel");
    }
    void trayProvidesExplicitEntryAndQuit() {
        DesktopTray tray;
        const auto actions=tray.menu()->actions();
        QCOMPARE(actions.size(),6);
        QCOMPARE(actions[0]->text(),QString("내 펫 메뉴 열기"));
        QCOMPARE(actions[1]->text(),QString("친구 펫 메뉴 열기"));
        QCOMPARE(actions[2]->text(),QString("받은 것"));
        QCOMPARE(actions[3]->text(),QString("세션 연결"));
        QSignalSpy own(&tray,&DesktopTray::ownPetRequested), peer(&tray,&DesktopTray::friendPetRequested),
            inbox(&tray,&DesktopTray::inboxRequested), quit(&tray,&DesktopTray::quitRequested),connection(&tray,&DesktopTray::connectionRequested);
        actions[0]->trigger(); actions[1]->trigger(); actions[2]->trigger(); actions[3]->trigger(); actions[5]->trigger();
        QCOMPARE(own.size(),1); QCOMPARE(peer.size(),1); QCOMPARE(inbox.size(),1); QCOMPARE(quit.size(),1);
        QCOMPARE(connection.size(),1);
    }
    void composingTextCanBeSavedWithOneClick_data() {
        QTest::addColumn<QString>("kind"); QTest::addColumn<QString>("peer");
        QTest::addColumn<QString>("editorName"); QTest::addColumn<QString>("buttonName");
        QTest::newRow("WIP")<<"wip"<<"a"<<"wipEditor"<<"saveWip";
        QTest::newRow("note")<<"note"<<"b"<<"noteEditor"<<"sendNote";
    }
    void composingTextCanBeSavedWithOneClick() {
        QFETCH(QString,kind); QFETCH(QString,peer); QFETCH(QString,editorName); QFETCH(QString,buttonName);
        SceneModel model; OverlayWindow window(QApplication::primaryScreen()); load(window,model);
        open(window,kind,peer);
        auto* editor=item(window.rootObject(),editorName); auto* save=item(window.rootObject(),buttonName);
        QVERIFY(editor); QVERIFY(save); editor->forceActiveFocus();
        QSignalSpy requests(&model,&SceneModel::actionRequested);
        QInputMethodEvent preedit(QString("한"),{}); QCoreApplication::sendEvent(editor,&preedit);
        QVERIFY(editor->property("inputMethodComposing").toBool());
        QVERIFY2(save->isEnabled(),"A valid composing draft must not require an extra outside click");
        QVERIFY(QMetaObject::invokeMethod(save,"clicked")); QCOMPARE(requests.size(),0);
        QCOMPARE(editor->property("text").toString(),QString());
        QInputMethodEvent commit; const QString text=QString::fromUtf8("한글 😀\n<b>과제</b>");
        commit.setCommitString(text); QCoreApplication::sendEvent(editor,&commit);
        QVERIFY(!editor->property("inputMethodComposing").toBool());
        QCOMPARE(editor->property("text").toString(),text);
        QTRY_COMPARE(requests.size(),1);
        QCOMPARE(requests.at(0).at(2).toString(),text);
    }
    void explicitTrayEntryAndEscapeRestoreWorkbenchFocus() {
        SceneModel model; OverlayWindow window(QApplication::primaryScreen()); load(window,model);
        QQuickView work; QQmlComponent component(work.engine());
        component.setData("import QtQuick\nRectangle { width: 400; height: 180; color: '#203028'; TextInput { anchors.fill: parent; focus: true; color: 'white' } }",QUrl());
        auto* root=qobject_cast<QQuickItem*>(component.create()); QVERIFY(root);
        work.setContent(QUrl(),&component,root); work.setTitle("Daemon panel focus workbench");
        work.show(); work.requestActivate(); QTRY_VERIFY(work.isActive());
        DesktopTray tray; connect(&tray,&DesktopTray::ownPetRequested,&window,[&]{open(window,"wip");});
        const auto actions=tray.menu()->actions(); QVERIFY(!actions.isEmpty()); actions[0]->trigger();
        QTRY_VERIFY(window.isActive());
        auto* editor=item(window.rootObject(),"wipEditor"); QVERIFY(editor); QTRY_VERIFY(editor->hasActiveFocus());
        QTest::keyClick(&window,Qt::Key_A); QCOMPARE(editor->property("text").toString(),QString("a"));
        QTest::keyClick(&window,Qt::Key_Escape);
        QTRY_COMPARE(window.rootObject()->property("panelKind").toString(),QString());
        QVERIFY(window.flags().testFlag(Qt::WindowDoesNotAcceptFocus)); QTRY_VERIFY(work.isActive());
    }
    void panelMaskIsRemovedWhenClosed() {
        SceneModel model; OverlayWindow window(QApplication::primaryScreen()); load(window,model);
        QVERIFY(!window.mask().contains(QPoint(200,50))); open(window,"wip");
        QVERIFY(window.mask().contains(QPoint(200,50)));
        QVERIFY(QMetaObject::invokeMethod(window.rootObject(),"closePanel",Q_ARG(QVariant,QVariant(false))));
        QCoreApplication::processEvents(); QVERIFY(!window.mask().contains(QPoint(200,50)));
    }
    void capturePanelStates() {
        const auto path=qEnvironmentVariable("PANEL_CAPTURE_DIR");
        if(path.isEmpty()) QSKIP("Optional bounded visual inspection");
        QVERIFY(QDir().mkpath(path));
        SceneModel model; OverlayWindow window(QApplication::primaryScreen()); load(window,model);
        auto state=connectedSession(); state.peers["a"].wip="알고리즘 과제\n그래프 탐색 정리";
        state.peers["b"].wip="OO프로젝트 UX 점검"; state.peers["b"].busy=true;
        state.peers["a"].inbox={{"gift",Action::Coffee,"친구가 커피 한 잔을 두고 갔어요."},{"note",Action::Note,"<b>과제</b> 끝나면 쉬자 😀"}};
        model.replaceSnapshot(state);
        open(window,"wip"); QTest::qWait(100); QVERIFY(window.grabWindow().save(path+"/wip.png"));
        open(window,"note","b"); auto* editor=item(window.rootObject(),"noteEditor"); QVERIFY(editor);
        editor->setProperty("text","커피 마시고 조금만 더 하자 ☕");
        QTest::qWait(100); QVERIFY(window.grabWindow().save(path+"/note.png"));
        open(window,"inbox"); QTest::qWait(100); QVERIFY(window.grabWindow().save(path+"/inbox.png"));
        window.resize(320,250); open(window,"wip"); QTest::qWait(100); QVERIFY(window.grabWindow().save(path+"/short.png"));
    }
};
QTEST_MAIN(PanelsWindowTest)
#include "tst_panelswindow.moc"
