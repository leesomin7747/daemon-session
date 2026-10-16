#pragma once
#include "Protocol.h"
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

Q_DECLARE_METATYPE(Action)
class SceneModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList peers READ peers NOTIFY peersChanged)
    Q_PROPERTY(QVariantMap peerStates READ peerStates NOTIFY peersChanged)
    Q_PROPERTY(QVariantMap lastEvent READ lastEvent NOTIFY lastEventChanged)
    Q_PROPERTY(QString localPeerId READ localPeerId WRITE setLocalPeerId NOTIFY localPeerIdChanged)
    Q_PROPERTY(bool showLocalOffline READ showLocalOffline WRITE setShowLocalOffline NOTIFY showLocalOfflineChanged)
public:
    explicit SceneModel(QObject* parent=nullptr);
    QVariantList peers() const;
    QVariantMap peerStates() const;
    QVariantMap lastEvent() const;
    QString localPeerId() const;
    void setLocalPeerId(const QString&);
    void replaceSnapshot(const SessionState&);
    void applyEvent(const SessionEvent&);
    Q_INVOKABLE QVariantMap peer(const QString&) const;
    void setTransportConnected(bool);
    bool showLocalOffline() const;
    void setShowLocalOffline(bool);
    void resetSession();
    Q_INVOKABLE QString newRequestId() const;
    Q_INVOKABLE void requestAction(const QString& action, const QString& target,
                                   const QString& text={}, bool busy=false,const QString& requestId={});
    Q_INVOKABLE int textLength(const QString& text) const;
    Q_INVOKABLE QString normalizedText(const QString& text) const;
    Q_INVOKABLE bool textValid(const QString& action, const QString& text) const;
signals:
    void peersChanged();
    void lastEventChanged();
    void localPeerIdChanged();
    void showLocalOfflineChanged();
    void sceneReset();
    void eventAboutToApply();
    void snapshotRequired();
    void actionRequested(Action, PeerId, QString text, bool busy,QString requestId);
    void actionRejected(QString code);
    void requestFailed(QString requestId,QString code);
private:
    SessionState m_state;
    QVariantMap m_lastEvent;
    QString m_localPeerId;
    bool m_transportConnected=true,m_showLocalOffline=true;
};
