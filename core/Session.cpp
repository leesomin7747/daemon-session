#include "Session.h"
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <limits>

double projectX(double x, double width, double petWidth, double margin)
{
    if (!std::isfinite(width) || !std::isfinite(petWidth) || width <= petWidth || petWidth < 0)
        return 0;
    if (!std::isfinite(x)) x = 0.5;
    if (!std::isfinite(margin)) margin = 0;
    margin = std::clamp(margin, 0.0, (width - petWidth) / 2);
    return margin + std::clamp(x, 0.0, 1.0) * (width - petWidth - 2 * margin);
}

Transition applyCommand(const SessionState& state, const PeerId& actor, const Command& command,
                        CopyCatalog& copy, const std::function<int(int)>& choose)
{
    const auto reject = [&](const QString& error) { return Transition{state, {}, error}; };
    if (!isValidSession(state) || !isValidCommand(command)) return reject("invalid");
    if (state.revision == std::numeric_limits<quint64>::max()) return reject("revision_exhausted");
    if (!state.peers.contains(actor) || !state.peers.contains(command.targetId)) return reject("forbidden");
    const bool self = command.action == Action::SetBusy || command.action == Action::SetWip
                      || command.action == Action::AckInbox;
    if (self != (actor == command.targetId)) return reject("forbidden");
    const auto& source = state.peers[actor];
    const auto& target = state.peers[command.targetId];
    if (!source.reachable || (!self && !target.reachable)) return reject("offline");
    const bool gift = command.action == Action::Coffee || command.action == Action::Energy;
    const bool note = command.action == Action::Note;
    if (!self && target.busy && !gift && !note) return reject("busy");

    SessionState next = state;
    auto& recipient = next.peers[command.targetId];
    SessionEvent event;
    event.eventId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    event.requestId = command.requestId;
    event.epoch = state.epoch;
    event.revision = ++next.revision;
    event.action = command.action;
    event.actorId = actor;
    event.targetId = command.targetId;
    event.queued = target.busy && (gift || note);
    if (note || command.action == Action::SetWip) event.text = command.text.trimmed();

    QString captionKey = "caption." + actionName(command.action);
    if (command.action == Action::SetBusy) {
        recipient.busy = command.busy;
        captionKey = recipient.busy ? "caption.busy" : "caption.ready";
    } else if (command.action == Action::SetWip) {
        recipient.wip = event.text;
    } else if (command.action == Action::AckInbox) {
        recipient.inbox.clear();
    } else if (command.action == Action::Rope || command.action == Action::Lure) {
        const double gap = command.action == Action::Rope ? 0.12 : 0.16;
        const double direction = target.x >= source.x ? 1 : -1;
        double destination = source.x + direction * gap;
        if (destination < 0 || destination > 1) destination = source.x - direction * gap;
        recipient.x = std::clamp(destination, 0.0, 1.0);
        recipient.pinned = true;
    }
    if (note || event.queued) {
        const auto text = note ? event.text : command.action == Action::Coffee
            ? QStringLiteral("친구가 커피 한 잔을 두고 갔어요.") : QStringLiteral("친구가 에너지음료를 두고 갔어요.");
        recipient.inbox.push_back({event.eventId, command.action, text});
    }
    // Keep snapshots deliverable rather than silently losing a received item on reconnect.
    const auto snapshot = encodeSnapshot(next);
    // Reserve outer-frame bytes, plus room for presence/busy flags and revision growth.
    const qsizetype limit = MaxFrameBytes - 1024
        - ((note || event.queued || command.action == Action::SetWip) ? 64 : 0);
    if (snapshot.isEmpty() || snapshot.size() > limit) return reject("session_full");
    if (event.queued) captionKey = gift ? "caption.queuedGift" : "caption.queuedNote";
    event.caption = copy.pick(captionKey, choose);
    if (!self && !note && !event.queued)
        event.bubble = copy.pick("bubble." + actionName(command.action), choose);
    event.finalX = recipient.x;
    event.busy = recipient.busy;
    return {next, event, {}};
}

SessionState applyPresence(const SessionState& state, const PeerId& peer, bool reachable)
{
    if (!isValidSession(state) || !state.peers.contains(peer)
        || state.peers[peer].reachable == reachable
        || state.revision == std::numeric_limits<quint64>::max()) return state;
    auto next = state;
    next.peers[peer].reachable = reachable;
    ++next.revision;
    return next;
}
