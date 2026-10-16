#include "PairStore.h"
#include <QCryptographicHash>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <future>
#include <stdexcept>

namespace {
const QDateTime now = QDateTime::fromString("2026-10-16T12:00:00Z", Qt::ISODate);

struct Paired {
    DeviceCredential a;
    DeviceCredential b;
    QString invite;
    QString request;
    PairId pair;
};
Paired createPair(PairStore& store)
{
    Paired p;
    p.a = store.createDevice(); p.b = store.createDevice();
    p.invite = store.createInvite(p.a.id, now);
    p.request = store.requestJoin(p.invite, p.b.id, now);
    p.pair = store.approveJoin(p.a.id, p.request, now);
    return p;
}

// Independent SQL access is confined to tests for persisted-data and rollback assertions.
class InspectDb {
public:
    explicit InspectDb(const QString& path) : name(QUuid::createUuid().toString())
    {
        db = QSqlDatabase::addDatabase("QSQLITE", name); db.setDatabaseName(path);
        if (!db.open()) throw std::runtime_error("Test DB inspection failed");
    }
    ~InspectDb() { db.close(); db = {}; QSqlDatabase::removeDatabase(name); }
    void exec(const QString& sql, const QVariantList& values = {})
    {
        QSqlQuery q(db);
        if (!q.prepare(sql)) throw std::runtime_error("Test SQL failed");
        for (const auto& value : values) q.addBindValue(value);
        if (!q.exec()) throw std::runtime_error("Test SQL failed");
    }
    QVariant scalar(const QString& sql)
    {
        QSqlQuery q(db);
        if (!q.exec(sql) || !q.next()) throw std::runtime_error("Test SQL read failed");
        return q.value(0);
    }
private:
    QString name;
    QSqlDatabase db;
};
}

