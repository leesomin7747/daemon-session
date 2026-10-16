#include "SessionBroker.h"
#include "WireProtocol.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QUuid>
#include <QTimeZone>
#include <QTcpSocket>
#include <QtTest>
#include <QWebSocket>

namespace {
bool until(const std::function<bool()>& predicate, int timeout = 1200)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeout) QTest::qWait(5);
    return predicate();
}
QByteArray json(const QJsonObject& o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
QString secret(const QByteArray& bytes)
{ return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)); }

struct Client {
    QWebSocket ws;
    QVector<QJsonObject> frames;
    Client() {
        QObject::connect(&ws, &QWebSocket::textMessageReceived, &ws, [this](const QString& text) {
            frames.append(QJsonDocument::fromJson(text.toUtf8()).object());
        });
    }
    bool open(quint16 port) {
        ws.open(QUrl(QString("ws://127.0.0.1:%1/").arg(port)));
        return until([&] { return ws.state() == QAbstractSocket::ConnectedState; });
    }
    void send(const QString& op, const QString& id, const QJsonObject& body = {}) {
        ws.sendTextMessage(QString::fromUtf8(json({{"v",1},{"op",op},{"requestId",id},{"body",body}})));
    }
    QJsonObject find(const QString& op, const QString& id = {}) const {
        for (const auto& f : frames)
            if (f["op"] == op && (id.isEmpty() || f["requestId"] == id)) return f;
        return {};
    }
    QJsonObject wait(const QString& op, const QString& id = {}) {
        until([&] { return !find(op,id).isEmpty(); });
        return find(op,id);
    }
    int count(const QString& op) const {
        int n=0;
        for (const auto& f : frames) if (f["op"] == op) ++n;
        return n;
    }
    void command(const QString& id, const QString& action, const QString& target,
                 const QString& text = {}, bool busy = false) {
        send("session.command", id, {{"requestId",id},{"action",action},
            {"targetId",target},{"text",text},{"busy",busy}});
    }
    QJsonObject snapshot(const QString& id) {
        send("session.snapshot",id);
        return wait("result",id)["body"].toObject();
    }
};
struct Fixture {
    qint64 ms = 0;
    QDateTime start = QDateTime::fromMSecsSinceEpoch(1800000000000LL, QTimeZone::UTC);
    PairStore store{":memory:"};
    SessionBroker broker{store, BrokerClock{[this]{return ms;}, [this]{return start.addMSecs(ms);}}};
    DeviceCredential a = store.createDevice(), b = store.createDevice();
    Client ca, cb;
    bool listen() { return broker.listen(QHostAddress::LocalHost,0); }
    bool authenticate(Client& c, const DeviceCredential& device) {
        if (!c.open(broker.serverPort())) return false;
        c.send("auth","auth",{{"token",secret(device.token)}});
        return c.wait("result","auth")["body"].toObject()["peerId"] == device.id;
    }
    bool pair() {
        if (!listen()) return false;
        const auto code=store.createInvite(a.id,start);
        const auto request=store.requestJoin(code,b.id,start);
        if (store.approveJoin(a.id,request,start).isEmpty()) return false;
        if (!authenticate(ca,a) || !authenticate(cb,b)) return false;
        if (ca.wait("session.snapshot").isEmpty() || cb.wait("session.snapshot").isEmpty()) return false;
        ca.frames.clear(); cb.frames.clear();
        return true;
    }
};
QJsonObject peer(const QJsonObject& state, const QString& id) {
    for (const auto& v : state["peers"].toArray())
        if (v.toObject()["id"] == id) return v.toObject();
    return {};
}
}

