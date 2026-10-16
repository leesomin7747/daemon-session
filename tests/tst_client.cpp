#include "SessionClient.h"
#include "SessionBroker.h"
#include "SceneModel.h"
#include "SessionFixture.h"
#include "MemorySecrets.h"
#include "WireProtocol.h"
#include <QJsonDocument>
#include <QSignalSpy>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QtTest>
#include "OverlayWindow.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QScopeGuard>
#include <QDir>
#include <QUuid>

namespace {
bool until(const std::function<bool()>& check,int timeout=900) {
    QElapsedTimer elapsed; elapsed.start();
    while(!check() && elapsed.elapsed()<timeout) QTest::qWait(5);
    return check();
}
ClientOptions fastOptions() { ClientOptions o; o.reconnectDelayMs=[](int){return 20;}; o.requestTimeoutMs=300; return o; }
QString token(const QByteArray& b) {return QString::fromLatin1(b.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));}
QJsonObject object(const QByteArray& bytes) {return QJsonDocument::fromJson(bytes).object();}
struct Fixture {
    PairStore db{":memory:"}; SessionBroker broker{db};
    MemorySecrets secretsA,secretsB;
    CredentialStore storeA{&secretsA,"a"},storeB{&secretsB,"b"};
    SessionClient a{&storeA,fastOptions()},b{&storeB,fastOptions()};
    Fixture() {broker.listen(QHostAddress::LocalHost,0);}
    QUrl url() {return QUrl(QString("ws://127.0.0.1:%1/").arg(broker.serverPort()));}
    bool connect() {a.connectTo(url()); b.connectTo(url()); return until([&]{return a.connected() && b.connected();});}
    bool pair() {
        if(!connect()) return false;
        if(a.requestControl("invite.create").isEmpty() || !until([&]{return !a.inviteCode().isEmpty();})) return false;
        if(b.requestControl("invite.join",{{"invite",a.inviteCode()}}).isEmpty() || !until([&]{return !a.joinRequestId().isEmpty();})) return false;
        a.requestControl("invite.approve",{{"joinRequestId",a.joinRequestId()}});
        return until([&]{return a.paired() && b.paired();});
    }
};
struct ScriptServer {
    QWebSocketServer server{"client-boundary",QWebSocketServer::NonSecureMode};
    QPointer<QWebSocket> socket;
    QVector<WireFrame> received;
    SessionState state=connectedSession();
    bool replySnapshots=true;
    bool replyHeartbeats=true;
    ScriptServer() {
        state.revision=1;
        server.listen(QHostAddress::LocalHost,0);
        QObject::connect(&server,&QWebSocketServer::newConnection,&server,[this]{
            socket=server.nextPendingConnection();
            auto* current=socket.data();
            QObject::connect(current,&QWebSocket::disconnected,current,&QObject::deleteLater);
            QObject::connect(current,&QWebSocket::textMessageReceived,&server,[this,current](const QString& message){
                const auto frame=decodeFrame(message.toUtf8()); if(!frame) return;
                received.append(*frame);
                if(frame->op=="device.register") respond(current,*frame,{{"peerId","a"},{"token",token(QByteArray(32,'t'))}});
                if(frame->op=="auth") {respond(current,*frame,{{"peerId","a"}}); pushSnapshot();}
                if(frame->op=="session.heartbeat" && replyHeartbeats) respond(current,*frame,{});
                if(frame->op=="session.snapshot" && replySnapshots) respond(current,*frame,object(encodeSnapshot(state)));
            });
        });
    }
    ~ScriptServer() {if(socket) socket->abort(); server.close();}
    QUrl url() {return QUrl(QString("ws://127.0.0.1:%1/").arg(server.serverPort()));}
    void respond(QWebSocket* s,const WireFrame& request,const QJsonObject& body) {s->sendTextMessage(QString::fromUtf8(encodeFrame({"result",request.requestId,body})));}
    void pushSnapshot() {if(socket) socket->sendTextMessage(QString::fromUtf8(encodeFrame({"session.snapshot",{},object(encodeSnapshot(state))})));}
    void push(const SessionEvent& event) {if(socket) socket->sendTextMessage(QString::fromUtf8(encodeFrame({"session.event",{},object(encodeEvent(event))})));}
    int count(const QString& op) const {int n=0; for(const auto& f:received) n+=f.op==op; return n;}
};
void connectScene(SessionClient& client,SceneModel& model) {
    QObject::connect(&client,&SessionClient::snapshotReceived,&model,&SceneModel::replaceSnapshot);
    QObject::connect(&client,&SessionClient::eventReceived,&model,&SceneModel::applyEvent);
    QObject::connect(&model,&SceneModel::snapshotRequired,&client,&SessionClient::requestSnapshot);
    QObject::connect(&client,&SessionClient::connectionChanged,&model,[&client,&model]{model.setLocalPeerId(client.peerId()); model.setTransportConnected(client.paired());});
    QObject::connect(&client,&SessionClient::error,&model,&SceneModel::requestFailed);
    QObject::connect(&model,&SceneModel::actionRequested,&client,[&client](Action action,PeerId target,QString text,bool busy,QString request){client.send({request,action,target,text,busy});});
}
}
class ClientTest : public QObject {
    Q_OBJECT
private slots:
    void heartbeatIsVisibleToTheApplicationServer() {
        ScriptServer server; MemorySecrets secrets; CredentialStore store(&secrets);
        auto options=fastOptions(); options.heartbeatMs=40; options.heartbeatTimeoutMs=500;
        SessionClient client(&store,options); client.connectTo(server.url());
        QVERIFY(until([&]{return client.paired();}));
        QVERIFY(until([&]{return server.count("session.heartbeat")>0;}));
    }
    void missingApplicationHeartbeatReconnectsEvenWhenTransportPongs() {
        ScriptServer server; server.replyHeartbeats=false;
        MemorySecrets secrets; CredentialStore store(&secrets);
        auto options=fastOptions(); options.heartbeatMs=40; options.heartbeatTimeoutMs=500; options.requestTimeoutMs=60;
        SessionClient client(&store,options); QSignalSpy retries(&client,&SessionClient::reconnectScheduled);
        client.connectTo(server.url()); QVERIFY(until([&]{return client.paired();}));
        QVERIFY(until([&]{return !retries.isEmpty();}));
    }
    void initTestCase() {
        qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
#ifdef Q_OS_MAC
        QGuiApplication::setFont(QFont("Apple SD Gothic Neo"));
#endif
        qmlRegisterType<SceneModel>("Daemon",1,0,"SceneModel");
    }
    void localPetRemainsOptionalOffline() {
        SceneModel model; model.setLocalPeerId("a"); auto state=connectedSession();
        state.peers["a"].inbox.append({"note",Action::Note,"받은 것은 남겨요"}); model.replaceSnapshot(state);
        model.setTransportConnected(false);
        QCOMPARE(model.peers().size(),1); QCOMPARE(model.peers()[0].toMap()["id"].toString(),QString("a"));
        QVERIFY(!model.peers()[0].toMap()["reachable"].toBool());
        QCOMPARE(model.peer("a")["inbox"].toList().size(),1);
        model.setShowLocalOffline(false); QCOMPARE(model.peers().size(),0);
        QCOMPARE(model.peer("a")["inbox"].toList().size(),1);
    }
    void firstConnectionRequiresApprovalAndStoresBeforeAuth() {
        Fixture f; QVERIFY(f.connect());
        QVERIFY(!f.a.paired()); QVERIFY(!f.b.paired());
        QCOMPARE(f.secretsA.writes,1); QCOMPARE(f.secretsB.writes,1);
        f.a.requestControl("invite.create"); QVERIFY(until([&]{return !f.a.inviteCode().isEmpty();}));
        f.b.requestControl("invite.join",{{"invite",f.a.inviteCode()}});
        QVERIFY(until([&]{return !f.a.joinRequestId().isEmpty();}));
        QVERIFY(!f.a.paired()); QCOMPARE(f.a.pendingPeerId(),f.b.peerId());
        f.a.requestControl("invite.approve",{{"joinRequestId",f.a.joinRequestId()}});
        QVERIFY(until([&]{return f.a.paired() && f.b.paired();}));
    }
    void relaunchReusesPairedIdentity() {
        Fixture f; QVERIFY(f.pair()); const auto id=f.a.peerId(); f.a.stop();
        SessionClient relaunched(&f.storeA,fastOptions()); QSignalSpy snapshots(&relaunched,&SessionClient::snapshotReceived);
        relaunched.connectTo(f.url()); QVERIFY(until([&]{return relaunched.paired();}));
        QCOMPARE(relaunched.peerId(),id); QCOMPARE(f.secretsA.writes,1); QVERIFY(!snapshots.isEmpty());
    }
    void secretSaveFailureNeverAuthenticates() {
        ScriptServer server; MemorySecrets secrets; secrets.writeError="denied";
        CredentialStore store(&secrets); SessionClient client(&store,fastOptions()); QSignalSpy errors(&client,&SessionClient::error);
        client.connectTo(server.url()); QVERIFY(until([&]{return !errors.isEmpty();}));
        QVERIFY(!client.connected()); QCOMPARE(server.count("auth"),0); QVERIFY(secrets.values.isEmpty());
        QTest::qWait(60); QCOMPARE(server.count("device.register"),1);
    }
    void staleSocketCannotSend() {
        ScriptServer first,second; MemorySecrets secrets; CredentialStore store(&secrets); SessionClient client(&store,fastOptions());
        client.connectTo(first.url()); QVERIFY(until([&]{return client.paired();}));
        client.connectTo(second.url());
        QSignalSpy errors(&client,&SessionClient::error);
        client.send({"stale",Action::Note,"b","must not send",false});
        QVERIFY(until([&]{return !errors.isEmpty();}));
        QVERIFY(until([&]{return client.paired();}));
        QCOMPARE(first.count("session.command"),0); QCOMPARE(second.count("session.command"),0);
    }
    void duplicateEventAppliedOnce() {
        ScriptServer server; MemorySecrets secrets; CredentialStore store(&secrets); SessionClient client(&store,fastOptions());
        SceneModel model; connectScene(client,model); QSignalSpy events(&client,&SessionClient::eventReceived);
        client.connectTo(server.url()); QVERIFY(until([&]{return client.paired();}));
        SessionEvent e{"event","command","epoch-1",2,Action::Note,"b","a","届いた","",0.25,false,"한글 😀",false};
        server.push(e); server.push(e); QVERIFY(until([&]{return events.size()==1;})); QTest::qWait(20);
        QCOMPARE(events.size(),1); QCOMPARE(model.peers()[0].toMap()["inbox"].toList().size(),1);
    }
    void revisionGapRequestsSnapshot() {
        ScriptServer server; MemorySecrets secrets; CredentialStore store(&secrets); SessionClient client(&store,fastOptions());
        SceneModel model; connectScene(client,model); QSignalSpy snapshots(&client,&SessionClient::snapshotReceived),events(&client,&SessionClient::eventReceived);
        client.connectTo(server.url()); QVERIFY(until([&]{return client.paired();}));
        server.state.revision=4; server.state.peers["b"].wip="복구됨";
        server.push({"gap","r","epoch-1",4,Action::SetWip,"b","b","갱신","",0.75,false,"복구됨",false});
        QVERIFY(until([&]{return snapshots.size()==2;})); QCOMPARE(server.count("session.snapshot"),1);
        QCOMPARE(events.size(),0); QCOMPARE(model.peers()[1].toMap()["wip"].toString(),QString("복구됨"));
    }
    void disconnectDoesNotReplayPendingNote() {
        ScriptServer server; MemorySecrets secrets; CredentialStore store(&secrets); SessionClient client(&store,fastOptions());
        QSignalSpy errors(&client,&SessionClient::error),snapshots(&client,&SessionClient::snapshotReceived);
        client.connectTo(server.url()); QVERIFY(until([&]{return client.paired();}));
        client.send({"pending-note",Action::Note,"b","only once",false});
        QVERIFY(until([&]{return server.count("session.command")==1;})); server.socket->close();
        QVERIFY(until([&]{return snapshots.size()>=2 && client.paired();}));
        QCOMPARE(server.count("session.command"),1);
        bool matched=false; for(const auto& e:errors) matched|=e[0].toString()=="pending-note";
        QVERIFY(matched); QCOMPARE(secrets.writes,1);
    }
    void badTlsCertificateIsNotIgnored() {
        QWebSocketServer server("untrusted",QWebSocketServer::SecureMode);
        QFile cert(QString(TEST_FIXTURES)+"/localhost-cert.pem"),key(QString(TEST_FIXTURES)+"/localhost-key.pem");
        QVERIFY(cert.open(QIODevice::ReadOnly)); QVERIFY(key.open(QIODevice::ReadOnly));
        auto config=QSslConfiguration::defaultConfiguration();
        config.setLocalCertificate(QSslCertificate(cert.readAll())); config.setPrivateKey(QSslKey(key.readAll(),QSsl::Rsa));
        server.setSslConfiguration(config); QVERIFY(server.listen(QHostAddress::LocalHost,0));
        MemorySecrets secrets; CredentialStore store(&secrets); SessionClient client(&store,fastOptions());
        QSignalSpy errors(&client,&SessionClient::error),accepted(&server,&QWebSocketServer::newConnection);
        client.connectTo(QUrl(QString("wss://127.0.0.1:%1/").arg(server.serverPort())));
        QVERIFY(until([&]{for(const auto& e:errors) if(e[1].toString()=="tls") return true; return false;},2000));
        QVERIFY(!client.connected()); QCOMPARE(accepted.size(),0); QCOMPARE(secrets.writes,0);
    }
    void unpairStopsReconnect() {
        Fixture f; QVERIFY(f.pair()); const auto old=f.a.peerId();
        f.a.disconnectAndForget(); QVERIFY(until([&]{return !f.a.connected() && f.secretsA.values.isEmpty();}));
        QTest::qWait(80); QVERIFY(!f.a.connected()); QVERIFY(!f.db.pairForPeer(old));
        QVERIFY(until([&]{return !f.b.connected();}));
    }
    void commandsUpdateBothSceneModels() {
        Fixture f; SceneModel ma,mb; connectScene(f.a,ma); connectScene(f.b,mb); QVERIFY(f.pair());
        QSignalSpy ea(&f.a,&SessionClient::eventReceived),eb(&f.b,&SessionClient::eventReceived);
        f.a.send({"wip",Action::SetWip,f.a.peerId(),"<b>과제</b> 😀",false});
        QVERIFY(until([&]{return ea.size()==1 && eb.size()==1;}));
        QCOMPARE(ma.peers(),mb.peers());
        f.b.send({"busy",Action::SetBusy,f.b.peerId(),{},true}); QVERIFY(until([&]{return ea.size()==2;}));
        f.a.send({"coffee",Action::Coffee,f.b.peerId(),{},false}); QVERIFY(until([&]{return ea.size()==3 && eb.size()==3;}));
        QCOMPARE(ma.peers(),mb.peers());
        const auto received=qvariant_cast<SessionEvent>(ea.last()[0]); QVERIFY(received.queued); QVERIFY(received.bubble.isEmpty());
    }
    void rejectsUnsafeUrlsBeforeSecretRead() {
        for(const auto& url:{"ws://example.com/","wss://user:pass@example.com/","wss://example.com/?token=secret","https://example.com/","wss://example.com/#x"}) {
            MemorySecrets secrets; CredentialStore store(&secrets); SessionClient client(&store,fastOptions()); QSignalSpy errors(&client,&SessionClient::error);
            client.connectTo(QUrl(url)); QVERIFY(until([&]{return !errors.isEmpty();})); QCOMPARE(secrets.reads,0);
        }
    }
    void pendingCommandTimesOutWithoutResending() {
        ScriptServer server; MemorySecrets secrets; CredentialStore store(&secrets); auto options=fastOptions(); options.requestTimeoutMs=60;
        SessionClient client(&store,options); QSignalSpy errors(&client,&SessionClient::error);
        client.connectTo(server.url()); QVERIFY(until([&]{return client.paired();}));
        client.send({"timeout",Action::Note,"b","once",false});
        QVERIFY(until([&]{for(const auto& e:errors) if(e[0].toString()=="timeout" && e[1].toString()=="timeout") return true; return false;}));
        QCOMPARE(server.count("session.command"),1); QVERIFY(client.connected());
    }
    void controlResultsNeverExposeRegistrationToken() {
        Fixture f; QSignalSpy controls(&f.a,&SessionClient::controlResult); QVERIFY(f.connect());
        for(const auto& row:controls) QVERIFY(!qvariant_cast<QJsonObject>(row[1]).contains("token"));
        QCOMPARE(controls.size(),0);
    }
    void resultWithoutUsableEventReportsUncertainDelivery() {
        ScriptServer server; MemorySecrets secrets; CredentialStore store(&secrets); SessionClient client(&store,fastOptions());
        QSignalSpy errors(&client,&SessionClient::error);
        client.connectTo(server.url()); QVERIFY(until([&]{return client.paired();}));
        client.send({"uncertain",Action::Note,"b","one note",false});
        QVERIFY(until([&]{return server.count("session.command")==1;}));
        server.state.revision=4;
        server.push({"event","uncertain","epoch-1",4,Action::Note,"a","b","받았어요","",0.75,false,"one note",false});
        server.socket->sendTextMessage(QString::fromUtf8(encodeFrame({"result","uncertain",{{"eventId","event"},{"epoch","epoch-1"},{"revision","4"}}})));
        QVERIFY(until([&]{for(const auto& e:errors) if(e[0].toString()=="uncertain" && e[1].toString()=="delivery_unknown") return true; return false;}));
        QCOMPARE(server.count("session.command"),1);
    }
    void matchingRequestCannotChangeSubmittedText() {
        ScriptServer server; MemorySecrets secrets; CredentialStore store(&secrets); SessionClient client(&store,fastOptions());
        QSignalSpy errors(&client,&SessionClient::error);
        client.connectTo(server.url()); QVERIFY(until([&]{return client.paired();}));
        client.send({"exact",Action::Note,"b","original",false}); QVERIFY(until([&]{return server.count("session.command")==1;}));
        server.push({"event","exact","epoch-1",2,Action::Note,"a","b","도착","",0.75,false,"different",false});
        QVERIFY(until([&]{return client.status()=="protocol_error";}));
        bool failed=false; for(const auto& e:errors) failed|=e[0].toString()=="exact";
        QVERIFY(failed);
    }
    void validEpochRecoveryHasNoLifetimeCountLimit() {
        ScriptServer server; MemorySecrets secrets; CredentialStore store(&secrets); SessionClient client(&store,fastOptions());
        QSignalSpy snapshots(&client,&SessionClient::snapshotReceived);
        client.connectTo(server.url()); QVERIFY(until([&]{return client.paired();}));
        for(int i=0;i<35;++i) {server.state.epoch=QString("fresh-%1").arg(i); server.pushSnapshot();}
        QVERIFY(until([&]{return snapshots.size()==36;})); QVERIFY(client.connected());
        server.state.epoch="fresh-34"; server.state.revision=2; server.pushSnapshot();
        QVERIFY(until([&]{return snapshots.size()==37;}));
        server.state.epoch="fresh-33"; server.pushSnapshot(); QTest::qWait(20); QCOMPARE(snapshots.size(),37);
    }
    void nativeKeychainRelaunch() {
        if(!qEnvironmentVariableIsSet("DAEMON_KEYCHAIN_NATIVE")) QSKIP("Explicit Cocoa and OS keychain verification");
        PairStore db(":memory:"); SessionBroker broker(db); QVERIFY(broker.listen(QHostAddress::LocalHost,0));
        const QUrl url(QString("ws://127.0.0.1:%1/").arg(broker.serverPort()));
        const auto suffix=QUuid::createUuid().toString(QUuid::WithoutBraces);
        CredentialStore sa("native-a-"+suffix),sb("native-b-"+suffix);
        sa.setEndpoint(url); sb.setEndpoint(url);
        auto options=fastOptions(); options.requestTimeoutMs=2000;
        SessionClient a(&sa,options),b(&sb,options);
        auto cleanup=qScopeGuard([&]{
            a.stop(); b.stop(); until([&]{return !sa.busy() && !sb.busy();},3000);
            bool ca=false,cb=false; QObject::connect(&sa,&CredentialStore::cleared,&sa,[&]{ca=true;}); QObject::connect(&sb,&CredentialStore::cleared,&sb,[&]{cb=true;});
            sa.clear(); sb.clear(); QVERIFY(until([&]{return ca && cb;},3000));
        });
        a.connectTo(url); b.connectTo(url); QVERIFY(until([&]{return a.connected() && b.connected();},4000));
        a.requestControl("invite.create"); QVERIFY(until([&]{return !a.inviteCode().isEmpty();}));
        b.requestControl("invite.join",{{"invite",a.inviteCode()}}); QVERIFY(until([&]{return !a.joinRequestId().isEmpty();}));
        a.requestControl("invite.approve",{{"joinRequestId",a.joinRequestId()}}); QVERIFY(until([&]{return a.paired() && b.paired();}));
        const auto identity=a.peerId(); a.stop();
        SessionClient relaunched(&sa,options); relaunched.connectTo(url);
        QVERIFY(until([&]{return relaunched.paired();},4000)); QCOMPARE(relaunched.peerId(),identity);
        QSignalSpy received(&b,&SessionClient::eventReceived);
        relaunched.send({"native-note",Action::Note,b.peerId(),"한글 😀 실제 저장소 연결",false});
        QVERIFY(until([&]{return received.size()==1;}));
        QCOMPARE(qvariant_cast<SessionEvent>(received[0][0]).text,QString("한글 😀 실제 저장소 연결"));
        relaunched.disconnectAndForget(); QVERIFY(until([&]{return !relaunched.connected() && !b.connected() && !sa.busy() && !sb.busy();},4000));
    }
    void nativeQmlSaveReceivesItsAuthoritativeEvent() {
        if(QGuiApplication::platformName()=="offscreen") QSKIP("Requires actual Cocoa/native renderer");
        Fixture f; SceneModel model; connectScene(f.a,model); QVERIFY(f.pair());
        OverlayWindow window(QGuiApplication::primaryScreen()); window.setContentHeight(380); window.resize(800,380);
        window.setInitialProperties({{"model",QVariant::fromValue(&model)},{"nativeOverlay",QVariant::fromValue(&window)},
            {"sessionClient",QVariant::fromValue(static_cast<QObject*>(&f.a))},{"serverUrl",f.url().toString()},{"reducedMotion",true}});
        window.setSource(QUrl::fromLocalFile(PET_MAIN_FILE)); QCOMPARE(window.status(),QQuickView::Ready); window.show();
        QVERIFY(QMetaObject::invokeMethod(window.rootObject(),"openPanel",Q_ARG(QVariant,QVariant("wip")),Q_ARG(QVariant,QVariant(f.a.peerId()))));
        std::function<QQuickItem*(QQuickItem*,const QString&)> find=[&](QQuickItem* root,const QString& name)->QQuickItem* {
            if(root->objectName()==name) return root; for(auto* child:root->childItems()) if(auto* result=find(child,name)) return result; return nullptr;
        };
        QVERIFY(until([&]{return find(window.rootObject(),"wipEditor")!=nullptr;}));
        auto* editor=find(window.rootObject(),"wipEditor"); auto* save=find(window.rootObject(),"saveWip"); QVERIFY(save);
        editor->setProperty("text","한글 과제 😀"); QSignalSpy requests(&model,&SceneModel::actionRequested);
        QVERIFY(QMetaObject::invokeMethod(save,"clicked"));
        QVERIFY(until([&]{return requests.size()==1 && model.peer(f.a.peerId())["wip"].toString()=="한글 과제 😀";}));
        QVERIFY(save->isEnabled());
        QCOMPARE(model.lastEvent()["requestId"].toString(),requests[0][4].toString());
    }
    void captureConnectionStates() {
        const auto path=qEnvironmentVariable("CONNECTION_CAPTURE_DIR");
        if(path.isEmpty()) QSKIP("Optional bounded native UI capture");
        QVERIFY(QDir().mkpath(path)); Fixture f; SceneModel model; connectScene(f.a,model); model.setLocalPeerId("local-device");
        OverlayWindow window(QGuiApplication::primaryScreen()); window.setContentHeight(380); window.resize(800,380);
        window.setInitialProperties({{"model",QVariant::fromValue(&model)},{"nativeOverlay",QVariant::fromValue(&window)},
            {"sessionClient",QVariant::fromValue(static_cast<QObject*>(&f.a))},{"serverUrl",f.url().toString()},{"reducedMotion",true}});
        window.setSource(QUrl::fromLocalFile(PET_MAIN_FILE)); QCOMPARE(window.status(),QQuickView::Ready); window.show();
        QVERIFY(QMetaObject::invokeMethod(window.rootObject(),"openConnection")); QTest::qWait(100);
        QVERIFY(window.grabWindow().save(path+"/server.png")); QVERIFY(f.connect());
        // Create the fixture invitation outside this client so no invitation capability is captured.
        const auto invitation=f.db.createInvite(f.a.peerId(),QDateTime::currentDateTimeUtc());
        f.b.requestControl("invite.join",{{"invite",invitation}}); QVERIFY(until([&]{return !f.a.joinRequestId().isEmpty();}));
        std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* root)->QQuickItem* {
            if(root->objectName()=="approveJoin") return root; for(auto* child:root->childItems()) if(auto* result=find(child)) return result; return nullptr;
        };
        auto* approve=find(window.rootObject()); QVERIFY(approve); approve->forceActiveFocus(); QTest::qWait(100);
        QVERIFY(window.grabWindow().save(path+"/approval.png"));
        f.a.requestControl("invite.approve",{{"joinRequestId",f.a.joinRequestId()}}); QVERIFY(until([&]{return f.a.paired() && f.b.paired();}));
        QTest::qWait(100); QVERIFY(window.grabWindow().save(path+"/connected.png"));
        window.resize(320,250); QTest::qWait(100); QVERIFY(window.grabWindow().save(path+"/short.png"));
    }
};
QTEST_MAIN(ClientTest)
#include "tst_client.moc"