class PairingTest : public QObject {
    Q_OBJECT
private slots:
    void persistentAuthenticationAndPair()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto path = dir.filePath("pairs.sqlite"); Paired paired;
        {
            PairStore store(path); paired = createPair(store);
            QVERIFY(!paired.pair.isEmpty()); QCOMPARE(paired.a.token.size(), 32);
            QVERIFY(paired.a.token != paired.b.token);
        }
        PairStore reopened(path);
        QCOMPARE(reopened.authenticate(paired.a.token), std::optional<PeerId>(paired.a.id));
        QCOMPARE(reopened.authenticate(paired.b.token), std::optional<PeerId>(paired.b.id));
        QCOMPARE(reopened.pairForPeer(paired.a.id), std::optional<PairId>(paired.pair));
        QCOMPARE(reopened.pairForPeer(paired.b.id), std::optional<PairId>(paired.pair));
        QCOMPARE(reopened.members(paired.pair), QVector<PeerId>({paired.a.id, paired.b.id}));
        QVERIFY(!reopened.authenticate(QByteArray(32, 'x')));
        QVERIFY(!reopened.authenticate({}));
    }
    void approvalIsRequired()
    {
        PairStore store(":memory:"); auto a = store.createDevice(); auto b = store.createDevice();
        auto invite = store.createInvite(a.id, now); QVERIFY(!invite.isEmpty());
        auto request = store.requestJoin(invite, b.id, now); QVERIFY(!request.isEmpty());
        QVERIFY(!store.pairForPeer(a.id)); QVERIFY(!store.pairForPeer(b.id));
        auto pair = store.approveJoin(a.id, request, now); QVERIFY(!pair.isEmpty());
        QCOMPARE(store.members(pair).size(), 2);
    }
    void inviteExpiresAtTenMinutes()
    {
        PairStore store(":memory:"); auto a = store.createDevice(); auto b = store.createDevice();
        auto invite = store.createInvite(a.id, now); QVERIFY(!invite.isEmpty());
        QVERIFY(!store.requestJoin(invite, b.id, now.addSecs(599)).isEmpty());
        QVERIFY(store.approveJoin(a.id, "unknown", now).isEmpty());
        invite = store.createInvite(a.id, now); QVERIFY(!invite.isEmpty());
        QVERIFY(store.requestJoin(invite, b.id, now.addSecs(600)).isEmpty());
        QVERIFY(!store.pairForPeer(a.id));
    }
    void pendingApprovalAlsoExpires()
    {
        PairStore store(":memory:"); auto a = store.createDevice(); auto b = store.createDevice();
        auto invite = store.createInvite(a.id, now); QVERIFY(!invite.isEmpty());
        auto request = store.requestJoin(invite, b.id, now.addSecs(599)); QVERIFY(!request.isEmpty());
        QVERIFY(store.approveJoin(a.id, request, now.addSecs(600)).isEmpty());
        QVERIFY(!store.pairForPeer(a.id)); QVERIFY(!store.pairForPeer(b.id));
    }
    void consumedCodeCannotBeReused()
    {
        PairStore store(":memory:"); auto a = store.createDevice(); auto b = store.createDevice();
        auto c = store.createDevice(); auto invite = store.createInvite(a.id, now);
        auto request = store.requestJoin(invite, b.id, now); QVERIFY(!request.isEmpty());
        QVERIFY(store.requestJoin(invite, b.id, now).isEmpty());
        QVERIFY(store.requestJoin(invite, c.id, now).isEmpty());
        auto pair = store.approveJoin(a.id, request, now); QVERIFY(!pair.isEmpty());
        QVERIFY(store.requestJoin(invite, c.id, now).isEmpty());
        QVERIFY(store.approveJoin(a.id, request, now).isEmpty());
    }
    void thirdDeviceAndWrongApproverAreRejected()
    {
        PairStore store(":memory:"); auto a = store.createDevice(); auto b = store.createDevice();
        auto c = store.createDevice(); auto invite = store.createInvite(a.id, now);
        auto request = store.requestJoin(invite, b.id, now); QVERIFY(!request.isEmpty());
        QVERIFY(store.approveJoin(c.id, request, now).isEmpty());
        QVERIFY(store.approveJoin(b.id, request, now).isEmpty());
        auto pair = store.approveJoin(a.id, request, now); QVERIFY(!pair.isEmpty());
        QVERIFY(store.createInvite(a.id, now).isEmpty());
        auto cInvite = store.createInvite(c.id, now); QVERIFY(!cInvite.isEmpty());
        QVERIFY(store.requestJoin(cInvite, b.id, now).isEmpty());
        QCOMPARE(store.members(pair).size(), 2); QVERIFY(!store.pairForPeer(c.id));
    }
    void revokeRemovesPairPermissionsAndInvites()
    {
        PairStore store(":memory:"); auto p = createPair(store); QVERIFY(!p.pair.isEmpty());
        store.revokePair(p.a.id);
        QVERIFY(!store.pairForPeer(p.a.id)); QVERIFY(!store.pairForPeer(p.b.id));
        QVERIFY(store.members(p.pair).isEmpty());
        QVERIFY(store.requestJoin(p.invite, p.b.id, now).isEmpty());
        QCOMPARE(store.authenticate(p.a.token), std::optional<PeerId>(p.a.id));
        QCOMPARE(store.authenticate(p.b.token), std::optional<PeerId>(p.b.id));
        auto invite = store.createInvite(p.b.id, now); QVERIFY(!invite.isEmpty());
        auto request = store.requestJoin(invite, p.a.id, now); QVERIFY(!request.isEmpty());
        store.revokePair(p.a.id); // Cancel this unpaired device's pending request too.
        QVERIFY(store.approveJoin(p.b.id, request, now).isEmpty());
    }
    void databaseContainsOnlySecretHashes()
    {
        QTemporaryDir dir; const auto path = dir.filePath("secrets.sqlite");
        DeviceCredential device; QString invite;
        {
            PairStore store(path); device = store.createDevice();
            QCOMPARE(device.token.size(), 32);
            invite = store.createInvite(device.id, now); QVERIFY(!invite.isEmpty());
        }
        InspectDb db(path);
        const auto tokenHash = db.scalar("SELECT token_hash FROM devices").toByteArray();
        QCOMPARE(tokenHash, QCryptographicHash::hash(device.token, QCryptographicHash::Sha256));
        QCOMPARE(db.scalar("SELECT length(code_hash) FROM invites").toInt(), 32);
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto bytes = file.readAll();
        QVERIFY(!bytes.contains(device.token)); QVERIFY(!bytes.contains(device.token.toHex()));
        QVERIFY(!bytes.contains(device.token.toBase64())); QVERIFY(!bytes.contains(invite.toUtf8()));
        QVERIFY(!bytes.contains(QByteArray::fromBase64(invite.toLatin1(), QByteArray::Base64UrlEncoding)));
    }
    void replacingInviteCancelsPendingRequest()
    {
        PairStore store(":memory:"); auto a = store.createDevice(); auto b = store.createDevice();
        auto old = store.createInvite(a.id, now); auto request = store.requestJoin(old, b.id, now);
        QVERIFY(!request.isEmpty()); auto fresh = store.createInvite(a.id, now); QVERIFY(fresh != old);
        QVERIFY(store.approveJoin(a.id, request, now).isEmpty());
        QVERIFY(store.requestJoin(old, b.id, now).isEmpty());
        auto replacement = store.requestJoin(fresh, b.id, now); QVERIFY(!replacement.isEmpty());
        QVERIFY(!store.approveJoin(a.id, replacement, now).isEmpty());
    }
    void malformedAndSelfRequestsAreRejected()
    {
        PairStore store(":memory:"); auto a = store.createDevice(); auto b = store.createDevice();
        QCOMPARE(a.token.size(), 32);
        auto invite = store.createInvite(a.id, now); QVERIFY(!invite.isEmpty());
        QVERIFY(store.requestJoin(invite, a.id, now).isEmpty());
        QVERIFY(store.requestJoin(invite, "unknown", now).isEmpty());
        QVERIFY(store.requestJoin(invite + "=", b.id, now).isEmpty());
        const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        auto alias = invite;
        alias[alias.size() - 1] = alphabet[alphabet.indexOf(invite.back()) + 1];
        QVERIFY(store.requestJoin(alias, b.id, now).isEmpty());
        QVERIFY(store.requestJoin(invite, b.id, now.addSecs(-1)).isEmpty());
        QVERIFY(store.createInvite("x'); DELETE FROM devices; --", now).isEmpty());
        QVERIFY(store.createInvite(a.id, QDateTime()).isEmpty());
        QCOMPARE(store.authenticate(a.token), std::optional<PeerId>(a.id));
        QVERIFY(!store.requestJoin(invite, b.id, now).isEmpty());
    }
    void storageFailuresNeverFallBackToMemory()
    {
        QTemporaryDir dir;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, PairStore(dir.filePath("missing-parent/auth.sqlite")));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, PairStore{QString()});
    }
    void approvalFailureRollsBackAllWrites()
    {
        QTemporaryDir dir; const auto path = dir.filePath("rollback.sqlite"); PairStore store(path);
        auto a = store.createDevice(); auto b = store.createDevice();
        auto invite = store.createInvite(a.id, now); auto request = store.requestJoin(invite, b.id, now);
        QVERIFY(!request.isEmpty());
        InspectDb inspect(path);
        inspect.exec("CREATE TRIGGER fail_second BEFORE INSERT ON members WHEN NEW.slot=1 "
                     "BEGIN SELECT RAISE(ABORT,'injected test failure'); END");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, store.approveJoin(a.id, request, now));
        QVERIFY(!store.pairForPeer(a.id)); QVERIFY(!store.pairForPeer(b.id));
        QCOMPARE(inspect.scalar("SELECT count(*) FROM pairs").toInt(), 0);
        QCOMPARE(inspect.scalar("SELECT count(*) FROM members").toInt(), 0);
        inspect.exec("DROP TRIGGER fail_second");
        QVERIFY(!store.approveJoin(a.id, request, now).isEmpty());
    }
    void concurrentJoinClaimsHaveOneWinner()
    {
        QTemporaryDir dir; const auto path = dir.filePath("concurrent.sqlite"); PairStore store(path);
        auto a = store.createDevice(); auto b = store.createDevice(); auto c = store.createDevice();
        auto invite = store.createInvite(a.id, now); QVERIFY(!invite.isEmpty());
        std::promise<void> start; auto ready = start.get_future().share();
        auto join = [&](PeerId peer) {
            PairStore worker(path); ready.wait(); return worker.requestJoin(invite, peer, now);
        };
        auto first = std::async(std::launch::async, join, b.id);
        auto second = std::async(std::launch::async, join, c.id); start.set_value();
        const auto x = first.get(); const auto y = second.get();
        QVERIFY(x.isEmpty() != y.isEmpty());
        auto pair = store.approveJoin(a.id, x.isEmpty() ? y : x, now); QVERIFY(!pair.isEmpty());
        QCOMPARE(store.members(pair).size(), 2);
        QVERIFY(!store.pairForPeer(x.isEmpty() ? b.id : c.id));
    }
    void pendingRequestPersistsAndRoutesOnlyToIssuer()
    {
        QTemporaryDir dir; const auto path = dir.filePath("pending.sqlite");
        PeerId issuer; QString request;
        {
            PairStore store(path); auto a = store.createDevice(); auto b = store.createDevice();
            issuer = a.id; request = store.requestJoin(store.createInvite(a.id, now), b.id, now);
            QVERIFY(!request.isEmpty());
        }
        PairStore reopened(path);
        QCOMPARE(reopened.inviterForRequest(request, now), std::optional<PeerId>(issuer));
        QVERIFY(!reopened.inviterForRequest(request, now.addSecs(600)));
        QVERIFY(!reopened.inviterForRequest("unknown", now));
    }
    void databaseConstraintEnforcesTwoSlots()
    {
        QTemporaryDir dir; const auto path = dir.filePath("slots.sqlite"); PairStore store(path);
        auto p = createPair(store); QVERIFY(!p.pair.isEmpty()); auto third = store.createDevice();
        InspectDb inspect(path);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            inspect.exec("INSERT INTO members(peer_id,pair_id,slot) VALUES(?,?,2)", {third.id, p.pair}));
        QCOMPARE(store.members(p.pair).size(), 2);
    }
    void unknownSchemaVersionFailsClosed()
    {
        QTemporaryDir dir; const auto path = dir.filePath("future.sqlite");
        { PairStore initial(path); }
        { InspectDb inspect(path); inspect.exec("PRAGMA user_version=99"); }
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, PairStore{path});
    }
};

QTEST_GUILESS_MAIN(PairingTest)
#include "tst_pairing.moc"
