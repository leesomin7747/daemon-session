#include "SessionClient.h"
#include "WireProtocol.h"
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QPointer>
#include <QQueue>
#include <QRandomGenerator>
#include <QSet>
#include <QSslError>
#include <QTimer>
#include <QUuid>
#include <QWebSocket>

namespace {
QString uuid() {return QUuid::createUuid().toString(QUuid::WithoutBraces);}
QByteArray json(const QJsonObject& o) {return QJsonDocument(o).toJson(QJsonDocument::Compact);}
QJsonObject object(const QByteArray& b) {return QJsonDocument::fromJson(b).object();}
bool keys(const QJsonObject& o,std::initializer_list<const char*> names) {
    if(o.size()!=qsizetype(names.size())) return false;
    for(auto name:names) if(!o.contains(QLatin1String(name))) return false;
    return true;
}
bool id(const QString& value) {return isValidCommand({"validation",Action::Ping,value,{},false});}
QByteArray token(const QString& text) {
    const auto bytes=QByteArray::fromBase64(text.toLatin1(),QByteArray::Base64UrlEncoding);
    return bytes.size()==32 && QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals))==text?bytes:QByteArray();
}
bool safeUrl(const QUrl& u) {
    if(!u.isValid() || (u.scheme()!="ws" && u.scheme()!="wss") || u.host().isEmpty()
        || !u.userInfo().isEmpty() || u.hasQuery() || u.hasFragment() || (u.path()!="/" && !u.path().isEmpty()) || u.port(1)<1) return false;
    return u.scheme()=="wss" || QHostAddress(u.host()).isLoopback();
}
bool controlBody(const QString& op,const QJsonObject& body) {
    if(op=="invite.create" || op=="session.snapshot") return body.isEmpty();
    if(op=="invite.join") return keys(body,{"invite"}) && body["invite"].isString() && !token(body["invite"].toString()).isEmpty();
    if(op=="invite.approve") return keys(body,{"joinRequestId"}) && body["joinRequestId"].isString() && id(body["joinRequestId"].toString());
    return false;
}
}
struct SessionClient::Impl {
    struct Pending {QString op; QTimer* deadline=nullptr; std::optional<Command> command;};
    SessionClient* owner;
    QPointer<CredentialStore> store;
    ClientOptions options;
    QWebSocket* socket=nullptr;
    QTimer retry,heartbeat;
    QElapsedTimer clock;
    QHash<QString,Pending> pending;
    QSet<QString> retired,oldEpochs;
    QQueue<QString> retiredOrder,epochOrder;
    DeviceCredential credential;
    QUrl url;
    QString state="offline",epoch,invite,joinRequest,joinPeer,clearState="offline";
    quint64 revision=0,generation=0;
    bool authenticated=false,haveSnapshot=false,recovering=false,wanted=false;
    bool loading=false,saving=false,clearing=false,persisted=false,forgetting=false,revoked=false;
    int attempt=0;
    qint64 lastPong=0,lastPing=0;
    QByteArray ping;
    Impl(SessionClient* q,CredentialStore* s,ClientOptions o):owner(q),store(s),options(std::move(o)) {
        clock.start(); retry.setSingleShot(true); heartbeat.setInterval(qMax(5,qMin(1000,options.heartbeatMs/2)));
        QObject::connect(&retry,&QTimer::timeout,owner,[this]{if(wanted && !saving && !loading) openSocket();});
        QObject::connect(&heartbeat,&QTimer::timeout,owner,[this]{
            if(!authenticated || !socket) return;
            const auto now=clock.elapsed();
            if(now-lastPong>=options.heartbeatTimeoutMs) {socket->abort(); return;}
            if(now-lastPing>=options.heartbeatMs) {lastPing=now; ping=uuid().toLatin1(); socket->ping(ping);}
        });
        QObject::connect(store,&CredentialStore::loaded,owner,[this](DeviceCredential value,bool found){
            if(!loading || !wanted) return;
            loading=false; credential=value; persisted=found; emit owner->connectionChanged(); openSocket();
        });
        QObject::connect(store,&CredentialStore::saved,owner,[this]{
            if(!saving || !wanted) return;
            saving=false; persisted=true;
            if(socket && socket->state()==QAbstractSocket::ConnectedState) authenticate(); else schedule();
        });
        QObject::connect(store,&CredentialStore::cleared,owner,[this]{
            if(!clearing) return;
            clearing=false; credential={}; persisted=false; invite.clear(); joinRequest.clear(); joinPeer.clear();
            setState(clearState); emit owner->connectionChanged();
        });
        QObject::connect(store,&CredentialStore::failed,owner,[this](const QString&){
            if(!loading && !saving && !clearing) return;
            loading=saving=clearing=false; fatal("secret_store");
        });
    }
    void setState(const QString& next) {state=next; emit owner->connectionChanged();}
    void remember(const QString& request) {
        if(retired.contains(request)) return;
        retired.insert(request); retiredOrder.enqueue(request);
        while(retiredOrder.size()>256) retired.remove(retiredOrder.dequeue());
    }
    std::optional<Pending> take(const QString& request) {
        auto it=pending.find(request); if(it==pending.end()) return {};
        auto value=it.value(); pending.erase(it); value.deadline->stop(); value.deadline->deleteLater(); remember(request); return value;
    }
    void failPending(const QString& code,bool notify=true) {
        const auto ids=pending.keys();
        for(const auto& request:ids) {auto p=take(request); if(notify && p && p->op!="auth" && p->op!="device.register") emit owner->error(request,code);}
        recovering=false;
    }
    void destroySocket() {
        ++generation; heartbeat.stop();
        if(socket) {auto* old=socket; socket=nullptr; QObject::disconnect(old,nullptr,owner,nullptr); old->abort(); old->deleteLater();}
    }
    void stop(bool notify=true) {
        wanted=loading=saving=false; retry.stop(); failPending("connection_lost",notify); destroySocket();
        authenticated=haveSnapshot=false; forgetting=false; setState("offline");
    }
    void fatal(const QString& code) {
        wanted=loading=saving=false; retry.stop(); failPending(code); destroySocket(); authenticated=haveSnapshot=false;
        setState(code=="tls"?"tls_error":code=="secret_store"?"secret_error":"protocol_error"); emit owner->error({},code);
    }
    void schedule() {
        if(!wanted || saving || loading || forgetting) return;
        const int base[]={1000,2000,4000,8000,15000};
        const int delay=options.reconnectDelayMs?options.reconnectDelayMs(attempt):qMin(15000,base[qMin(attempt,4)]+int(QRandomGenerator::global()->bounded(251)));
        attempt=qMin(attempt+1,4); retry.start(qBound(1,delay,15000)); setState("reconnecting"); emit owner->reconnectScheduled(retry.interval());
    }
    void clearCredentials(const QString& finalState) {
        wanted=false; retry.stop(); failPending("connection_lost"); destroySocket(); authenticated=haveSnapshot=false;
        forgetting=false; revoked=true; clearing=true; clearState=finalState; setState("forgetting"); store->clear();
    }
    bool current(QWebSocket* value,quint64 version) const {return socket==value && generation==version;}
    void openSocket() {
        if(!wanted || !store || store->busy()) return;
        failPending("connection_lost"); destroySocket(); authenticated=haveSnapshot=false;
        auto* value=new QWebSocket(QString(),QWebSocketProtocol::VersionLatest,owner); socket=value;
        value->setMaxAllowedIncomingFrameSize(MaxFrameBytes); value->setMaxAllowedIncomingMessageSize(MaxFrameBytes);
        value->setReadBufferSize(MaxFrameBytes+1024); const auto version=generation;
        QObject::connect(value,&QWebSocket::connected,owner,[this,value,version]{
            if(!current(value,version)) return;
            if(persisted) authenticate(); else {setState("registering"); submit("device.register",{},uuid());}
        });
        QObject::connect(value,&QWebSocket::textMessageReceived,owner,[this,value,version](const QString& text){if(current(value,version)) receive(text.toUtf8());});
        QObject::connect(value,&QWebSocket::binaryMessageReceived,owner,[this,value,version](const QByteArray&){if(current(value,version)) fatal("protocol");});
        QObject::connect(value,&QWebSocket::sslErrors,owner,[this,value,version](const QList<QSslError>&){if(current(value,version)) fatal("tls");});
        QObject::connect(value,&QWebSocket::pong,owner,[this,value,version](quint64,const QByteArray& payload){if(current(value,version) && !ping.isEmpty() && payload==ping) {lastPong=clock.elapsed(); ping.clear();}});
        QObject::connect(value,&QWebSocket::disconnected,owner,[this,value,version]{
            if(!current(value,version)) return;
            const auto reason=value->closeReason(); destroySocket(); authenticated=haveSnapshot=false;
            if(reason=="pair_revoked") {clearCredentials("revoked"); return;}
            failPending("connection_lost");
            if(forgetting) {forgetting=false; wanted=false; setState("offline"); emit owner->error({},"revoke_failed"); return;}
            if(wanted && !saving) schedule(); else setState(saving?"saving_credentials":"offline");
        });
        QObject::connect(value,&QWebSocket::errorOccurred,owner,[this,value,version](QAbstractSocket::SocketError){
            if(!current(value,version)) return;
            if(value->state()==QAbstractSocket::UnconnectedState) {destroySocket(); authenticated=haveSnapshot=false; failPending("connection_lost"); schedule();}
        });
        setState("connecting"); value->open(url);
    }
    void authenticate() {
        if(!socket || !persisted) return;
        setState("authenticating"); submit("auth",{{"token",QString::fromLatin1(credential.token.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals))}},uuid());
    }
    bool submit(const QString& op,const QJsonObject& body,const QString& request,std::optional<Command> command={}) {
        if(!socket || socket->state()!=QAbstractSocket::ConnectedState || pending.size()>=32 || pending.contains(request) || retired.contains(request)) {emit owner->error(request,"offline"); return false;}
        const auto bytes=encodeFrame({op,request,body}); if(bytes.isEmpty()) {emit owner->error(request,"invalid"); return false;}
        if(socket->bytesToWrite()+bytes.size()>64*1024) {emit owner->error(request,"backpressure"); socket->abort(); return false;}
        auto* deadline=new QTimer(owner); deadline->setSingleShot(true); pending.insert(request,{op,deadline,command}); const auto version=generation;
        QObject::connect(deadline,&QTimer::timeout,owner,[this,request,version]{
            if(generation!=version) return;
            const auto p=take(request); if(!p) return;
            if(p->op=="auth" || p->op=="device.register") {if(socket) socket->abort(); return;}
            if(p->op=="session.snapshot") recovering=false;
            if(p->op=="pair.revoke") {forgetting=false; wanted=false; destroySocket(); authenticated=haveSnapshot=false; setState("offline");}
            emit owner->error(request,"timeout");
        });
        deadline->start(options.requestTimeoutMs);
        if(socket->sendTextMessage(QString::fromUtf8(bytes))<0) {take(request); emit owner->error(request,"connection_lost"); if(socket) socket->abort(); return false;}
        return true;
    }
    void snapshot(const QJsonObject& body) {
        const auto value=decodeSnapshot(json(body)); if(!value || !value->peers.contains(credential.id)) {fatal("protocol"); return;}
        if(value->epoch==epoch && value->revision<revision) return;
        if(value->epoch!=epoch) {
            if(oldEpochs.contains(value->epoch)) return;
            if(!epoch.isEmpty()) {oldEpochs.insert(epoch); epochOrder.enqueue(epoch);}
            while(epochOrder.size()>32) oldEpochs.remove(epochOrder.dequeue());
        }
        epoch=value->epoch; revision=value->revision; haveSnapshot=true; recovering=false; setState("connected"); emit owner->snapshotReceived(*value);
    }
    void event(const QJsonObject& body) {
        const auto value=decodeEvent(json(body)); if(!value) {fatal("protocol"); return;}
        if(value->epoch==epoch && value->revision<=revision) return;
        if(!haveSnapshot || recovering || value->epoch!=epoch || value->revision!=revision+1) {owner->requestSnapshot(); return;}
        if(value->actorId==credential.id && pending.contains(value->requestId)) {
            const auto p=pending.value(value->requestId);
            if(!p.command || p.command->action!=value->action || p.command->targetId!=value->targetId
                || ((value->action==Action::Note || value->action==Action::SetWip) && p.command->text.trimmed()!=value->text)
                || (value->action==Action::SetBusy && p.command->busy!=value->busy)) {fatal("protocol"); return;}
            take(value->requestId);
        }
        revision=value->revision; emit owner->eventReceived(*value);
    }
    void result(const WireFrame& frame) {
        if(!pending.contains(frame.requestId)) {if(!retired.contains(frame.requestId)) fatal("protocol"); return;}
        const auto p=take(frame.requestId); const auto& body=frame.body;
        const auto bad=[&]{emit owner->error(frame.requestId,"protocol"); fatal("protocol");};
        if(frame.op=="error") {
            if(!keys(body,{"code"}) || !body["code"].isString() || body["code"].toString().size()>64) {bad(); return;}
            if(p->op=="session.snapshot") recovering=false;
            if(p->op=="pair.revoke") {forgetting=false; wanted=false; setState("connected");}
            emit owner->error(frame.requestId,body["code"].toString()); return;
        }
        if(p->op=="device.register") {
            const auto bytes=token(body["token"].toString());
            if(!keys(body,{"peerId","token"}) || !body["peerId"].isString() || !id(body["peerId"].toString()) || bytes.isEmpty()) {bad(); return;}
            credential={body["peerId"].toString(),bytes}; saving=true; setState("saving_credentials"); store->save(credential); return;
        }
        if(p->op=="auth") {
            if(!keys(body,{"peerId"}) || body["peerId"]!=credential.id) {bad(); return;}
            authenticated=true; attempt=0; lastPing=lastPong=clock.elapsed(); heartbeat.start(); setState("connected"); return;
        }
        if(p->op=="session.snapshot") {snapshot(body); return;}
        if(p->op=="pair.revoke") {
            if(!body.isEmpty()) {bad(); return;}
            emit owner->controlResult(frame.requestId,body); clearCredentials("offline"); return;
        }
        if(p->op=="invite.create") {
            if(!keys(body,{"invite","expiresIn"}) || !body["invite"].isString() || token(body["invite"].toString()).isEmpty() || body["expiresIn"]!=600) {bad(); return;}
            invite=body["invite"].toString(); const auto code=invite;
            QTimer::singleShot(600000,owner,[this,code]{if(invite==code) {invite.clear(); emit owner->connectionChanged();}});
        } else if(p->op=="invite.join") {
            if(!keys(body,{"joinRequestId"}) || !body["joinRequestId"].isString() || !id(body["joinRequestId"].toString())) {bad(); return;}
            setState("awaiting_approval");
        } else if(p->op=="invite.approve") {
            if(!keys(body,{"pairId"}) || !body["pairId"].isString() || !id(body["pairId"].toString())) {bad(); return;}
            invite.clear(); joinRequest.clear(); joinPeer.clear();
        } else if(p->op=="session.command") {
            bool validRevision=false; const auto number=body["revision"].toString().toULongLong(&validRevision);
            if(!keys(body,{"eventId","epoch","revision"}) || !body["eventId"].isString() || !id(body["eventId"].toString())
                || !body["epoch"].isString() || !id(body["epoch"].toString()) || !body["revision"].isString()
                || !validRevision || number==0 || QString::number(number)!=body["revision"].toString()) {bad(); return;}
            emit owner->error(frame.requestId,"delivery_unknown");
            owner->requestSnapshot();
        }
        emit owner->connectionChanged(); emit owner->controlResult(frame.requestId,body);
    }
    void receive(const QByteArray& bytes) {
        const auto frame=decodeFrame(bytes); if(!frame) {fatal("protocol"); return;}
        if(frame->op=="result" || frame->op=="error") {result(*frame); return;}
        if(!authenticated || !frame->requestId.isEmpty()) {fatal("protocol"); return;}
        if(frame->op=="session.snapshot") snapshot(frame->body);
        else if(frame->op=="session.event") event(frame->body);
        else if(frame->op=="session.notice") {
            if(!keys(frame->body,{"text"}) || !frame->body["text"].isString() || frame->body["text"].toString().toUcs4().size()>512) {fatal("protocol"); return;}
            emit owner->noticeReceived(frame->body["text"].toString());
        } else if(frame->op=="join.requested") {
            if(!keys(frame->body,{"joinRequestId","peerId"}) || !frame->body["joinRequestId"].isString() || !frame->body["peerId"].isString()
                || !id(frame->body["joinRequestId"].toString()) || !id(frame->body["peerId"].toString())) {fatal("protocol"); return;}
            joinRequest=frame->body["joinRequestId"].toString(); joinPeer=frame->body["peerId"].toString(); emit owner->connectionChanged();
        } else fatal("protocol");
    }
};
SessionClient::SessionClient(CredentialStore* s,ClientOptions o,QObject* p):QObject(p),d(std::make_unique<Impl>(this,s,std::move(o))) {qRegisterMetaType<SessionState>();qRegisterMetaType<SessionEvent>();}
SessionClient::~SessionClient() {d->stop(false);}
bool SessionClient::connected() const {return d->authenticated;}
bool SessionClient::paired() const {return d->authenticated && d->haveSnapshot;}
QString SessionClient::status() const {return d->state;}
QString SessionClient::peerId() const {return d->credential.id;}
QString SessionClient::inviteCode() const {return d->invite;}
QString SessionClient::joinRequestId() const {return d->joinRequest;}
QString SessionClient::pendingPeerId() const {return d->joinPeer;}
QString SessionClient::serverUrl() const {return d->url.toString(QUrl::FullyEncoded);}
bool SessionClient::endpointAllowed(const QUrl& url) const {return safeUrl(url);}
void SessionClient::connectTo(const QUrl& url) {
    d->stop(); d->credential={}; d->persisted=false; d->revoked=false; d->epoch.clear(); d->oldEpochs.clear(); d->epochOrder.clear(); d->revision=0;
    d->invite.clear(); d->joinRequest.clear(); d->joinPeer.clear(); d->attempt=0;
    if(!safeUrl(url)) {d->fatal("unsafe_url"); return;}
    if(!d->store || !d->store->setEndpoint(url)) {d->fatal("secret_store"); return;}
    d->url=url; d->wanted=true; d->loading=true; d->setState("loading_credentials"); d->store->load();
}
void SessionClient::send(const Command& command) {
    if(!connected() || !paired() || d->recovering || !isValidCommand(command)) {emit error(command.requestId,!isValidCommand(command)?"invalid":"offline"); return;}
    d->submit("session.command",object(encodeCommand(command)),command.requestId,command);
}
QString SessionClient::requestControl(const QString& operation,const QJsonObject& body) {
    if(!connected() || !controlBody(operation,body)) {emit error({},connected()?"invalid":"offline"); return {};}
    const auto request=uuid(); return d->submit(operation,body,request)?request:QString();
}
void SessionClient::requestSnapshot() {
    if(!connected() || d->recovering) return;
    d->recovering=true; if(requestControl("session.snapshot").isEmpty()) d->recovering=false;
}
void SessionClient::disconnectAndForget() {
    if(d->revoked && !d->clearing) {d->clearCredentials("offline"); return;}
    if(!connected() || d->forgetting) {emit error({},"offline"); return;}
    d->wanted=false; d->retry.stop(); d->forgetting=true; d->setState("forgetting"); if(!d->submit("pair.revoke",{},uuid())) d->forgetting=false;
}
void SessionClient::stop() {d->stop();}
