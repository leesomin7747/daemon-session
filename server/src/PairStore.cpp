#include "PairStore.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <array>
#include <cstring>
#include <stdexcept>

namespace {
[[noreturn]] void storageFailure()
{
    // Never include SQL parameters, credentials, or invite codes in errors or logs.
    throw std::runtime_error("Pair store storage failure");
}

QSqlQuery query(const QSqlDatabase& db, const QString& sql, const QVariantList& values = {})
{
    QSqlQuery q(db);
    if (!q.prepare(sql)) storageFailure();
    for (const auto& value : values) q.addBindValue(value);
    if (!q.exec()) storageFailure();
    return q;
}

bool nextRow(QSqlQuery& q)
{
    const bool found = q.next();
    if (!found && q.lastError().isValid()) storageFailure();
    return found;
}

class Transaction {
public:
    explicit Transaction(QSqlDatabase& db) : db(db) { query(db, "BEGIN IMMEDIATE"); }
    ~Transaction() { if (!committed) db.rollback(); }
    void commit()
    {
        if (!db.commit()) storageFailure();
        committed = true;
    }
private:
    QSqlDatabase& db;
    bool committed = false;
};

QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QByteArray secret()
{
    std::array<quint32, 8> words;
    QRandomGenerator::system()->fillRange(words.data(), words.size());
    QByteArray result(32, Qt::Uninitialized);
    std::memcpy(result.data(), words.data(), 32);
    return result;
}
QByteArray hash(const QByteArray& value)
{
    return QCryptographicHash::hash(value, QCryptographicHash::Sha256);
}
bool boundedId(const QString& id) { return !id.isEmpty() && id.size() <= 128 && id == id.trimmed(); }
bool deviceExists(const QSqlDatabase& db, const PeerId& id)
{
    if (!boundedId(id)) return false;
    auto q = query(db, "SELECT 1 FROM devices WHERE id=?", {id});
    return nextRow(q);
}
void pruneExpired(const QSqlDatabase& db, qint64 now)
{
    query(db, "DELETE FROM invites WHERE expires_at<=?", {now});
}

std::optional<QByteArray> inviteHash(const QString& code)
{
    if (code.size() != 43) return {};
    const auto raw = QByteArray::fromBase64(code.toLatin1(),
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (raw.size() != 32 || QString::fromLatin1(raw.toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)) != code) return {};
    return hash(raw);
}
}

PairStore::PairStore(const QString& dbPath) : m_connectionName("pair-store-" + newId())
{
    try {
        if (dbPath.trimmed().isEmpty()) storageFailure();
        if (dbPath != ":memory:") {
            QFile file(dbPath);
            if (!file.exists()) {
                if (file.open(QIODevice::WriteOnly | QIODevice::NewOnly,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner)) file.close();
                else if (!QFileInfo(dbPath).isFile()) storageFailure();
            }
            if (!QFile::setPermissions(dbPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner))
                storageFailure();
        }
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
        m_db.setDatabaseName(dbPath);
        m_db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
        if (!m_db.open()) storageFailure();
        query(m_db, "PRAGMA foreign_keys=ON");
        {
            auto q = query(m_db, "PRAGMA foreign_keys");
            if (!nextRow(q) || q.value(0).toInt() != 1) storageFailure();
        }
        Transaction transaction(m_db);
        int version = -1;
        {
            auto q = query(m_db, "PRAGMA user_version");
            if (nextRow(q)) version = q.value(0).toInt();
        }
        if (version == 0) {
            auto existing = query(m_db, "SELECT count(*) FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'");
            if (!nextRow(existing) || existing.value(0).toInt() != 0) storageFailure();
            existing.finish();
            query(m_db, "CREATE TABLE devices (id TEXT PRIMARY KEY NOT NULL, "
                        "token_hash BLOB UNIQUE NOT NULL CHECK(length(token_hash)=32))");
            query(m_db, "CREATE TABLE pairs (id TEXT PRIMARY KEY NOT NULL)");
            query(m_db, "CREATE TABLE members (peer_id TEXT PRIMARY KEY NOT NULL REFERENCES devices(id), "
                        "pair_id TEXT NOT NULL REFERENCES pairs(id) ON DELETE CASCADE, "
                        "slot INTEGER NOT NULL CHECK(slot IN (0,1)), UNIQUE(pair_id,slot))");
            query(m_db, "CREATE TABLE invites (code_hash BLOB PRIMARY KEY NOT NULL CHECK(length(code_hash)=32), "
                        "inviter_id TEXT UNIQUE NOT NULL REFERENCES devices(id) ON DELETE CASCADE, "
                        "created_at INTEGER NOT NULL, expires_at INTEGER NOT NULL CHECK(expires_at>created_at), "
                        "request_id TEXT UNIQUE, applicant_id TEXT UNIQUE REFERENCES devices(id) ON DELETE CASCADE, "
                        "CHECK((request_id IS NULL AND applicant_id IS NULL) OR "
                        "(request_id IS NOT NULL AND applicant_id IS NOT NULL)))");
            query(m_db, "PRAGMA user_version=1");
        } else if (version != 1) {
            storageFailure();
        }
        // Validate required columns on reopening; a broken store cannot start accepting auth.
        query(m_db, "SELECT id,token_hash FROM devices LIMIT 0");
        query(m_db, "SELECT id FROM pairs LIMIT 0");
        query(m_db, "SELECT peer_id,pair_id,slot FROM members LIMIT 0");
        query(m_db, "SELECT code_hash,inviter_id,created_at,expires_at,request_id,applicant_id FROM invites LIMIT 0");
        transaction.commit();
    } catch (...) {
        closeDatabase();
        throw;
    }
}

