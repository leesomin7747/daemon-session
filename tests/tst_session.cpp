#include "SessionFixture.h"
#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <limits>

class SessionTest : public QObject {
    Q_OBJECT
private slots:
    void busyRejectsPrankWithoutQueue()
    {
        for (auto action : {Action::Ping, Action::Rope, Action::Lure, Action::Blanket, Action::Plane}) {
            auto state = connectedSession(); state.peers["b"].busy = true;
            CopyCatalog copy;
            auto result = applyCommand(state, "a", command(action), copy, chooseFirst);
            QCOMPARE(result.error, "busy"); QVERIFY(!result.event);
            QCOMPARE(result.state.revision, 0u); QVERIFY(result.state.peers["b"].inbox.isEmpty());
            QCOMPARE(result.state.peers["b"].x, 0.75);
        }
    }
    void busyQueuesGiftAndNoteSilently()
    {
        auto state = connectedSession(); state.peers["b"].busy = true; CopyCatalog copy;
        for (auto action : {Action::Coffee, Action::Energy, Action::Note}) {
            auto result = applyCommand(state, "a", command(action, "b", action == Action::Note ? "힘내 ☕" : ""), copy, chooseFirst);
            QVERIFY(result.error.isEmpty()); QVERIFY(result.event); QVERIFY(result.event->queued);
            QVERIFY(result.event->bubble.isEmpty());
            QCOMPARE(result.state.peers["b"].inbox.size(), state.peers["b"].inbox.size() + 1);
            QCOMPARE(result.state.peers["b"].inbox.last().eventId, result.event->eventId);
            QCOMPARE(result.event->text, result.state.peers["b"].inbox.last().text);
            auto delivered = decodeEvent(encodeEvent(*result.event)); QVERIFY(delivered);
            auto restored = decodeSnapshot(encodeSnapshot(result.state)); QVERIFY(restored);
            QCOMPARE(delivered->text, restored->peers["b"].inbox.last().text);
            QCOMPARE(delivered->eventId, restored->peers["b"].inbox.last().eventId);
            state = result.state;
        }
        QCOMPARE(state.peers["b"].inbox.last().text, "힘내 ☕");
    }
    void unbusyDoesNotAutoOpen()
    {
        auto state = connectedSession(); state.peers["b"].busy = true; CopyCatalog copy;
        state = applyCommand(state, "a", command(Action::Note, "b", "나중에 봐"), copy, chooseFirst).state;
        auto result = applyCommand(state, "b", command(Action::SetBusy, "b", {}, false), copy, chooseFirst);
        QVERIFY(result.event); QVERIFY(!result.state.peers["b"].busy);
        QCOMPARE(result.state.peers["b"].inbox.size(), 1);
        QCOMPARE(result.state.peers["b"].inbox.first().text, "나중에 봐");
    }
    void offlineRejectsNote()
    {
        auto state = connectedSession(); state.peers["b"].reachable = false; CopyCatalog copy;
        auto result = applyCommand(state, "a", command(Action::Note, "b", "안녕"), copy, chooseFirst);
        QCOMPARE(result.error, "offline"); QVERIFY(!result.event);
        QVERIFY(result.state.peers["b"].inbox.isEmpty());
        auto local = applyCommand(state, "a", command(Action::SetWip, "a", "과제"), copy, chooseFirst);
        QVERIFY(local.event); QCOMPARE(local.state.peers["a"].wip, "과제");
    }
    void onlyOwnerChangesWip()
    {
        auto state = connectedSession(); CopyCatalog copy;
        auto denied = applyCommand(state, "a", command(Action::SetWip, "b", "과제"), copy, chooseFirst);
        QCOMPARE(denied.error, "forbidden"); QVERIFY(!denied.event);
        auto own = applyCommand(state, "a", command(Action::SetWip, "a", "  알고리즘 과제  "), copy, chooseFirst);
        QVERIFY(own.event); QCOMPARE(own.state.peers["a"].wip, "알고리즘 과제");
        QCOMPARE(own.event->text, "알고리즘 과제");
        QCOMPARE(own.state.revision, 1u); QCOMPARE(own.event->revision, 1u);
    }
    void ropeAndLurePinDestination()
    {
        for (auto action : {Action::Rope, Action::Lure}) {
            auto state = connectedSession(); CopyCatalog copy;
            auto first = applyCommand(state, "a", command(action), copy, chooseFirst);
            QVERIFY(first.event); QVERIFY(first.state.peers["b"].pinned);
            const double x = first.state.peers["b"].x;
            QVERIFY(x >= 0 && x <= 1); QVERIFY(qAbs(x - 0.25) < 0.25);
            QCOMPARE(first.event->finalX, x);
            auto again = applyCommand(first.state, "a", command(action), copy, chooseFirst);
            QCOMPARE(again.state.peers["b"].x, x);
            state.peers["a"].x = 0.99;
            auto edge = applyCommand(state, "a", command(action), copy, chooseFirst);
            QVERIFY(edge.event); QVERIFY(edge.event->finalX <= 1 && edge.event->finalX >= 0);
        }
    }
    void otherActionKeepsPinnedPosition()
    {
        auto state = connectedSession(); CopyCatalog copy;
        state = applyCommand(state, "a", command(Action::Rope), copy, chooseFirst).state;
        for (auto action : {Action::Ping, Action::Coffee, Action::Energy, Action::Blanket, Action::Plane, Action::Note}) {
            auto result = applyCommand(state, "a", command(action, "b", action == Action::Note ? "쪽지" : ""), copy, chooseFirst);
            QVERIFY(result.event); QCOMPARE(result.state.peers["b"].x, state.peers["b"].x);
            QVERIFY(result.state.peers["b"].pinned);
        }
    }
    void copyAvoidsImmediateRepeat()
    {
        CopyCatalog copy;
        auto first = copy.pick("bubble.coffee", chooseFirst);
        QVERIFY(!first.isEmpty());
        QVERIFY(!copy.pick("bubble.energy", chooseFirst).isEmpty());
        auto second = copy.pick("bubble.coffee", chooseFirst); QVERIFY(first != second);
        QVERIFY(second != copy.pick("bubble.coffee", chooseFirst));
    }
    void sameEventHasSameBubble()
    {
        auto state = connectedSession(); CopyCatalog copy;
        auto result = applyCommand(state, "a", command(Action::Coffee), copy, chooseFirst);
        QVERIFY(result.event);
        auto bytes = encodeEvent(*result.event);
        for (int client = 0; client < 2; ++client) {
            auto received = decodeEvent(bytes); QVERIFY(received);
            QCOMPARE(received->bubble, "한 모금만");
            QCOMPARE(received->caption, "커피 한 잔 도착. 말 대신 두 손으로 건넸어요.");
        }
    }
    void busyOrderingAndAcknowledgment()
    {
        auto state = connectedSession(); CopyCatalog copy;
        auto busy = applyCommand(state, "b", command(Action::SetBusy, "b", {}, true), copy, chooseFirst);
        QVERIFY(busy.event); QVERIFY(busy.event->busy);
        auto blocked = applyCommand(busy.state, "a", command(Action::Ping), copy, chooseFirst);
        QCOMPARE(blocked.error, "busy"); QCOMPARE(blocked.state.revision, 1u);
        auto pingFirst = applyCommand(state, "a", command(Action::Ping), copy, chooseFirst);
        QVERIFY(pingFirst.event);
        auto busySecond = applyCommand(pingFirst.state, "b", command(Action::SetBusy, "b", {}, true), copy, chooseFirst);
        QCOMPARE(busySecond.state.revision, 2u);
        auto note = applyCommand(busySecond.state, "a", command(Action::Note, "b", "쪽지"), copy, chooseFirst);
        auto forbidden = applyCommand(note.state, "a", command(Action::AckInbox), copy, chooseFirst);
        QCOMPARE(forbidden.error, "forbidden"); QCOMPARE(forbidden.state.peers["b"].inbox.size(), 1);
        auto ack = applyCommand(note.state, "b", command(Action::AckInbox), copy, chooseFirst);
        QVERIFY(ack.event); QVERIFY(ack.state.peers["b"].inbox.isEmpty());
    }
    void presencePreservesDeliveredNotes()
    {
        auto state = connectedSession(); CopyCatalog copy;
        state = applyCommand(state, "a", command(Action::Note, "b", "받은 쪽지"), copy, chooseFirst).state;
        auto offline = applyPresence(state, "b", false);
        QVERIFY(!offline.peers["b"].reachable); QCOMPARE(offline.revision, 2u);
        QCOMPARE(offline.peers["b"].inbox.size(), 1);
        QCOMPARE(applyPresence(offline, "b", false).revision, 2u);
        QCOMPARE(applyPresence(offline, "unknown", true).peers.size(), 2);
    }
    void invalidActorAndCoordinatesAreRejected()
    {
        auto state = connectedSession(); CopyCatalog copy;
        QCOMPARE(applyCommand(state, "unknown", command(Action::Ping), copy, chooseFirst).error, "forbidden");
        state.peers["b"].x = std::numeric_limits<double>::quiet_NaN();
        auto invalid = applyCommand(state, "a", command(Action::Ping), copy, chooseFirst);
        QVERIFY(!invalid.error.isEmpty()); QVERIFY(!invalid.event);
    }
    void fullSessionRejectsNewNotesWithoutDroppingDeliveredItems()
    {
        auto state = connectedSession(); CopyCatalog copy; bool full = false;
        for (int i = 0; i < 128; ++i) {
            auto result = applyCommand(state, "a", command(Action::Note, "b", QString::fromUtf8("😀").repeated(120)), copy, chooseFirst);
            if (!result.event) {
                QCOMPARE(result.error, "session_full");
                QCOMPARE(result.state.peers["b"].inbox.size(), state.peers["b"].inbox.size());
                QCOMPARE(result.state.revision, state.revision);
                full = true; break;
            }
            state = result.state;
            QVERIFY(!encodeSnapshot(state).isEmpty());
            QVERIFY(encodeSnapshot(state).size() <= MaxFrameBytes);
        }
        QVERIFY(full);
        const QJsonObject envelope{{"v", 1}, {"op", "session.snapshot"},
            {"requestId", QString::fromUtf8("😀").repeated(128)},
            {"body", QJsonDocument::fromJson(encodeSnapshot(state)).object()}};
        QVERIFY2(QJsonDocument(envelope).toJson(QJsonDocument::Compact).size() <= 16 * 1024,
                 "A full state must still fit in the planned wire envelope");
        auto ack = applyCommand(state, "b", command(Action::AckInbox), copy, chooseFirst);
        QVERIFY(ack.event);
        auto retry = applyCommand(ack.state, "a", command(Action::Note, "b", "공간이 생겼어"), copy, chooseFirst);
        QVERIFY(retry.event); QCOMPARE(retry.state.peers["b"].inbox.size(), 1);
    }
};
QTEST_GUILESS_MAIN(SessionTest)
#include "tst_session.moc"
