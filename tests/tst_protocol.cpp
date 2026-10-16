#include "SessionFixture.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

class ProtocolTest : public QObject {
    Q_OBJECT
private slots:
    void commandRoundTrip()
    {
        auto input = command(Action::SetBusy, "a", {}, true);
        auto decoded = decodeCommand(encodeCommand(input)); QVERIFY(decoded);
        QCOMPARE(decoded->requestId, "request-1"); QCOMPARE(decoded->targetId, "a");
        QCOMPARE(decoded->action, Action::SetBusy); QVERIFY(decoded->busy);
    }
    void unicodeLimitsAndPlainText()
    {
        const QString emoji = QString::fromUtf8("☕😀");
        const auto check = [&](Action action, int limit) {
            auto c = command(action, action == Action::SetWip ? "a" : "b", QString::fromUtf8("😀").repeated(limit));
            auto valid = decodeCommand(encodeCommand(c)); QVERIFY(valid);
            QCOMPARE(valid->text.toUcs4().size(), limit);
            c.text += "한"; QVERIFY(encodeCommand(c).isEmpty());
        };
        check(Action::SetWip, 80); check(Action::Note, 120);
        auto note = decodeCommand(encodeCommand(command(Action::Note, "b", "  <b>안녕</b>\n" + emoji + "  ")));
        QVERIFY(note); QCOMPARE(note->text, "<b>안녕</b>\n" + emoji);
        QVERIFY(encodeCommand(command(Action::Note, "b", " \n ")).isEmpty());
        QVERIFY(!encodeCommand(command(Action::SetWip, "a", "")).isEmpty());
    }
    void invalidFrameIsRejected()
    {
        QVERIFY(!decodeCommand("not json")); QVERIFY(!decodeCommand("[]"));
        QVERIFY(!decodeCommand(QByteArray(16 * 1024 + 1, ' ')));
        const QJsonObject valid{{"requestId","r1"},{"action","ping"},{"targetId","b"},{"text",""},{"busy",false}};
        QVERIFY(decodeCommand(QJsonDocument(valid).toJson(QJsonDocument::Compact)));
        for (auto key : {"requestId", "targetId", "text", "busy", "action"}) {
            auto obj = valid; obj[key] = 42;
            QVERIFY(!decodeCommand(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        }
        auto unknown = valid; unknown["action"] = "launchShell";
        QVERIFY(!decodeCommand(QJsonDocument(unknown).toJson()));
        auto spoof = valid; spoof["actorId"] = "a";
        QVERIFY(!decodeCommand(QJsonDocument(spoof).toJson()));
        auto broken = command(Action::Ping); broken.action = static_cast<Action>(99);
        QVERIFY(encodeCommand(broken).isEmpty());
        QVERIFY(!decodeCommand(QByteArray("{\"text\":\"") + char(0xff) + "\"}"));
        auto surrogate = command(Action::Note, "b", QString(QChar(0xd800)));
        QVERIFY(encodeCommand(surrogate).isEmpty());
    }
    void snapshotRetainsTextAndRevision()
    {
        auto state = connectedSession(); state.revision = 9007199254740993ULL;
        state.peers["b"].inbox.push_back({"event-1", Action::Note, "<script>☕</script>"});
        auto decoded = decodeSnapshot(encodeSnapshot(state)); QVERIFY(decoded);
        QCOMPARE(decoded->revision, 9007199254740993ULL);
        QCOMPARE(decoded->peers["b"].x, 0.75);
        QCOMPARE(decoded->peers["b"].inbox.first().text, "<script>☕</script>");
        state.peers["b"].x = 1.01; QVERIFY(encodeSnapshot(state).isEmpty());
    }
    void malformedSnapshotAndEventAreRejected()
    {
        auto state = connectedSession();
        auto obj = QJsonDocument::fromJson(encodeSnapshot(state)).object();
        obj["revision"] = -1; QVERIFY(!decodeSnapshot(QJsonDocument(obj).toJson()));
        SessionEvent event{"e1", "r1", "epoch-1", 1, Action::Note, "a", "b", "쪽지 도착", "", 0.75, false, "힘내", false};
        auto bytes = encodeEvent(event); QVERIFY(!bytes.isEmpty());
        auto decoded = decodeEvent(bytes); QVERIFY(decoded); QCOMPARE(decoded->text, "힘내");
        auto bad = QJsonDocument::fromJson(bytes).object(); bad["finalX"] = "NaN";
        QVERIFY(!decodeEvent(QJsonDocument(bad).toJson()));
        bad = QJsonDocument::fromJson(bytes).object(); bad["finalX"] = -0.01;
        QVERIFY(!decodeEvent(QJsonDocument(bad).toJson()));
        bad = QJsonDocument::fromJson(bytes).object(); bad["queued"] = "yes";
        QVERIFY(!decodeEvent(QJsonDocument(bad).toJson()));
        QVERIFY(!decodeEvent("{}")); QVERIFY(!decodeSnapshot("{}"));
    }
};
QTEST_GUILESS_MAIN(ProtocolTest)
#include "tst_protocol.moc"
