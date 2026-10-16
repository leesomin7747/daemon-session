#pragma once
#include "Protocol.h"
#include <QDateTime>
#include <QSqlDatabase>

// Callers must derive peer IDs from authenticate(), never from untrusted actor fields.
// One instance belongs to the thread that created its Qt SQL connection.
class PairStore {
public:
    explicit PairStore(const QString& dbPath);
    ~PairStore();
    PairStore(const PairStore&) = delete;
    PairStore& operator=(const PairStore&) = delete;

    DeviceCredential createDevice();
    QString createInvite(const PeerId& inviter, QDateTime now);
    QString requestJoin(const QString& invite, const PeerId& applicant, QDateTime now);
    PairId approveJoin(const PeerId& inviter, const QString& requestId,
                       QDateTime now = QDateTime::currentDateTimeUtc());
    std::optional<PeerId> authenticate(const QByteArray& token);
    std::optional<PairId> pairForPeer(const PeerId& peer);
    QVector<PeerId> members(const PairId& pair);
    std::optional<PeerId> inviterForRequest(const QString& requestId, QDateTime now);
    void revokePair(const PeerId& peer);

private:
    QString m_connectionName;
    QSqlDatabase m_db;
    void closeDatabase() noexcept;
};