PairStore::~PairStore() { closeDatabase(); }
void PairStore::closeDatabase() noexcept
{
    if (m_db.isValid()) m_db.close();
    m_db = {};
    if (QSqlDatabase::contains(m_connectionName)) QSqlDatabase::removeDatabase(m_connectionName);
}

DeviceCredential PairStore::createDevice()
{
    DeviceCredential credential{newId(), secret()};
    Transaction transaction(m_db);
    query(m_db, "INSERT INTO devices(id,token_hash) VALUES(?,?)", {credential.id, hash(credential.token)});
    transaction.commit();
    return credential;
}

std::optional<PeerId> PairStore::authenticate(const QByteArray& token)
{
    if (token.size() != 32) return {};
    auto q = query(m_db, "SELECT id FROM devices WHERE token_hash=?", {hash(token)});
    if (!nextRow(q)) return {};
    return q.value(0).toString();
}

std::optional<PairId> PairStore::pairForPeer(const PeerId& peer)
{
    if (!boundedId(peer)) return {};
    auto q = query(m_db, "SELECT pair_id FROM members WHERE peer_id=?", {peer});
    if (!nextRow(q)) return {};
    return q.value(0).toString();
}

QVector<PeerId> PairStore::members(const PairId& pair)
{
    if (!boundedId(pair)) return {};
    auto q = query(m_db, "SELECT peer_id FROM members WHERE pair_id=? ORDER BY slot", {pair});
    QVector<PeerId> result;
    while (nextRow(q)) result.push_back(q.value(0).toString());
    return result;
}