class BrokerTest : public QObject {
    Q_OBJECT
private slots:
    void strictSharedEnvelope();
    void controlFramesEstablishMutuallyApprovedPair();
    void authTimeoutClosesSocket();
    void bothPeersReceiveIdenticalEvent();
    void busyRaceRespectsServerOrder();
    void disconnectBeforeNoteRejectsWithoutReplay();
    void duplicateRequestDoesNotRepeat();
    void reconnectGetsSnapshotNotOldAnimation();
    void unknownPeerCannotJoin();
    void revokeInvalidatesBothLiveSockets();
    void oversizedFrameAndFloodAreBounded();
    void deviceRateSurvivesReconnect();
    void registrationAndInvitesShareIpBudget();
    void heartbeatAndDepartureUseInjectedClock();
    void reconnectCancelsDepartureAndExpiredSessionIsFresh();
    void controlRetriesHaveNoDuplicateSideEffects();
    void malformedBodiesNeverMutateState();
    void listenRejectsRemotePlaintext();
    void serverCloseDropsLiveSockets();
    void inboxLimitAndAcknowledgement();
    void cacheEvictsOldestAndSeparatesActors();
    void controlsUseDeviceBudgetAcrossReconnect();
    void expiredHeartbeatCannotSendBeforeSweep();
    void slowReceiverWriteBufferIsBounded();
    void startupPersistsAuthenticationAndRejectsBadOptions();
    void nonBmpRequestIdRoundTrips();
    void unsafeHandshakeAndBinaryFramesAreRejected();
    void rateLimitedCommandRetryKeepsOriginalFailure();
    void pendingJoinNotifiesReconnectedIssuer();
    void storageFailureReturnsSafeErrorAndCloses();
};

