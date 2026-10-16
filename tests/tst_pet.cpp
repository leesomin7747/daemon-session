#include "SceneModel.h"
#include "SessionFixture.h"
#include <QQmlContext>
#include <QQmlEngine>
#include <QJsonObject>
#include <QJsonDocument>
#include <QtQuickTest>

class Fixtures : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE void panelInbox(SceneModel* model) {
        auto s=connectedSession();
        for(int i=0;i<12;++i) s.peers["a"].inbox.append({QString("note-%1").arg(i),Action::Note,"조금만 더 하면 끝나요.\n끝나면 커피 마시자."});
        model->replaceSnapshot(s);
    }
    Q_INVOKABLE void panelState(SceneModel* model, bool localOnline=true, bool peerOnline=true,
                               bool busy=false, const QString& wip="알고리즘 과제", bool inbox=false) {
        auto s=connectedSession();
        s.peers["a"].reachable=localOnline; s.peers["b"].reachable=peerOnline;
        s.peers["a"].busy=busy; s.peers["a"].wip=wip;
        s.peers["b"].wip="<b>UX 점검</b>";
        if(inbox) {
            s.peers["a"].inbox.append({"gift",Action::Coffee,"커피를 두고 갔어요."});
            s.peers["a"].inbox.append({"note",Action::Note,"<b>과제</b>\n끝나면 쉬자 ☕"});
        }
        model->replaceSnapshot(s);
    }
    Q_INVOKABLE void reset(SceneModel* model, double x=0.75, bool busy=false, int revision=0) {
        auto s=connectedSession(); s.peers["b"].x=x; s.peers["b"].busy=busy;
        s.revision=quint64(revision);
        model->replaceSnapshot(s);
    }
    Q_INVOKABLE void deliver(SceneModel* model,const QVariantMap& map) {
        const auto event=decodeEvent(QJsonDocument(QJsonObject::fromVariantMap(map)).toJson());
        if (event) model->applyEvent(*event);
    }
};
class Setup : public QObject {
    Q_OBJECT
    Fixtures fixtures;
public slots:
    void applicationAvailable() {
        qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
#ifdef Q_OS_MAC
        QGuiApplication::setFont(QFont("Apple SD Gothic Neo"));
#endif
        qmlRegisterType<SceneModel>("Daemon",1,0,"SceneModel");
    }
    void qmlEngineAvailable(QQmlEngine* engine) { engine->rootContext()->setContextProperty("fixtures",&fixtures); }
};
QUICK_TEST_MAIN_WITH_SETUP(pet,Setup)
#include "tst_pet.moc"