QString PairStore::createInvite(const PeerId& inviter, QDateTime now)
{
    const auto expiry = now.addSecs(600);
    if (!now.isValid() || !expiry.isValid() || expiry <= now || !boundedId(inviter)) return {};
    Transaction transaction(m_db);
    pruneExpired(m_db, now.toMSecsSinceEpoch());
    if (!deviceExists(m_db, inviter) || pairForPeer(inviter)) return {};
    query(m_db, "DELETE FROM invites WHERE inviter_id=?", {inviter});
    const auto nonce = secret();
    query(m_db, "INSERT INTO invites(code_hash,inviter_id,created_at,expires_at) VALUES(?,?,?,?)",
          {hash(nonce), inviter, now.toMSecsSinceEpoch(), expiry.toMSecsSinceEpoch()});
    transaction.commit();
    return QString::fromLatin1(nonce.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString PairStore::requestJoin(const QString& invite, const PeerId& applicant, QDateTime now)
{
    const auto codeHash = inviteHash(invite);
    if (!codeHash || !now.isValid() || !boundedId(applicant)) return {};
    Transaction transaction(m_db);
    pruneExpired(m_db, now.toMSecsSinceEpoch());
    if (!deviceExists(m_db, applicant) || pairForPeer(applicant)) return {};
    auto q = query(m_db, "SELECT inviter_id,created_at,expires_at,request_id FROM invites WHERE code_hash=?", {*codeHash});
    if (!nextRow(q) || !q.value(3).isNull()) return {};
    const auto inviter = q.value(0).toString();
    if (inviter == applicant || now.toMSecsSinceEpoch() < q.value(1).toLongLong()
        || now.toMSecsSinceEpoch() >= q.value(2).toLongLong()) return {};
    q.finish();
    if (!deviceExists(m_db, inviter) || pairForPeer(inviter)) return {};
    {
        auto pending = query(m_db, "SELECT 1 FROM invites WHERE applicant_id=?", {applicant});
        if (nextRow(pending)) return {};
    }
    const auto request = newId();
    auto claimed = query(m_db, "UPDATE invites SET request_id=?,applicant_id=? "
                               "WHERE code_hash=? AND request_id IS NULL", {request, applicant, *codeHash});
    if (claimed.numRowsAffected() != 1) return {};
    claimed.finish();
    transaction.commit();
    return request;
}

std::optional<PeerId> PairStore::inviterForRequest(const QString& requestId, QDateTime now)
{
    if (!boundedId(requestId) || !now.isValid()) return {};
    auto q = query(m_db, "SELECT inviter_id FROM invites WHERE request_id=? AND applicant_id IS NOT NULL "
                        "AND created_at<=? AND expires_at>?",
                   {requestId, now.toMSecsSinceEpoch(), now.toMSecsSinceEpoch()});
    if (!nextRow(q)) return {};
    return q.value(0).toString();
}

PairId PairStore::approveJoin(const PeerId& inviter, const QString& requestId, QDateTime now)
{
    if (!boundedId(inviter) || !boundedId(requestId) || !now.isValid()) return {};
    Transaction transaction(m_db);
    pruneExpired(m_db, now.toMSecsSinceEpoch());
    auto q = query(m_db, "SELECT applicant_id,created_at,expires_at FROM invites WHERE request_id=? AND inviter_id=?",
                   {requestId, inviter});
    if (!nextRow(q)) return {};
    const auto applicant = q.value(0).toString();
    if (!boundedId(applicant) || inviter == applicant || now.toMSecsSinceEpoch() < q.value(1).toLongLong()
        || now.toMSecsSinceEpoch() >= q.value(2).toLongLong()) return {};
    q.finish();
    if (!deviceExists(m_db, inviter) || !deviceExists(m_db, applicant)
        || pairForPeer(inviter) || pairForPeer(applicant)) return {};
    const auto pair = newId();
    query(m_db, "INSERT INTO pairs(id) VALUES(?)", {pair});
    query(m_db, "INSERT INTO members(peer_id,pair_id,slot) VALUES(?,?,0)", {inviter, pair});
    query(m_db, "INSERT INTO members(peer_id,pair_id,slot) VALUES(?,?,1)", {applicant, pair});
    query(m_db, "DELETE FROM invites WHERE inviter_id IN (?,?) OR applicant_id IN (?,?)",
          {inviter, applicant, inviter, applicant});
    transaction.commit();
    return pair;
}

void PairStore::revokePair(const PeerId& peer)
{
    if (!boundedId(peer)) return;
    Transaction transaction(m_db);
    const auto pair = pairForPeer(peer);
    if (pair) {
        query(m_db, "DELETE FROM invites WHERE inviter_id IN (SELECT peer_id FROM members WHERE pair_id=?) "
                    "OR applicant_id IN (SELECT peer_id FROM members WHERE pair_id=?)", {*pair, *pair});
        query(m_db, "DELETE FROM pairs WHERE id=?", {*pair});
    } else {
        query(m_db, "DELETE FROM invites WHERE inviter_id=? OR applicant_id=?", {peer, peer});
    }
    transaction.commit();
}