void BrokerTest::strictSharedEnvelope()
{
    const QByteArray good=R"({"v":1,"op":"auth","requestId":"r","body":{"token":"abc"}})";
    const auto decoded=decodeFrame(good);
    QVERIFY(decoded);
    QCOMPARE(decoded->op,QString("auth"));
    QCOMPARE(decoded->body["token"].toString(),QString("abc"));
    QVERIFY(decodeFrame(encodeFrame({"session.notice",{},{{"text","hello"}}})));
    for (const auto& bad : QList<QByteArray>{
        R"({"v":2,"op":"auth","requestId":"r","body":{}})",
        R"({"v":1,"op":"auth","requestId":"","body":{}})",
        R"({"v":1,"op":"execute","requestId":"r","body":{}})",
        R"({"v":1,"op":"auth","requestId":"r","body":{},"actor":"x"})",
        R"({"v":1,"op":"auth","requestId":1,"body":{}})",
        R"({"v":1,"op":"auth","requestId":"r","body":[]})",
        QByteArray(MaxFrameBytes+1,'x'), QByteArray("\xff",1)}) QVERIFY(!decodeFrame(bad));
    QVERIFY(encodeFrame({"auth",QString(129,'a'),{}}).isEmpty());
}
void BrokerTest::controlFramesEstablishMutuallyApprovedPair()
{
    Fixture f;
    QVERIFY(f.listen());
    Client a,b;
    QVERIFY(a.open(f.broker.serverPort())); QVERIFY(b.open(f.broker.serverPort()));
    a.send("device.register","reg-a"); b.send("device.register","reg-b");
    const auto da=a.wait("result","reg-a")["body"].toObject();
    const auto db=b.wait("result","reg-b")["body"].toObject();
    QVERIFY(!da["peerId"].toString().isEmpty()); QVERIFY(!db["token"].toString().isEmpty());
    a.send("auth","auth-a",{{"token",da["token"]}}); b.send("auth","auth-b",{{"token",db["token"]}});
    QVERIFY(!a.wait("result","auth-a").isEmpty()); QVERIFY(!b.wait("result","auth-b").isEmpty());
    a.send("invite.create","code");
    const QJsonValue code=a.wait("result","code")["body"].toObject()["invite"];
    b.send("invite.join","join",{{"invite",code}});
    const QJsonValue request=b.wait("result","join")["body"].toObject()["joinRequestId"];
    QCOMPARE(a.wait("join.requested")["body"].toObject()["joinRequestId"],request);
    b.command("before","ping",da["peerId"].toString());
    QCOMPARE(b.wait("error","before")["body"].toObject()["code"].toString(),QString("not_paired"));
    a.send("invite.approve","approve",{{"joinRequestId",request}});
    QVERIFY(!a.wait("result","approve")["body"].toObject()["pairId"].toString().isEmpty());
    const auto sa=a.wait("session.snapshot")["body"].toObject();
    QCOMPARE(sa,b.wait("session.snapshot")["body"].toObject());
    QCOMPARE(sa["peers"].toArray().size(),2);
    QCOMPARE(a.wait("session.notice")["body"],b.wait("session.notice")["body"]);
}
void BrokerTest::authTimeoutClosesSocket()
{
    Fixture f; QVERIFY(f.listen()); Client c; QVERIFY(c.open(f.broker.serverPort()));
    f.ms=4999; QTest::qWait(130); QCOMPARE(c.ws.state(),QAbstractSocket::ConnectedState);
    f.ms=5000; QVERIFY(until([&]{return c.ws.state()==QAbstractSocket::UnconnectedState;}));
}
void BrokerTest::bothPeersReceiveIdenticalEvent()
{
    Fixture f; QVERIFY(f.pair());
    f.ca.command("move","rope",f.b.id);
    const auto a=f.ca.wait("session.event")["body"].toObject();
    QCOMPARE(a,f.cb.wait("session.event")["body"].toObject());
    QVERIFY(decodeEvent(json(a))); QCOMPARE(a["finalX"].toDouble(),0.37);
    QVERIFY(!a["eventId"].toString().isEmpty()); QVERIFY(!a["bubble"].toString().isEmpty());
    QCOMPARE(f.ca.wait("result","move")["body"].toObject()["eventId"],a["eventId"]);
}
void BrokerTest::busyRaceRespectsServerOrder()
{
    Fixture f; QVERIFY(f.pair());
    f.cb.command("busy","setBusy",f.b.id,{},true); QVERIFY(!f.cb.wait("result","busy").isEmpty());
    f.ca.command("late","ping",f.b.id);
    QCOMPARE(f.ca.wait("error","late")["body"].toObject()["code"].toString(),QString("busy"));
    f.ca.command("gift","coffee",f.b.id); QVERIFY(!f.ca.wait("result","gift").isEmpty());
    auto state=f.cb.snapshot("s"); QCOMPARE(peer(state,f.b.id)["inbox"].toArray().size(),1);
    f.cb.command("ready","setBusy",f.b.id); QVERIFY(!f.cb.wait("result","ready").isEmpty());
    f.ca.command("early","ping",f.b.id); QVERIFY(!f.ca.wait("result","early").isEmpty());
    f.cb.command("again","setBusy",f.b.id,{},true); QVERIFY(!f.cb.wait("result","again").isEmpty());
    QCOMPARE(peer(f.cb.snapshot("after"),f.b.id)["inbox"].toArray().size(),1);
}
void BrokerTest::disconnectBeforeNoteRejectsWithoutReplay()
{
    Fixture f; QVERIFY(f.pair()); f.cb.ws.abort();
    QVERIFY(until([&]{for(const auto& v:f.ca.frames) if(v["op"]=="session.snapshot"
        && !peer(v["body"].toObject(),f.b.id)["reachable"].toBool()) return true; return false;}));
    f.ca.command("lost","note",f.b.id,"보고 싶어");
    const auto rejected=f.ca.wait("error","lost");
    QCOMPARE(rejected["body"].toObject()["code"].toString(),QString("offline"));
    Client back; QVERIFY(f.authenticate(back,f.b)); QVERIFY(!back.wait("session.snapshot").isEmpty());
    f.ca.frames.clear(); f.ca.command("lost","note",f.b.id,"보고 싶어");
    QCOMPARE(f.ca.wait("error","lost"),rejected);
    QCOMPARE(peer(back.snapshot("s"),f.b.id)["inbox"].toArray().size(),0);
    QCOMPARE(back.count("session.event"),0);
}
void BrokerTest::duplicateRequestDoesNotRepeat()
{
    Fixture f; QVERIFY(f.pair());
    f.ca.command("same","note",f.b.id,"한 모금 ☕"); const auto result=f.ca.wait("result","same");
    QVERIFY(!result.isEmpty()); QVERIFY(!f.cb.wait("session.event").isEmpty());
    f.ca.frames.clear(); f.cb.frames.clear();
    f.ca.command("same","note",f.b.id,"한 모금 ☕"); QCOMPARE(f.ca.wait("result","same"),result);
    QCOMPARE(f.cb.count("session.event"),0);
    QCOMPARE(peer(f.cb.snapshot("s"),f.b.id)["inbox"].toArray().size(),1);
    f.ca.command("same","note",f.b.id,"changed");
    QCOMPARE(f.ca.wait("error","same")["body"].toObject()["code"].toString(),QString("request_conflict"));
    Client replacement; QVERIFY(f.authenticate(replacement,f.a));
    QVERIFY(until([&]{return f.ca.ws.state()==QAbstractSocket::UnconnectedState;}));
    replacement.command("same","note",f.b.id,"한 모금 ☕"); QCOMPARE(replacement.wait("result","same"),result);
    QCOMPARE(replacement.count("session.event"),0);
}
void BrokerTest::reconnectGetsSnapshotNotOldAnimation()
{
    Fixture f; QVERIFY(f.pair());
    f.ca.command("lure","lure",f.b.id); QVERIFY(!f.ca.wait("result","lure").isEmpty());
    f.ca.command("note","note",f.b.id,"<b>과제</b>\n🙂"); QVERIFY(!f.ca.wait("result","note").isEmpty());
    f.cb.ws.abort(); Client back; QVERIFY(f.authenticate(back,f.b));
    const auto state=back.wait("session.snapshot")["body"].toObject();
    QVERIFY(decodeSnapshot(json(state))); QCOMPARE(peer(state,f.b.id)["x"].toDouble(),0.41);
    QCOMPARE(peer(state,f.b.id)["inbox"].toArray()[0].toObject()["text"].toString(),QString("<b>과제</b>\n🙂"));
    QCOMPARE(back.count("session.event"),0);
    back.command("ack","ackInbox",f.b.id); QVERIFY(!back.wait("result","ack").isEmpty());
    QVERIFY(peer(back.snapshot("s"),f.b.id)["inbox"].toArray().isEmpty());
}
void BrokerTest::unknownPeerCannotJoin()
{
    Fixture f; QVERIFY(f.pair()); Client bad; QVERIFY(bad.open(f.broker.serverPort()));
    bad.send("auth","bad",{{"token",secret(QByteArray(32,'x'))}});
    QVERIFY(until([&]{return bad.ws.state()==QAbstractSocket::UnconnectedState;}));
    const auto third=f.store.createDevice(); Client c; QVERIFY(f.authenticate(c,third));
    c.command("steal","note",f.b.id,"x");
    QCOMPARE(c.wait("error","steal")["body"].toObject()["code"].toString(),QString("not_paired"));
    f.ca.send("session.command","spoof",{{"requestId","spoof"},{"action","setWip"},
        {"targetId",f.b.id},{"text","x"},{"busy",false},{"actorId",f.b.id}});
    QCOMPARE(f.ca.wait("error","spoof")["body"].toObject()["code"].toString(),QString("invalid"));
    f.ca.command("forbid","setWip",f.b.id,"x");
    QCOMPARE(f.ca.wait("error","forbid")["body"].toObject()["code"].toString(),QString("forbidden"));
}
void BrokerTest::revokeInvalidatesBothLiveSockets()
{
    Fixture f; QVERIFY(f.pair()); f.ca.send("pair.revoke","revoke");
    QVERIFY(!f.ca.wait("result","revoke").isEmpty());
    QVERIFY(until([&]{return f.ca.ws.state()==QAbstractSocket::UnconnectedState
        && f.cb.ws.state()==QAbstractSocket::UnconnectedState;}));
    QVERIFY(!f.store.pairForPeer(f.a.id)); QVERIFY(!f.store.pairForPeer(f.b.id));
    Client back; QVERIFY(f.authenticate(back,f.b)); back.send("session.snapshot","s");
    QCOMPARE(back.wait("error","s")["body"].toObject()["code"].toString(),QString("not_paired"));
}
void BrokerTest::oversizedFrameAndFloodAreBounded()
{
    Fixture f; QVERIFY(f.pair()); Client huge; QVERIFY(f.authenticate(huge,f.a));
    huge.ws.setOutgoingFrameSize(1024);
    huge.ws.sendTextMessage(QString(MaxFrameBytes+1,'x'));
    QVERIFY(until([&]{return huge.ws.state()==QAbstractSocket::UnconnectedState;}));
    Client flood; QVERIFY(f.authenticate(flood,f.a));
    for(int i=0;i<21;++i) flood.command(QString("r%1").arg(i),"ping",f.b.id);
    QCOMPARE(flood.wait("error","r20")["body"].toObject()["code"].toString(),QString("rate_limited"));
    QVERIFY(until([&]{return f.cb.count("session.event")==20;}));
    QCOMPARE(f.cb.count("session.event"),20);
}
void BrokerTest::deviceRateSurvivesReconnect()
{
    Fixture f; QVERIFY(f.pair());
    for(int i=0;i<20;++i) f.ca.command(QString("r%1").arg(i),"ping",f.b.id);
    QVERIFY(!f.ca.wait("result","r19").isEmpty()); Client back; QVERIFY(f.authenticate(back,f.a));
    back.command("limited","ping",f.b.id);
    QCOMPARE(back.wait("error","limited")["body"].toObject()["code"].toString(),QString("rate_limited"));
    f.ms=100; back.command("refill","ping",f.b.id); QVERIFY(!back.wait("result","refill").isEmpty());
}
void BrokerTest::registrationAndInvitesShareIpBudget()
{
    Fixture f; QVERIFY(f.listen()); QVERIFY(f.authenticate(f.ca,f.a));
    for(int i=0;i<10;++i) {f.ca.send("invite.create",QString("i%1").arg(i));
        QVERIFY(!f.ca.wait("result",QString("i%1").arg(i)).isEmpty());}
    Client c; QVERIFY(c.open(f.broker.serverPort())); c.send("device.register","reg");
    QCOMPARE(c.wait("error","reg")["body"].toObject()["code"].toString(),QString("rate_limited"));
    f.ms=60000;
    // This unauthenticated socket has expired; a fresh socket can use the refilled IP budget.
    Client fresh; QVERIFY(fresh.open(f.broker.serverPort())); fresh.send("device.register","new");
    QVERIFY(!fresh.wait("result","new")["body"].toObject()["token"].toString().isEmpty());
}
void BrokerTest::heartbeatAndDepartureUseInjectedClock()
{
    Fixture f; QVERIFY(f.pair()); QSignalSpy ping(&f.ca.ws,&QWebSocket::pong);
    // Automatic protocol pong counts only in response to the broker's outstanding ping.
    f.ms=10000; QTest::qWait(160);
    f.ms=30000; QTest::qWait(160); QCOMPARE(f.ca.ws.state(),QAbstractSocket::ConnectedState);
    f.ca.frames.clear();
    f.ms=60000; QVERIFY(until([&]{return f.cb.ws.state()==QAbstractSocket::UnconnectedState;}));
    // Rejoin one side while the other stays offline; no departure joke before the grace expires.
    Client back; QVERIFY(f.authenticate(back,f.a)); QVERIFY(!back.wait("session.snapshot").isEmpty());
    back.frames.clear(); f.ms=74999; QTest::qWait(140); QCOMPARE(back.count("session.notice"),0);
    f.ms=75000; QVERIFY(!back.wait("session.notice").isEmpty());
}
void BrokerTest::reconnectCancelsDepartureAndExpiredSessionIsFresh()
{
    Fixture f; QVERIFY(f.pair()); const QJsonValue epoch=f.ca.snapshot("initial")["epoch"];
    f.cb.ws.abort(); QTest::qWait(30); f.ms=14000;
    Client back; QVERIFY(f.authenticate(back,f.b)); QVERIFY(!back.wait("session.snapshot").isEmpty());
    back.frames.clear(); f.ca.frames.clear(); f.ms=15000; QTest::qWait(160);
    QCOMPARE(back.count("session.notice"),0); QCOMPARE(f.ca.count("session.notice"),0);
    f.ca.ws.abort(); back.ws.abort(); QTest::qWait(30); f.ms=30000; QTest::qWait(160);
    Client fresh; QVERIFY(f.authenticate(fresh,f.a));
    const auto state=fresh.wait("session.snapshot")["body"].toObject();
    QVERIFY(state["epoch"]!=epoch); QVERIFY(!peer(state,f.b.id)["reachable"].toBool());
}
void BrokerTest::controlRetriesHaveNoDuplicateSideEffects()
{
    Fixture f; QVERIFY(f.listen()); QVERIFY(f.ca.open(f.broker.serverPort()));
    f.ca.send("device.register","reg"); const auto result=f.ca.wait("result","reg");
    QVERIFY(!result.isEmpty()); f.ca.frames.clear(); f.ca.send("device.register","reg");
    QCOMPARE(f.ca.wait("result","reg"),result);
    f.ca.send("auth","auth",{{"token",result["body"].toObject()["token"]}});
    QVERIFY(!f.ca.wait("result","auth").isEmpty());
    f.ca.send("invite.create","code"); const auto code=f.ca.wait("result","code");
    f.ca.frames.clear(); f.ca.send("invite.create","code"); QCOMPARE(f.ca.wait("result","code"),code);
    QVERIFY(!f.store.requestJoin(code["body"].toObject()["invite"].toString(),f.b.id,f.start).isEmpty());
}
void BrokerTest::malformedBodiesNeverMutateState()
{
    Fixture f; QVERIFY(f.pair()); const auto initial=f.ca.snapshot("initial");
    f.ca.send("session.command","outer",{{"requestId","inner"},{"action","note"},
        {"targetId",f.b.id},{"text","x"},{"busy",false}});
    QCOMPARE(f.ca.wait("error","outer")["body"].toObject()["code"].toString(),QString("invalid"));
    f.ca.send("pair.revoke","bad-revoke",{{"actorId",f.b.id}});
    QCOMPARE(f.ca.wait("error","bad-revoke")["body"].toObject()["code"].toString(),QString("invalid"));
    QCOMPARE(f.ca.snapshot("after"),initial);
}
void BrokerTest::listenRejectsRemotePlaintext()
{
    Fixture f; QVERIFY(f.listen()); f.broker.close();
    QVERIFY(!f.broker.listen(QHostAddress::Any,0)); QVERIFY(!f.broker.listen(QHostAddress("192.0.2.1"),0));
}
void BrokerTest::serverCloseDropsLiveSockets()
{
    Fixture f; QVERIFY(f.pair()); f.broker.close();
    QVERIFY(until([&]{return f.ca.ws.state()==QAbstractSocket::UnconnectedState
        && f.cb.ws.state()==QAbstractSocket::UnconnectedState;}));
    QVERIFY(f.broker.listen(QHostAddress::LocalHost,0));
}
void BrokerTest::inboxLimitAndAcknowledgement()
{
    Fixture f; QVERIFY(f.pair());
    for(int i=0;i<100;++i) {
        f.ms=i*100;
        const auto id=QString("n%1").arg(i); f.ca.command(id,"note",f.b.id,"x");
        QVERIFY2(!f.ca.wait("result",id).isEmpty(),qPrintable(id));
    }
    f.ms=10000; f.ca.command("full","note",f.b.id,"x");
    QCOMPARE(f.ca.wait("error","full")["body"].toObject()["code"].toString(),QString("inbox_full"));
    const auto state=f.cb.snapshot("s");
    QCOMPARE(peer(state,f.b.id)["inbox"].toArray().size(),100);
    f.cb.command("ack","ackInbox",f.b.id); QVERIFY(!f.cb.wait("result","ack").isEmpty());
    f.ms=10100; f.ca.command("next","note",f.b.id,"still here");
    QVERIFY(!f.ca.wait("result","next").isEmpty());
    QCOMPARE(peer(f.cb.snapshot("after"),f.b.id)["inbox"].toArray().size(),1);
}
void BrokerTest::cacheEvictsOldestAndSeparatesActors()
{
    Fixture f; QVERIFY(f.pair());
    f.ca.command("shared","setWip",f.a.id,"a"); QVERIFY(!f.ca.wait("result","shared").isEmpty());
    f.cb.command("shared","setWip",f.b.id,"b"); QVERIFY(!f.cb.wait("result","shared").isEmpty());
    for(int i=0;i<256;++i) {
        f.ms=i*100; const auto id=QString("p%1").arg(i);
        f.ca.command(id,"ping",f.b.id); QVERIFY2(!f.ca.wait("result",id).isEmpty(),qPrintable(id));
    }
    f.ms=25600; f.ca.frames.clear(); f.cb.frames.clear();
    f.ca.command("shared","setWip",f.a.id,"new after eviction");
    QVERIFY(!f.ca.wait("result","shared").isEmpty()); QVERIFY(!f.cb.wait("session.event").isEmpty());
    const auto state=f.cb.snapshot("s");
    QCOMPARE(peer(state,f.a.id)["wip"].toString(),QString("new after eviction"));
    QCOMPARE(peer(state,f.b.id)["wip"].toString(),QString("b"));
}
void BrokerTest::controlsUseDeviceBudgetAcrossReconnect()
{
    Fixture f; QVERIFY(f.pair());
    for(int i=0;i<20;++i) QVERIFY(!f.ca.snapshot(QString("s%1").arg(i)).isEmpty());
    Client back; QVERIFY(f.authenticate(back,f.a)); back.send("session.snapshot","limited");
    QCOMPARE(back.wait("error","limited")["body"].toObject()["code"].toString(),QString("rate_limited"));
}
void BrokerTest::expiredHeartbeatCannotSendBeforeSweep()
{
    Fixture f; QVERIFY(f.pair());
    f.ms=30000; f.ca.command("late","note",f.b.id,"must not arrive");
    QVERIFY(until([&]{return f.ca.ws.state()==QAbstractSocket::UnconnectedState;}));
    QCOMPARE(f.ca.count("result"),0); QCOMPARE(f.cb.count("session.event"),0);
}
void BrokerTest::slowReceiverWriteBufferIsBounded()
{
    Fixture f; QVERIFY(f.pair());
    for(int i=0;i<80;++i) {
        f.ms=i*100; const auto id=QString("n%1").arg(i);
        f.cb.command(id,"note",f.a.id,QString(40,'x'));
        QVERIFY(!f.cb.wait("result",id).isEmpty());
    }
    // Queue several large replies in one read cycle, without draining them between requests.
    for(int i=0;i<20;++i) f.ca.send("session.snapshot",QString("s%1").arg(i));
    QVERIFY(until([&]{return f.ca.ws.state()==QAbstractSocket::UnconnectedState;}));
    QVERIFY(until([&]{for(const auto& frame:f.cb.frames) if(frame["op"]=="session.snapshot"
        && !peer(frame["body"].toObject(),f.a.id)["reachable"].toBool()) return true; return false;}));
}
void BrokerTest::startupPersistsAuthenticationAndRejectsBadOptions()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    const auto path=dir.filePath("pair.sqlite");
    QProcess process;
    const auto program=QCoreApplication::applicationDirPath()+"/../server/daemon-session-server";
    const auto launch=[&]() -> quint16 {
        process.start(program,{"--db",path,"--port","0"});
        if (!process.waitForStarted(1500) || !process.waitForReadyRead(1500)) return 0;
        const auto message=QString::fromUtf8(process.readAllStandardOutput());
        return QRegularExpression(":(\\d+)/").match(message).captured(1).toUShort();
    };
    const auto port=launch(); QVERIFY(port!=0); Client c; QVERIFY(c.open(port));
    c.send("device.register","reg"); const auto credentials=c.wait("result","reg")["body"].toObject();
    QVERIFY(!credentials["token"].toString().isEmpty());
    process.kill(); QVERIFY(process.waitForFinished(1500));
    const auto newPort=launch(); QVERIFY(newPort!=0); Client back; QVERIFY(back.open(newPort));
    back.send("auth","auth",{{"token",credentials["token"]}});
    QCOMPARE(back.wait("result","auth")["body"].toObject()["peerId"],credentials["peerId"]);
    process.kill(); QVERIFY(process.waitForFinished(1500));
    for (const auto& args : QList<QStringList>{{"--port","abc"},{"--port","65536"},{"--db",":memory:"}}) {
        process.start(program,args); QVERIFY(process.waitForFinished(1500));
        QCOMPARE(process.exitCode(),2);
    }
}
void BrokerTest::nonBmpRequestIdRoundTrips()
{
    const QString id=QString::fromUtf8("작업🙂");
    QVERIFY(decodeFrame(encodeFrame({"session.snapshot",id,{}})));
    Fixture f; QVERIFY(f.pair()); QVERIFY(!f.ca.snapshot(id).isEmpty());
}
void BrokerTest::unsafeHandshakeAndBinaryFramesAreRejected()
{
    Fixture f; QVERIFY(f.listen());
    QWebSocket web("https://untrusted.example");
    QSignalSpy errors(&web,&QWebSocket::errorOccurred);
    QSignalSpy accepted(&web,&QWebSocket::connected);
    web.open(QUrl(QString("ws://127.0.0.1:%1/").arg(f.broker.serverPort())));
    QVERIFY(until([&]{return !errors.isEmpty() || !accepted.isEmpty();}));
    QVERIFY(!errors.isEmpty()); QVERIFY(accepted.isEmpty());
    QCOMPARE(web.state(),QAbstractSocket::UnconnectedState);
    QWebSocket query;
    query.open(QUrl(QString("ws://127.0.0.1:%1/?token=secret").arg(f.broker.serverPort())));
    QSignalSpy disconnected(&query,&QWebSocket::disconnected);
    QVERIFY(until([&]{return !disconnected.isEmpty();}));
    Client binary; QVERIFY(f.authenticate(binary,f.a)); binary.ws.sendBinaryMessage("{}");
    QVERIFY(until([&]{return binary.ws.state()==QAbstractSocket::UnconnectedState;}));
}
void BrokerTest::rateLimitedCommandRetryKeepsOriginalFailure()
{
    Fixture f; QVERIFY(f.pair());
    for (int i=0;i<20;++i) f.ca.command(QString("p%1").arg(i),"ping",f.b.id);
    QVERIFY(!f.ca.wait("result","p19").isEmpty());
    f.ca.command("limited","note",f.b.id,"must remain failed");
    const auto failed=f.ca.wait("error","limited"); QVERIFY(!failed.isEmpty());
    f.ms=1000; f.ca.frames.clear(); f.cb.frames.clear();
    f.ca.command("limited","note",f.b.id,"must remain failed");
    QCOMPARE(f.ca.wait("error","limited"),failed);
    QCOMPARE(f.cb.count("session.event"),0);
    QCOMPARE(peer(f.cb.snapshot("s"),f.b.id)["inbox"].toArray().size(),0);
}
void BrokerTest::pendingJoinNotifiesReconnectedIssuer()
{
    Fixture f; QVERIFY(f.listen()); QVERIFY(f.authenticate(f.ca,f.a)); QVERIFY(f.authenticate(f.cb,f.b));
    f.ca.send("invite.create","code");
    const QJsonValue code=f.ca.wait("result","code")["body"].toObject()["invite"];
    f.ca.ws.abort(); QTest::qWait(30);
    f.cb.send("invite.join","join",{{"invite",code}});
    const QJsonValue request=f.cb.wait("result","join")["body"].toObject()["joinRequestId"];
    QVERIFY(!request.toString().isEmpty());
    Client back; QVERIFY(f.authenticate(back,f.a));
    QCOMPARE(back.wait("join.requested")["body"].toObject()["joinRequestId"],request);
    back.send("invite.approve","approve",{{"joinRequestId",request}});
    QVERIFY(!back.wait("result","approve")["body"].toObject()["pairId"].toString().isEmpty());
}
void BrokerTest::storageFailureReturnsSafeErrorAndCloses()
{
    QTemporaryDir dir; const auto path=dir.filePath("fault.sqlite"); PairStore store(path);
    const auto device=store.createDevice(); SessionBroker broker(store);
    QVERIFY(broker.listen(QHostAddress::LocalHost,0)); Client c; QVERIFY(c.open(broker.serverPort()));
    c.send("auth","auth",{{"token",secret(device.token)}}); QVERIFY(!c.wait("result","auth").isEmpty());
    const auto connection=QUuid::createUuid().toString();
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE",connection); db.setDatabaseName(path); QVERIFY(db.open());
        { QSqlQuery query(db); QVERIFY(query.exec("DROP TABLE devices")); }
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    c.send("invite.create","fail");
    QCOMPARE(c.wait("error","fail")["body"].toObject(),QJsonObject({{"code","storage_unavailable"}}));
    QVERIFY(until([&]{return c.ws.state()==QAbstractSocket::UnconnectedState;}));
}
QTEST_GUILESS_MAIN(BrokerTest)
#include "tst_broker.moc"
