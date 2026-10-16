#pragma once
#include "CredentialStore.h"
#include <QJsonObject>
#include <memory>

Q_DECLARE_METATYPE(SessionState)
Q_DECLARE_METATYPE(SessionEvent)
struct ClientOptions {
    std::function<int(int)> reconnectDelayMs;
    int requestTimeoutMs=5000;
    int heartbeatMs=10000;
    int heartbeatTimeoutMs=30000;
};
class SessionClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(bool paired READ paired NOTIFY connectionChanged)
    Q_PROPERTY(QString status READ status NOTIFY connectionChanged)
    Q_PROPERTY(QString peerId READ peerId NOTIFY connectionChanged)
    Q_PROPERTY(QString inviteCode READ inviteCode NOTIFY connectionChanged)
    Q_PROPERTY(QString joinRequestId READ joinRequestId NOTIFY connectionChanged)
    Q_PROPERTY(QString pendingPeerId READ pendingPeerId NOTIFY connectionChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY connectionChanged)
public:
    explicit SessionClient(CredentialStore*,ClientOptions options={},QObject* parent=nullptr);
    ~SessionClient() override;
    bool connected() const;
    bool paired() const;
    QString status() const;
    QString peerId() const;
    QString inviteCode() const;
    QString joinRequestId() const;
    QString pendingPeerId() const;
    QString serverUrl() const;
    Q_INVOKABLE bool endpointAllowed(const QUrl&) const;
    Q_INVOKABLE void connectTo(const QUrl&);
    void send(const Command&);
    Q_INVOKABLE QString requestControl(const QString&,const QJsonObject& body={});
    Q_INVOKABLE void requestSnapshot();
    Q_INVOKABLE void disconnectAndForget();
    Q_INVOKABLE void stop();
signals:
    void snapshotReceived(SessionState);
    void eventReceived(SessionEvent);
    void noticeReceived(QString);
    void connectionChanged();
    void controlResult(QString requestId,QJsonObject body);
    void error(QString requestId,QString code);
    void reconnectScheduled(int milliseconds);
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
