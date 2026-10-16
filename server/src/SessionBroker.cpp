#include "SessionBroker.h"
#include "Session.h"
#include "WireProtocol.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QTimer>
#include <QUuid>
#include <QWebSocket>
#include <QWebSocketCorsAuthenticator>
#include <QWebSocketServer>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace {
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QJsonObject object(const QByteArray& bytes) { return QJsonDocument::fromJson(bytes).object(); }
QByteArray json(const QJsonObject& o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
bool keys(const QJsonObject& o, std::initializer_list<const char*> fields)
{
    if (o.size() != qsizetype(fields.size())) return false;
    for (auto key : fields) if (!o.contains(QLatin1String(key))) return false;
    return true;
}
QByteArray token(const QJsonValue& value)
{
    if (!value.isString()) return {};
    const auto encoded=value.toString().toLatin1();
    const auto raw=QByteArray::fromBase64(encoded,QByteArray::Base64UrlEncoding
        | QByteArray::AbortOnBase64DecodingErrors);
    return raw.size()==32 && raw.toBase64(QByteArray::Base64UrlEncoding
        | QByteArray::OmitTrailingEquals)==encoded ? raw : QByteArray();
}
struct Cached { QByteArray digest, reply; };
struct Cache {
    QMap<QString,Cached> entries;
    QStringList order;
    void insert(const QString& key, Cached value) {
        entries.insert(key,std::move(value)); order.append(key);
        if (order.size()>256) entries.remove(order.takeFirst());
    }
};
struct Bucket {
    double tokens=20;
    double capacity=20, perSecond=10;
    qint64 at=0;
    bool take(qint64 now) {
        tokens=std::min(capacity,tokens+std::max(qint64(0),now-at)*perSecond/1000.0);
        at=std::max(now,at);
        if (tokens<1) return false;
        --tokens; return true;
    }
};
struct IpBudget { qint64 start=0; int used=0; };
struct Connection {
    PeerId peer;
    QString ip;
    qint64 created=0, lastPong=0, lastPing=0;
    QByteArray ping;
    Bucket frames;
    Cache controls;
    bool registered=false;
};
struct LiveSession {
    SessionState state;
    CopyCatalog copy;
    Cache commands;
    QMap<PeerId,qint64> departures;
};
}

struct SessionBroker::Impl {
    SessionBroker* owner;
    PairStore& store;
    BrokerClock clock;
    QWebSocketServer server{"daemon-session",QWebSocketServer::NonSecureMode};
    QTimer timer;
    QMap<QWebSocket*,Connection> connections;
    QMap<PeerId,QWebSocket*> active;
    QMap<PairId,LiveSession> sessions;
    QMap<PeerId,Bucket> deviceRates;
    QMap<QString,IpBudget> ipRates;
    bool shuttingDown=false;

