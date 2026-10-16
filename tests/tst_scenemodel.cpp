#include "SceneModel.h"
#include "SessionFixture.h"
#include <QSignalSpy>
#include <QtTest>

class SceneModelTest : public QObject {
    Q_OBJECT
private slots:
    void snapshotAndIdenticalEvent()
    {
        SceneModel a,b; a.replaceSnapshot(connectedSession()); b.replaceSnapshot(connectedSession());
        QCOMPARE(a.peers().size(),2);
        const SessionEvent e{"e","r","epoch-1",1,Action::Rope,"a","b","가까이","어어",0.37,false,"",false};
        a.applyEvent(e); b.applyEvent(e);
        QCOMPARE(a.peers(),b.peers()); QCOMPARE(a.lastEvent(),b.lastEvent());
        QCOMPARE(a.peers()[1].toMap()["x"].toDouble(),0.37);
        QVERIFY(a.peers()[1].toMap()["pinned"].toBool());
        auto ping=e; ping.eventId="e2"; ping.revision=2; ping.action=Action::Ping;
        a.applyEvent(ping); QCOMPARE(a.peers()[1].toMap()["x"].toDouble(),0.37);
    }
    void queuedItemsWipAndAcknowledgement()
    {
        auto state=connectedSession(); state.peers["b"].busy=true;
        SceneModel scene; scene.replaceSnapshot(state);
        QCOMPARE(scene.peers().size(),2);
        SessionEvent e{"e","r","epoch-1",1,Action::Coffee,"a","b","옆에 두었어요","",0.75,true,"커피",true};
        scene.applyEvent(e); QCOMPARE(scene.peers()[1].toMap()["inbox"].toList().size(),1);
        e.eventId="e2"; e.revision=2; e.action=Action::Note; e.text="<b>안녕</b>\n🙂";
        scene.applyEvent(e); QCOMPARE(scene.peers()[1].toMap()["inbox"].toList()[1].toMap()["text"].toString(),e.text);
        e.eventId="e3"; e.revision=3; e.action=Action::SetBusy; e.actorId="b"; e.busy=false; e.queued=false; e.text.clear();
        scene.applyEvent(e); QCOMPARE(scene.peers()[1].toMap()["inbox"].toList().size(),2);
        e.eventId="e4"; e.revision=4; e.action=Action::AckInbox;
        scene.applyEvent(e); QVERIFY(scene.peers()[1].toMap()["inbox"].toList().isEmpty());
        e.eventId="e5"; e.revision=5; e.action=Action::SetWip; e.text="알고리즘 과제";
        scene.applyEvent(e); QCOMPARE(scene.peers()[1].toMap()["wip"].toString(),e.text);
    }
    void duplicateGapAndSnapshotRecovery()
    {
        SceneModel scene; scene.replaceSnapshot(connectedSession());
        const SessionEvent e{"e","r","epoch-1",1,Action::Ping,"a","b","툭","안녕",0.75,false,"",false};
        scene.applyEvent(e); QSignalSpy changed(&scene,&SceneModel::lastEventChanged);
        scene.applyEvent(e); QCOMPARE(changed.size(),0);
        auto gap=e; gap.revision=3; QSignalSpy recovery(&scene,&SceneModel::snapshotRequired);
        scene.applyEvent(gap); QCOMPARE(recovery.size(),1);
        auto state=connectedSession(); state.revision=3; state.peers["b"].x=0.81;
        QSignalSpy reset(&scene,&SceneModel::sceneReset); scene.replaceSnapshot(state);
        QCOMPARE(reset.size(),1); QVERIFY(scene.lastEvent().isEmpty());
        QCOMPARE(scene.peers()[1].toMap()["x"].toDouble(),0.81);
        scene.replaceSnapshot(connectedSession()); QCOMPARE(scene.peers()[1].toMap()["x"].toDouble(),0.81);
    }
    void invalidEventNeverMutatesScene()
    {
        SceneModel scene; scene.replaceSnapshot(connectedSession()); const auto before=scene.peers();
        QSignalSpy recovery(&scene,&SceneModel::snapshotRequired);
        SessionEvent e{"e","r","epoch-1",1,Action::Ping,"a","b","툭","안녕",0.1,false,"",false};
        scene.applyEvent(e); QCOMPARE(recovery.size(),1); QCOMPARE(scene.peers(),before);
        e.finalX=0.75; e.epoch="other"; scene.applyEvent(e); QCOMPARE(recovery.size(),2);
        e.epoch="epoch-1"; e.targetId="unknown"; scene.applyEvent(e); QCOMPARE(recovery.size(),3);
    }
    void actionsAreIntentOnlyAndRespectIdentity()
    {
        SceneModel scene; scene.setLocalPeerId("a"); scene.replaceSnapshot(connectedSession());
        QSignalSpy requested(&scene,&SceneModel::actionRequested);
        QSignalSpy rejected(&scene,&SceneModel::actionRejected);
        const auto before=scene.peers(); scene.requestAction("rope","b");
        QCOMPARE(requested.size(),1); QCOMPARE(scene.peers(),before);
        QCOMPARE(requested[0][0].value<Action>(),Action::Rope);
        scene.requestAction("setBusy","b",{},true); QCOMPARE(requested.size(),1); QCOMPARE(rejected.size(),1);
        scene.requestAction("setBusy","a",{},true); QCOMPARE(requested.size(),2);
        auto state=connectedSession(); state.peers["b"].reachable=false; scene.replaceSnapshot(state);
        scene.requestAction("coffee","b"); QCOMPARE(requested.size(),2); QCOMPARE(rejected.last()[0].toString(),QString("offline"));
    }
};
QTEST_GUILESS_MAIN(SceneModelTest)
#include "tst_scenemodel.moc"
