#include "SessionClient.h"
#include "SceneModel.h"
#include "MemorySecrets.h"
#include "OverlayWindow.h"
#include "Protocol.h"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QQuickItem>
#include <QQmlComponent>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QFile>
#include <QtTest>
#include <unistd.h>
#include <fcntl.h>

namespace {
QJsonObject object(const QByteArray& bytes) {return QJsonDocument::fromJson(bytes).object();}
QByteArray line(const QJsonObject& value) {return QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n';}
QString uuid() {return QUuid::createUuid().toString(QUuid::WithoutBraces);}
bool until(const std::function<bool()>& predicate,int timeout=6000) {
    QElapsedTimer clock;clock.start();
    while(!predicate() && clock.elapsed()<timeout) QTest::qWait(10);
    return predicate();
}
QQuickItem* item(QQuickItem* root,const QString& name) {
    if(root->objectName()==name) return root;
    for(auto* child:root->childItems()) if(auto* found=item(child,name)) return found;
    return nullptr;
}
int worker(QGuiApplication& app,const QUrl& url) {
    MemorySecrets backend; CredentialStore secrets(&backend,uuid());
    ClientOptions options; options.reconnectDelayMs=[](int){return 80;};
    SessionClient client(&secrets,options); SceneModel model; model.setLocalPeerId("local-device"); model.setTransportConnected(false);
    OverlayWindow window(app.primaryScreen()); window.setContentHeight(380); window.resize(800,380);
    window.setInitialProperties({{"model",QVariant::fromValue(&model)},{"nativeOverlay",QVariant::fromValue(&window)},
        {"sessionClient",QVariant::fromValue(static_cast<QObject*>(&client))},{"serverUrl",url.toString()},{"reducedMotion",!qEnvironmentVariableIsSet("DAEMON_TEST_NORMAL_MOTION")}});
    window.setSource(QUrl::fromLocalFile(PET_MAIN_FILE)); if(window.status()!=QQuickView::Ready) return 2; window.show();
    const auto out=[](const QJsonObject& value){const auto bytes=line(value); ::write(STDOUT_FILENO,bytes.constData(),size_t(bytes.size()));};
    const auto state=[&]{out({{"kind","state"},{"peers",QJsonObject::fromVariantMap(model.peerStates())}});};
    QObject::connect(&client,&SessionClient::snapshotReceived,&model,[&](const SessionState& s){model.setLocalPeerId(client.peerId()); model.setTransportConnected(true);model.replaceSnapshot(s);});
    QObject::connect(&client,&SessionClient::eventReceived,&model,&SceneModel::applyEvent);
    QObject::connect(&model,&SceneModel::snapshotRequired,&client,&SessionClient::requestSnapshot);
    QObject::connect(&model,&SceneModel::peersChanged,&model,state);
    QObject::connect(&model,&SceneModel::actionRequested,&client,[&](Action action,PeerId target,QString text,bool busy,QString id){out({{"kind","intent"},{"id",id}});client.send({id,action,target,text,busy});});
    QObject::connect(&client,&SessionClient::connectionChanged,&model,[&]{
        if(!client.peerId().isEmpty()) model.setLocalPeerId(client.peerId());
        model.setTransportConnected(client.paired());
        out({{"kind","connection"},{"connected",client.connected()},{"paired",client.paired()},{"peerId",client.peerId()},{"join",client.joinRequestId()},{"status",client.status()}});
    });
    QObject::connect(&client,&SessionClient::error,&model,&SceneModel::requestFailed);
    QObject::connect(&model,&SceneModel::requestFailed,&model,[&](QString id,QString code){out({{"kind","error"},{"id",id},{"code",code}});});
    QObject::connect(&client,&SessionClient::controlResult,&model,[&](QString id,QJsonObject body){out({{"kind","result"},{"id",id},{"body",body}});});
    QObject::connect(&client,&SessionClient::eventReceived,&model,[&](const SessionEvent& e){out({{"kind","event"},{"body",object(encodeEvent(e))},{"effect",window.rootObject()->property("effectKind").toString()},{"bubble",window.rootObject()->property("displayedBubble").toString()}});});
    QObject::connect(&client,&SessionClient::noticeReceived,window.rootObject(),[&](QString text){out({{"kind","notice"},{"text",text}});});
    QByteArray input;
    fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK);
    QSocketNotifier reader(STDIN_FILENO,QSocketNotifier::Read);
    QObject::connect(&reader,&QSocketNotifier::activated,&app,[&]{
        char bytes[4096];const auto size=::read(STDIN_FILENO,bytes,sizeof bytes);
        if(size==0) {app.quit();return;} if(size<0) return;
        input.append(bytes,size);if(input.size()>65536) {app.exit(2);return;}
        while(input.contains('\n')) {
            const auto end=input.indexOf('\n');const auto message=object(input.left(end));input.remove(0,end+1);
            const auto kind=message["kind"].toString();const auto tag=message["tag"].toString();
            if(kind=="control") {const auto id=client.requestControl(message["op"].toString(),message["body"].toObject());out({{"kind","sent"},{"tag",tag},{"id",id}});}
            else if(kind=="action") {model.requestAction(message["action"].toString(),message["target"].toString(),message["text"].toString(),message["busy"].toBool(),tag);}
            else if(kind=="ui") {
                const auto panel=message["panel"].toString();
                QMetaObject::invokeMethod(window.rootObject(),"openPanel",Q_ARG(QVariant,QVariant(panel)),Q_ARG(QVariant,message["target"].toVariant()));
                QTimer::singleShot(50,&app,[&,message]{
                    auto* edit=item(window.rootObject(),message["editor"].toString());auto* button=item(window.rootObject(),message["button"].toString());
                    if(edit) edit->setProperty("text",message["text"].toString());
                    if(button && button->isEnabled()) QMetaObject::invokeMethod(button,"clicked");else out({{"kind","harness_error"}});
                });
            } else if(kind=="stop") client.stop();
            else if(kind=="connect") client.connectTo(url);
            else if(kind=="revoke") client.disconnectAndForget();
            else if(kind=="inspect") {state();out({{"kind","inspect"},{"tag",tag},{"panel",window.rootObject()->property("panelKind").toString()},{"active",window.isActive()},{"reducedMotion",window.rootObject()->property("reducedMotion").toBool()}});}
            else if(kind=="close_panel") QMetaObject::invokeMethod(window.rootObject(),"closePanel",Q_ARG(QVariant,QVariant(false)));
            else if(kind=="quit") app.quit();
        }
    });
    client.connectTo(url);return app.exec();
}
struct PeerProcess {
    QProcess process;QByteArray buffer;QVector<QJsonObject> messages;QJsonObject connection,peers;
    ~PeerProcess(){if(process.state()!=QProcess::NotRunning){send({{"kind","quit"}});if(!process.waitForFinished(2000)){process.kill();process.waitForFinished();}}}
    bool start(const QUrl& url) {
        process.start(QCoreApplication::applicationFilePath(),{"--worker",url.toString()});return process.waitForStarted();
    }
    void drain() {
        buffer+=process.readAllStandardOutput();
        while(buffer.contains('\n')) {const auto n=buffer.indexOf('\n');const auto value=object(buffer.left(n));buffer.remove(0,n+1);messages.append(value);
            if(value["kind"]=="connection") connection=value;
            if(value["kind"]=="state") peers=value["peers"].toObject();
        }
    }
    void send(QJsonObject value){if(process.state()!=QProcess::Running)return;process.write(line(value));process.waitForBytesWritten(1000);}
    QString control(const QString& op,const QJsonObject& body={}) {const auto tag=uuid();send({{"kind","control"},{"op",op},{"body",body},{"tag",tag}});return tag;}
    QString action(const QString& action,const QString& target,const QString& text={},bool busy=false) {const auto tag=uuid();send({{"kind","action"},{"action",action},{"target",target},{"text",text},{"busy",busy},{"tag",tag}});return tag;}
    QString id()const{return connection["peerId"].toString();}
    bool connected()const{return connection["connected"].toBool();}
    bool paired()const{return connection["paired"].toBool();}
    QJsonObject peer(const QString& id)const{return peers[id].toObject();}
    QJsonObject find(const QString& kind,const QString& key,const QString& value)const {
        for(auto it=messages.crbegin();it!=messages.crend();++it) if((*it)["kind"]==kind && (*it)[key]==value) return *it;return {};
    }
    QJsonObject receipt(const QString& request)const {for(const auto& m:messages)if(m["kind"]=="event"&&m["body"].toObject()["requestId"]==request)return m;return {};}
    int eventCount(const QString& request)const{int n=0;for(const auto& m:messages)n+=m["kind"]=="event"&&m["body"].toObject()["requestId"]==request;return n;}
};
struct Run {
    QTemporaryDir directory;QProcess server;PeerProcess a,b;QUrl url;
    ~Run(){a.send({{"kind","quit"}});b.send({{"kind","quit"}});if(server.state()!=QProcess::NotRunning){server.terminate();if(!server.waitForFinished(2000)){server.kill();server.waitForFinished();}}}
    bool wait(const std::function<bool()>& predicate,int ms=6000){return until([&]{a.drain();b.drain();return predicate();},ms);}
    bool start() {
        const auto external=qEnvironmentVariable("DAEMON_TEST_SERVER_URL");
        if(!external.isEmpty()) {url=QUrl(external);return a.start(url)&&b.start(url)&&wait([&]{return a.connected()&&b.connected();});}
        server.start(SERVER_PROGRAM,{"--db",directory.filePath("pairing.sqlite"),"--port","0"});if(!server.waitForStarted())return false;
        QByteArray bytes;if(!until([&]{bytes+=server.readAllStandardOutput();return bytes.contains('\n');}))return false;
        url=QUrl(QString::fromUtf8(bytes).trimmed().section(' ',2,2));if(!url.isValid()||url.scheme()!="ws")return false;
        return a.start(url)&&b.start(url)&&wait([&]{return a.connected()&&b.connected();});
    }
    bool pair() {
        if(!start())return false;
        const auto tag=a.control("invite.create");QString request;
        if(!wait([&]{request=a.find("sent","tag",tag)["id"].toString();return !request.isEmpty()&&!a.find("result","id",request).isEmpty();}))return false;
        const auto invite=a.find("result","id",request)["body"].toObject()["invite"];
        b.control("invite.join",{{"invite",invite}});
        if(!wait([&]{return !a.connection["join"].toString().isEmpty();}))return false;
        a.control("invite.approve",{{"joinRequestId",a.connection["join"]}});
        return wait([&]{return a.paired()&&b.paired()&&a.peers.size()==2&&b.peers.size()==2;});
    }
    bool both(const QString& request){return wait([&]{return !a.receipt(request).isEmpty()&&!b.receipt(request).isEmpty();});}
};
}
class TwoPeersTest:public QObject {
    Q_OBJECT
private slots:
    void fullDesktopScenario() {
        Run run;QVERIFY2(run.pair(),"Two actual desktop processes must join the server after approval");const auto a=run.a.id(),b=run.b.id();QVERIFY(a!=b);
        run.a.send({{"kind","ui"},{"panel","wip"},{"target",a},{"editor","wipEditor"},{"button","saveWip"},{"text","알고리즘 과제 😀\n<b>문자</b>"}});
        QVERIFY(run.wait([&]{return run.a.peer(a)["wip"]=="알고리즘 과제 😀\n<b>문자</b>"&&run.b.peer(a)["wip"]=="알고리즘 과제 😀\n<b>문자</b>";}));
        QCOMPARE(run.a.peer(a)["wip"],run.b.peer(a)["wip"]);
        run.a.send({{"kind","close_panel"}});
        auto request=run.b.action("setBusy",b,{},true);QVERIFY(run.both(request));
        request=run.a.action("rope",b);QVERIFY(run.wait([&]{return run.a.find("error","id",request)["code"]=="busy";}));QCOMPARE(run.b.eventCount(request),0);
        for(const auto& action:QStringList{"coffee","energy","note"}) {
            request=run.a.action(action,b,action=="note"?"한 모금 쉬어 ☕\n<b>안녕</b>":QString());QVERIFY(run.both(request));
            QVERIFY(run.b.receipt(request)["body"].toObject()["queued"].toBool());QCOMPARE(run.b.receipt(request)["effect"].toString(),QString());
        }
        QCOMPARE(run.b.peer(b)["inbox"].toArray().size(),3);QCOMPARE(run.a.peer(b)["inbox"],run.b.peer(b)["inbox"]);
        request=run.b.action("setBusy",b,{},false);QVERIFY(run.both(request));QCOMPARE(run.b.peer(b)["inbox"].toArray().size(),3);
        run.b.send({{"kind","ui"},{"panel","inbox"},{"target",b},{"button","ackInbox"}});
        QVERIFY(run.wait([&]{return run.b.peer(b)["inbox"].toArray().isEmpty()&&run.a.peer(b)["inbox"].toArray().isEmpty();}));
        run.b.send({{"kind","close_panel"}});
        request=run.a.action("coffee",b);QVERIFY(run.both(request));
        QCOMPARE(run.a.receipt(request)["body"],run.b.receipt(request)["body"]);
        QCOMPARE(run.a.receipt(request)["bubble"],run.b.receipt(request)["bubble"]);QVERIFY(!run.a.receipt(request)["bubble"].toString().isEmpty());
        request=run.a.action("rope",b);QVERIFY(run.both(request));const auto bx=run.b.peer(b)["x"].toDouble();QVERIFY(bx<0.75);
        request=run.b.action("lure",a);QVERIFY(run.both(request));const auto ax=run.a.peer(a)["x"].toDouble();QVERIFY(ax<0.25);
        request=run.a.action("ping",b);QVERIFY(run.both(request));QCOMPARE(run.a.peer(a)["x"].toDouble(),ax);QCOMPARE(run.b.peer(b)["x"].toDouble(),bx);
        run.b.send({{"kind","stop"}});QVERIFY(run.wait([&]{return !run.b.connected()&&!run.a.peer(b)["reachable"].toBool();}));
        const auto offline=run.a.action("note",b,"offlineには送らない");QVERIFY(run.wait([&]{return !run.a.find("error","id",offline).isEmpty();}));QCOMPARE(run.b.eventCount(offline),0);
        run.b.send({{"kind","connect"}});QVERIFY(run.wait([&]{return run.b.paired()&&run.a.peer(b)["reachable"].toBool();}));QCOMPARE(run.b.id(),b);
        QCOMPARE(run.b.peer(b)["x"].toDouble(),bx);QCOMPARE(run.a.peer(a)["x"].toDouble(),ax);QCOMPARE(run.b.eventCount(offline),0);QCOMPARE(run.b.eventCount(request),1);
        run.a.send({{"kind","revoke"}});QVERIFY(run.wait([&]{return !run.a.connected()&&!run.b.connected()&&run.a.id().isEmpty()&&run.b.id().isEmpty();}));
        QTest::qWait(250);run.a.drain();run.b.drain();QVERIFY(!run.a.connected()&&!run.b.connected());
    }
    void nativeThirtyMinuteSoak() {
        bool ok=false;const auto seconds=qEnvironmentVariableIntValue("DAEMON_SOAK_SECONDS",&ok);if(!ok)QSKIP("Explicit native soak run only");QVERIFY(seconds>=1800);
        const auto path=qEnvironmentVariable("DAEMON_SOAK_OUTPUT");QVERIFY(!path.isEmpty());QFile output(path);QVERIFY(output.open(QIODevice::WriteOnly|QIODevice::Truncate));
        Run run;QVERIFY(run.pair());const auto a=run.a.id(),b=run.b.id();
        const bool normal=qEnvironmentVariableIsSet("DAEMON_TEST_NORMAL_MOTION");
        for(auto* peer:{&run.a,&run.b}){const auto tag=uuid();peer->send({{"kind","inspect"},{"tag",tag}});QVERIFY(run.wait([&]{return !peer->find("inspect","tag",tag).isEmpty();}));QCOMPARE(peer->find("inspect","tag",tag)["reducedMotion"].toBool(),!normal);}
        qInfo("Soak starts: normal motion=%s, seconds=%d",normal?"true":"false",seconds);
        output.write("elapsed_s,phase,pid,cpu_percent,rss_kib\n");QElapsedTimer timer;timer.start();int phase=-1;int lastAction=-30,lastSample=-10;
        while(timer.elapsed()<seconds*1000LL) {
            const int elapsed=int(timer.elapsed()/1000),next=qMin(2,elapsed/(seconds/3));
            if(next!=phase) {phase=next;const auto request=run.b.action("setBusy",b,{},phase==1);QVERIFY(run.both(request));}
            if(phase>0&&elapsed-lastAction>=30) {lastAction=elapsed;const QStringList actions={"ping","coffee","plane","rope","lure"};const auto request=run.a.action(phase==1?"coffee":actions[(elapsed/30)%actions.size()],b);QVERIFY(run.both(request));}
            if(elapsed-lastSample>=10) {
                lastSample=elapsed;QProcess ps;ps.start("/bin/ps",{"-p",QString("%1,%2,%3").arg(run.server.processId()).arg(run.a.process.processId()).arg(run.b.process.processId()),"-o","pid=,%cpu=,rss="});QVERIFY(ps.waitForFinished(2000));
                const auto lines=ps.readAllStandardOutput().trimmed().split('\n');QCOMPARE(lines.size(),3);
                for(const auto& l:lines) {const auto fields=l.simplified().split(' ');QCOMPARE(fields.size(),3);output.write(QByteArray::number(elapsed)+','+QByteArray::number(phase)+','+fields.join(',')+'\n');}output.flush();
            }
            QVERIFY(run.a.process.state()==QProcess::Running&&run.b.process.state()==QProcess::Running&&run.server.state()==QProcess::Running);
            QTest::qWait(100);run.a.drain();run.b.drain();
        }
        run.a.send({{"kind","revoke"}});QVERIFY(run.wait([&]{return !run.a.connected()&&!run.b.connected();}));
        qInfo("Completed native idle/busy/interaction soak; metrics contain no credentials or user content.");
    }
    void remoteHeartbeatKeepsTheSameConnection() {
        if(qEnvironmentVariableIsEmpty("DAEMON_TEST_SERVER_URL"))QSKIP("Explicit Worker/WSS heartbeat check only");
        Run run;QVERIFY(run.pair());const auto a=run.a.id(),b=run.b.id();
        QTest::qWait(35000);run.a.drain();run.b.drain();
        QVERIFY(run.a.paired()&&run.b.paired());QCOMPARE(run.a.id(),a);QCOMPARE(run.b.id(),b);
        for(const auto& peer:{&run.a,&run.b})for(const auto& m:peer->messages)QVERIFY(m["status"]!="reconnecting");
        QVERIFY(run.both(run.a.action("ping",b)));
        run.a.send({{"kind","revoke"}});QVERIFY(run.wait([&]{return !run.a.connected()&&!run.b.connected();}));
    }
    void nativeReceiveKeepsWorkbenchFocus() {
        if(QGuiApplication::platformName()=="offscreen")QSKIP("Actual native focus check only");
        Run run;QVERIFY(run.pair());
        QQuickView workbench;QQmlComponent component(workbench.engine());
        component.setData("import QtQuick\nRectangle {width:240;height:80;color: \"#eeeae1\";TextInput {objectName: \"fixtureInput\";anchors.centerIn:parent;focus:true}}",QUrl());
        auto* content=component.create();QVERIFY(content);workbench.setContent(QUrl(),&component,content);
        workbench.setTitle("Daemon Session focus test");workbench.show();workbench.requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(&workbench,3000));
        QTest::keyClick(&workbench,Qt::Key_A);
        const auto request=run.a.action("coffee",run.b.id());QVERIFY(run.both(request));
        QVERIFY(workbench.isActive());QTest::keyClick(&workbench,Qt::Key_B);
        QCOMPARE(content->findChild<QObject*>("fixtureInput")->property("text").toString(),QString("ab"));
        const auto tag=uuid();run.b.send({{"kind","inspect"},{"tag",tag}});
        QVERIFY(run.wait([&]{return !run.b.find("inspect","tag",tag).isEmpty();}));
        QVERIFY(!run.b.find("inspect","tag",tag)["active"].toBool());
        run.a.send({{"kind","revoke"}});QVERIFY(run.wait([&]{return !run.a.connected()&&!run.b.connected();}));
    }
};
int main(int argc,char** argv) {
    qputenv("QT_QUICK_CONTROLS_STYLE","Basic");QGuiApplication app(argc,argv);
#ifdef Q_OS_MAC
    app.setFont(QFont("Apple SD Gothic Neo"));
#endif
    qmlRegisterType<SceneModel>("Daemon",1,0,"SceneModel");
    if(app.arguments().size()==3&&app.arguments()[1]=="--worker")return worker(app,QUrl(app.arguments()[2]));
    TwoPeersTest test;return QTest::qExec(&test,argc,argv);
}
#include "tst_twopeers.moc"