    Impl(SessionBroker* owner, PairStore& store, BrokerClock clock)
        : owner(owner), store(store), clock(std::move(clock))
    {
        if (!this->clock.monotonicMs) this->clock.monotonicMs=[] {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        };
        if (!this->clock.utcNow) this->clock.utcNow=QDateTime::currentDateTimeUtc;
        server.setMaxPendingConnections(16);
        server.setHandshakeTimeout(5000);
        QObject::connect(&server,&QWebSocketServer::originAuthenticationRequired,owner,
            [](QWebSocketCorsAuthenticator* auth) { auth->setAllowed(auth->origin().isEmpty()); });
        QObject::connect(&server,&QWebSocketServer::newConnection,owner,[this] { accept(); });
        timer.setInterval(100);
        QObject::connect(&timer,&QTimer::timeout,owner,[this] { sweep(); });
    }
    qint64 now() const { return clock.monotonicMs(); }
    int choose(int size) { return QRandomGenerator::global()->bounded(size); }
    void drop(QWebSocket* socket)
    {
        const auto it=connections.find(socket);
        if (it==connections.end()) return;
        const auto peer=it->peer;
        connections.erase(it);
        if (!peer.isEmpty() && active.value(peer)==socket) {
            active.remove(peer);
            if (!shuttingDown) for (auto s=sessions.begin();s!=sessions.end();++s) {
                if (!s->state.peers.contains(peer)) continue;
                s->state=applyPresence(s->state,peer,false);
                s->departures[peer]=now()+15000;
                broadcastSnapshot(s.key()); break;
            }
        }
    }
    void abort(QWebSocket* socket)
    {
        drop(socket); // revoke the grant before reentrant signals or buffered commands
        socket->abort(); socket->deleteLater();
    }
    void retire(QWebSocket* socket, const QString& reason)
    {
        drop(socket);
        // Flush the result before closing; the identity grant is already gone.
        socket->close(QWebSocketProtocol::CloseCodeNormal,reason);
        QTimer::singleShot(1000,socket,[socket] { socket->abort(); socket->deleteLater(); });
    }
    bool writable(QWebSocket* socket, qsizetype bytes)
    {
        return connections.contains(socket) && socket->state()==QAbstractSocket::ConnectedState
            && socket->bytesToWrite()+bytes+16<=64*1024;
    }
    void send(QWebSocket* socket, const QByteArray& bytes)
    {
        if (!connections.contains(socket)) return;
        if (bytes.isEmpty() || !writable(socket,bytes.size())) { abort(socket); return; }
        if (socket->sendTextMessage(QString::fromUtf8(bytes))<0) abort(socket);
    }
    void send(QWebSocket* socket, const WireFrame& frame) { send(socket,encodeFrame(frame)); }
    QByteArray reply(const WireFrame& f, const QString& error={}, const QJsonObject& body={})
    {
        return encodeFrame({error.isEmpty()?"result":"error",f.requestId,
            error.isEmpty()?body:QJsonObject{{"code",error}}});
    }
    QVector<QWebSocket*> recipients(const PairId& pair) const
    {
        QVector<QWebSocket*> result;
        const auto it=sessions.constFind(pair);
        if (it==sessions.cend()) return result;
        for (const auto& peer : it->state.peers)
            if (active.contains(peer.id)) result.append(active.value(peer.id));
        return result;
    }
    void broadcast(const PairId& pair, const QByteArray& bytes)
    {
        const auto sockets=recipients(pair); // send() can synchronously disconnect a slow peer
        for (auto socket : sockets) send(socket,bytes);
    }
    void broadcastSnapshot(const PairId& pair)
    {
        if (sessions.contains(pair)) broadcast(pair,encodeFrame({"session.snapshot",{},
            object(encodeSnapshot(sessions[pair].state))}));
    }
    void notice(const PairId& pair, const QString& key)
    {
        if (!sessions.contains(pair)) return;
        const auto text=sessions[pair].copy.pick(key,[this](int n){return choose(n);});
        broadcast(pair,encodeFrame({"session.notice",{},{{"text",text}}}));
    }
    bool ipPermit(const QString& ip)
    {
        const auto t=now();
        if (!ipRates.contains(ip)) {
            if (ipRates.size()>=1024) return false;
            ipRates.insert(ip,{t,0});
        }
        auto& b=ipRates[ip];
        if (t-b.start>=60000) b={t,0};
        if (b.used>=10) return false;
        ++b.used; return true;
    }
    void accept()
    {
        while (server.hasPendingConnections()) {
            auto socket=server.nextPendingConnection(); socket->setParent(owner);
            const auto url=socket->requestUrl();
            if (connections.size()>=64 || !socket->peerAddress().isLoopback()
                || !url.query().isEmpty() || !url.userInfo().isEmpty() || url.path()!="/") {
                socket->abort(); socket->deleteLater(); continue;
            }
            socket->setMaxAllowedIncomingFrameSize(MaxFrameBytes);
            socket->setMaxAllowedIncomingMessageSize(MaxFrameBytes);
            socket->setReadBufferSize(MaxFrameBytes+1024);
            Connection c;
            c.ip=socket->peerAddress().toString();
            c.created=c.lastPong=c.lastPing=now(); c.frames.at=now();
            c.frames.tokens=c.frames.capacity=40; c.frames.perSecond=20;
            connections.insert(socket,c);
            QObject::connect(socket,&QWebSocket::textMessageReceived,owner,
                [this,socket](const QString& data) { receive(socket,data.toUtf8()); });
            QObject::connect(socket,&QWebSocket::binaryMessageReceived,owner,
                [this,socket](const QByteArray&) { abort(socket); });
            QObject::connect(socket,&QWebSocket::pong,owner,
                [this,socket](quint64,const QByteArray& payload) {
                    auto it=connections.find(socket);
                    if (it!=connections.end() && !it->ping.isEmpty() && it->ping==payload) {
                        it->lastPong=now(); it->ping.clear();
                    }
                });
            QObject::connect(socket,&QWebSocket::disconnected,owner,[this,socket] {
                drop(socket); socket->deleteLater();
            });
        }
    }
    void attach(const PeerId& peer)
    {
        const auto pending=store.pendingJoinForInviter(peer,clock.utcNow());
        if (pending && active.contains(peer)) send(active[peer],WireFrame{"join.requested",{},
            {{"joinRequestId",pending->requestId},{"peerId",pending->applicantId}}});
        const auto pair=store.pairForPeer(peer);
        if (!pair) return;
        if (!sessions.contains(*pair)) {
            LiveSession s; s.state.epoch=uuid();
            const auto members=store.members(*pair);
            if (members.size()!=2) throw std::runtime_error("Invalid membership");
            for (int i=0;i<2;++i) {
                PeerState p; p.id=members[i]; p.x=i==0?0.25:0.75;
                s.state.peers.insert(p.id,p);
            }
            sessions.insert(*pair,s);
        }
        auto& s=sessions[*pair]; bool changed=false;
        for (const auto& id : s.state.peers.keys()) {
            if (!active.contains(id) || s.state.peers[id].reachable) continue;
            s.state=applyPresence(s.state,id,true); s.departures.remove(id); changed=true;
        }
        broadcastSnapshot(*pair);
        if (changed && std::all_of(s.state.peers.cbegin(),s.state.peers.cend(),
            [](const PeerState& p){return p.reachable;})) notice(*pair,"caption.join");
    }
    bool replay(QWebSocket* socket, const WireFrame& f, const QByteArray& digest,
                const Cache& cache, const QString& key)
    {
        const auto it=cache.entries.constFind(key);
        if (it==cache.entries.cend()) return false;
        const auto bytes=it->digest==digest?it->reply:reply(f,"request_conflict");
        send(socket,bytes); return true;
    }
    void command(QWebSocket* socket, const WireFrame& f, const QByteArray& digest)
    {
        const auto actor=connections[socket].peer;
        const auto pair=store.pairForPeer(actor);
        if (!pair || !sessions.contains(*pair)) { send(socket,reply(f,"not_paired")); return; }
        auto& s=sessions[*pair];
        const auto key=actor+":"+f.requestId; // generated UUID actor; cache belongs to one epoch
        if (replay(socket,f,digest,s.commands,key)) return;
        const auto cmd=decodeCommand(json(f.body));
        QString error; std::optional<SessionEvent> event;
        if (!deviceRates[actor].take(now())) error="rate_limited";
        else if (!cmd || cmd->requestId!=f.requestId) error="invalid";
        else {
            const auto sockets=recipients(*pair);
            for (auto target : sockets) if (!writable(target,MaxFrameBytes)) abort(target);
            if (!connections.contains(socket)) return;
            const bool add=cmd->action==Action::Note || ((cmd->action==Action::Coffee
                || cmd->action==Action::Energy) && s.state.peers.value(cmd->targetId).busy);
            if (add && s.state.peers.value(cmd->targetId).inbox.size()>=100) error="inbox_full";
            else {
                auto result=applyCommand(s.state,actor,*cmd,s.copy,[this](int n){return choose(n);});
                s.state=result.state; event=result.event; error=result.error;
            }
        }
        QJsonObject body;
        if (event) body={{"eventId",event->eventId},{"epoch",event->epoch},
                        {"revision",QString::number(event->revision)}};
        const auto bytes=reply(f,error,body);
        s.commands.insert(key,{digest,bytes});
        if (event) broadcast(*pair,encodeFrame({"session.event",{},object(encodeEvent(*event))}));
        send(socket,bytes);
    }
    void control(QWebSocket* socket, const WireFrame& f, const QByteArray& digest)
    {
        const auto actor=connections[socket].peer;
        QString error; QJsonObject body;
        bool attachAfter=false, revokeAfter=false;
        QVector<PeerId> revokedMembers;
        const auto& b=f.body;
        if (f.op=="device.register") {
            if (!keys(b,{})) error="invalid";
            else if (!actor.isEmpty() || connections[socket].registered) error="forbidden";
            else if (!ipPermit(connections[socket].ip)) error="rate_limited";
            else {
                const auto device=store.createDevice(); connections[socket].registered=true;
                body={{"peerId",device.id},{"token",QString::fromLatin1(device.token.toBase64(
                    QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals))}};
            }
        } else if (f.op=="auth") {
            if (!actor.isEmpty() || !keys(b,{"token"})) { abort(socket); return; }
            const auto credential=token(b["token"]);
            const auto peer=credential.isEmpty()?std::nullopt:store.authenticate(credential);
            if (!peer) { abort(socket); return; }
            if (!deviceRates.contains(*peer)) {
                if (deviceRates.size()>=4096) { abort(socket); return; }
                Bucket bucket; bucket.at=now(); deviceRates.insert(*peer,bucket);
            }
            if (active.contains(*peer)) abort(active.value(*peer));
            connections[socket].peer=*peer;
            connections[socket].lastPong=connections[socket].lastPing=now();
            active[*peer]=socket; body={{"peerId",*peer}}; attachAfter=true;
        } else if (actor.isEmpty()) error="unauthenticated";
        else if (f.op=="invite.create") {
            if (!keys(b,{})) error="invalid";
            else if (!ipPermit(connections[socket].ip)) error="rate_limited";
            else {
                const auto code=store.createInvite(actor,clock.utcNow());
                if (code.isEmpty()) error="refused"; else body={{"invite",code},{"expiresIn",600}};
            }
        } else if (f.op=="invite.join") {
            if (!keys(b,{"invite"}) || !b["invite"].isString()) error="invalid";
            else {
                const auto request=store.requestJoin(b["invite"].toString(),actor,clock.utcNow());
                if (request.isEmpty()) error="refused";
                else {
                    body={{"joinRequestId",request}};
                    const auto inviter=store.inviterForRequest(request,clock.utcNow());
                    if (inviter && active.contains(*inviter)) send(active[*inviter],WireFrame{
                        "join.requested",{},{{"joinRequestId",request},{"peerId",actor}}});
                }
            }
        } else if (f.op=="invite.approve") {
            if (!keys(b,{"joinRequestId"}) || !b["joinRequestId"].isString()) error="invalid";
            else {
                const auto pair=store.approveJoin(actor,b["joinRequestId"].toString(),clock.utcNow());
                if (pair.isEmpty()) error="refused";
                else { body={{"pairId",pair}}; attachAfter=true; }
            }
        } else if (f.op=="pair.revoke") {
            if (!keys(b,{})) error="invalid";
            else {
                const auto pair=store.pairForPeer(actor);
                revokedMembers=pair?store.members(*pair):QVector<PeerId>{actor};
                store.revokePair(actor);
                if (pair) sessions.remove(*pair);
                revokeAfter=true;
            }
        } else if (f.op=="session.snapshot") {
            if (!keys(b,{})) error="invalid";
            else {
                const auto pair=store.pairForPeer(actor);
                if (!pair || !sessions.contains(*pair)) error="not_paired";
                else body=object(encodeSnapshot(sessions[*pair].state));
            }
        } else error="invalid";
        const auto bytes=reply(f,error,body);
        if (connections.contains(socket)) connections[socket].controls.insert(f.requestId,{digest,bytes});
        send(socket,bytes);
        if (attachAfter && connections.contains(socket)) attach(connections[socket].peer);
        if (revokeAfter) for (const auto& peer : revokedMembers)
            if (active.contains(peer)) retire(active.value(peer),"pair_revoked");
    }
    void receive(QWebSocket* socket, const QByteArray& bytes)
    {
        if (!connections.contains(socket) || shuttingDown) return;
        const auto& c=connections[socket];
        if ((c.peer.isEmpty() && now()-c.created>=5000)
            || (!c.peer.isEmpty() && now()-c.lastPong>=30000)) {
            abort(socket); return;
        }
        const auto f=decodeFrame(bytes);
        if (!f || f->requestId.isEmpty() || f->op=="result" || f->op=="error"
            || f->op=="session.notice" || f->op=="session.event" || f->op=="join.requested") {
            abort(socket); return;
        }
        if (!connections[socket].frames.take(now())) {
            abort(socket); return;
        }
        const auto digest=QCryptographicHash::hash(encodeFrame(*f),QCryptographicHash::Sha256);
        try {
            if (f->op=="session.command" && !connections[socket].peer.isEmpty()) command(socket,*f,digest);
            else if (!replay(socket,*f,digest,connections[socket].controls,f->requestId)) {
                const auto peer=connections[socket].peer;
                if (!peer.isEmpty() && !deviceRates[peer].take(now())) {
                    const auto bytes=reply(*f,"rate_limited");
                    connections[socket].controls.insert(f->requestId,{digest,bytes});
                    send(socket,bytes);
                }
                else control(socket,*f,digest);
            }
        } catch (const std::exception&) {
            send(socket,reply(*f,"storage_unavailable"));
            if (connections.contains(socket)) retire(socket,"storage_unavailable");
        }
    }
    void sweep()
    {
        const auto t=now();
        for (auto socket : connections.keys()) {
            if (!connections.contains(socket)) continue;
            auto& c=connections[socket];
            if ((c.peer.isEmpty() && t-c.created>=5000) || (!c.peer.isEmpty() && t-c.lastPong>=30000)) {
                abort(socket); continue;
            }
            if (!c.peer.isEmpty() && t-c.lastPing>=10000) {
                c.lastPing=t; c.ping=uuid().toLatin1(); socket->ping(c.ping);
            }
        }
        for (const auto& pair : sessions.keys()) {
            auto& s=sessions[pair];
            for (const auto& peer : s.departures.keys()) if (t>=s.departures[peer]) {
                s.departures.remove(peer); bool coffee=false;
                for (const auto& p : s.state.peers) for (const auto& item : p.inbox)
                    coffee |= item.kind==Action::Coffee;
                notice(pair,coffee?"caption.leaveCoffee":"caption.leave");
            }
            if (s.departures.isEmpty() && std::none_of(s.state.peers.cbegin(),s.state.peers.cend(),
                [](const PeerState& p){return p.reachable;})) sessions.remove(pair);
        }
        for (const auto& ip : ipRates.keys()) if (t-ipRates[ip].start>=60000) ipRates.remove(ip);
        for (const auto& peer : deviceRates.keys())
            if (!active.contains(peer) && t-deviceRates[peer].at>=60000) deviceRates.remove(peer);
    }
    void close()
    {
        shuttingDown=true; timer.stop(); server.close();
        for (auto socket : connections.keys()) abort(socket);
        sessions.clear(); active.clear(); deviceRates.clear(); ipRates.clear(); shuttingDown=false;
    }
};
SessionBroker::SessionBroker(PairStore& store, QObject* parent) : SessionBroker(store,{},parent) {}
SessionBroker::SessionBroker(PairStore& store, BrokerClock clock, QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>(this,store,std::move(clock))) {}
SessionBroker::~SessionBroker() { d->close(); }
bool SessionBroker::listen(const QHostAddress& host, quint16 port)
{
    if (!host.isLoopback() || d->server.isListening()) return false;
    if (!d->server.listen(host,port)) return false;
    d->timer.start(); return true;
}
quint16 SessionBroker::serverPort() const { return d->server.serverPort(); }
void SessionBroker::close() { d->close(); }
