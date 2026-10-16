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
