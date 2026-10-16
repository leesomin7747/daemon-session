#include "Protocol.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringDecoder>
#include <cmath>

namespace {
const char* const names[] = {"ping", "rope", "lure", "blanket", "plane", "coffee", "energy",
                             "note", "setBusy", "setWip", "ackInbox"};

std::optional<Action> parseAction(const QJsonValue& value)
{
    if (!value.isString()) return {};
    for (int i = 0; i < 11; ++i)
        if (value.toString() == QLatin1String(names[i])) return static_cast<Action>(i);
    return {};
}

bool textFits(const QString& text, qsizetype limit, bool emptyAllowed)
{
    qsizetype count = 0;
    if (!emptyAllowed && text.trimmed().isEmpty()) return false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text[i].isHighSurrogate()) {
            if (i + 1 >= text.size() || !text[i + 1].isLowSurrogate()) return false;
            ++i;
        } else if (text[i].isLowSurrogate()) {
            return false;
        }
        if (++count > limit) return false;
    }
    return true;
}

bool validId(const QString& id)
{
    if (id != id.trimmed() || !textFits(id, 128, false)) return false;
    for (const auto ch : id)
        if (ch.category() == QChar::Other_Control) return false;
    return true;
}

bool position(double x) { return std::isfinite(x) && x >= 0 && x <= 1; }
bool gift(Action action) { return action == Action::Coffee || action == Action::Energy; }
bool own(Action action)
{
    return action == Action::SetBusy || action == Action::SetWip || action == Action::AckInbox;
}

bool exactKeys(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    if (object.size() != static_cast<qsizetype>(keys.size())) return false;
    for (const auto* key : keys)
        if (!object.contains(QLatin1String(key))) return false;
    return true;
}

std::optional<QJsonObject> readObject(const QByteArray& bytes)
{
    if (bytes.isEmpty() || bytes.size() > MaxFrameBytes) return {};
    QStringDecoder utf8(QStringDecoder::Utf8);
    const QString decoded = utf8(bytes);
    Q_UNUSED(decoded);
    if (utf8.hasError()) return {};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    return document.object();
}

QByteArray writeObject(const QJsonObject& object)
{
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return bytes.size() <= MaxFrameBytes ? bytes : QByteArray();
}

std::optional<quint64> revision(const QJsonValue& value)
{
    if (!value.isString()) return {};
    bool ok = false;
    const auto number = value.toString().toULongLong(&ok);
    if (!ok || QString::number(number) != value.toString()) return {};
    return number;
}

bool validEvent(const SessionEvent& event)
{
    if (!validId(event.eventId) || !validId(event.requestId) || !validId(event.epoch)
        || !validId(event.actorId) || !validId(event.targetId) || event.revision == 0
        || actionName(event.action).isEmpty() || !position(event.finalX)
        || !textFits(event.caption, 512, false) || !textFits(event.bubble, 512, true)) return false;
    if (own(event.action) != (event.actorId == event.targetId)) return false;
    if (!own(event.action) && event.busy && !gift(event.action) && event.action != Action::Note)
        return false;
    if ((gift(event.action) || event.action == Action::Note) && event.queued != event.busy)
        return false;
    if (event.queued && (!event.busy || (!gift(event.action) && event.action != Action::Note)
                         || !event.bubble.isEmpty())) return false;
    if (event.action == Action::Note) return textFits(event.text, 120, false);
    if (event.action == Action::SetWip) return textFits(event.text, 80, true);
    if (gift(event.action) && event.queued) return textFits(event.text, 512, false);
    return event.text.isEmpty();
}
}

QString actionName(Action action)
{
    const auto index = static_cast<int>(action);
    return index >= 0 && index < 11 ? QString::fromLatin1(names[index]) : QString();
}

bool isValidCommand(const Command& command)
{
    if (!validId(command.requestId) || !validId(command.targetId)
        || actionName(command.action).isEmpty()) return false;
    if (command.action != Action::SetBusy && command.busy) return false;
    const auto text = command.text.trimmed();
    if (command.action == Action::Note) return textFits(text, 120, false);
    if (command.action == Action::SetWip) return textFits(text, 80, true);
    return text.isEmpty();
}

bool isValidSession(const SessionState& state)
{
    if (!validId(state.epoch) || state.peers.size() != 2) return false;
    for (auto it = state.peers.cbegin(); it != state.peers.cend(); ++it) {
        const auto& peer = it.value();
        if (it.key() != peer.id || !validId(peer.id) || !position(peer.x)
            || peer.wip != peer.wip.trimmed() || !textFits(peer.wip, 80, true)) return false;
        QSet<QString> ids;
        for (const auto& item : peer.inbox) {
            if (!validId(item.eventId) || ids.contains(item.eventId)
                || (item.kind != Action::Note && !gift(item.kind))
                || !textFits(item.text, item.kind == Action::Note ? 120 : 512, false)) return false;
            ids.insert(item.eventId);
        }
    }
    return true;
}

QByteArray encodeCommand(const Command& command)
{
    if (!isValidCommand(command)) return {};
    return writeObject({{"requestId", command.requestId}, {"action", actionName(command.action)},
                        {"targetId", command.targetId}, {"text", command.text.trimmed()}, {"busy", command.busy}});
}

std::optional<Command> decodeCommand(const QByteArray& data)
{
    auto object = readObject(data);
    if (!object || !exactKeys(*object, {"requestId", "action", "targetId", "text", "busy"})) return {};
    const auto& o = *object;
    const auto action = parseAction(o["action"]);
    if (!action || !o["requestId"].isString() || !o["targetId"].isString()
        || !o["text"].isString() || !o["busy"].isBool()) return {};
    Command command{o["requestId"].toString(), *action, o["targetId"].toString(),
                    o["text"].toString().trimmed(), o["busy"].toBool()};
    return isValidCommand(command) ? std::optional<Command>(command) : std::nullopt;
}

QByteArray encodeEvent(const SessionEvent& event)
{
    if (!validEvent(event)) return {};
    return writeObject({{"eventId", event.eventId}, {"requestId", event.requestId},
        {"epoch", event.epoch}, {"revision", QString::number(event.revision)},
        {"action", actionName(event.action)}, {"actorId", event.actorId}, {"targetId", event.targetId},
        {"caption", event.caption}, {"bubble", event.bubble}, {"finalX", event.finalX},
        {"queued", event.queued}, {"text", event.text}, {"busy", event.busy}});
}

std::optional<SessionEvent> decodeEvent(const QByteArray& data)
{
    auto object = readObject(data);
    if (!object || !exactKeys(*object, {"eventId", "requestId", "epoch", "revision", "action",
            "actorId", "targetId", "caption", "bubble", "finalX", "queued", "text", "busy"})) return {};
    const auto& o = *object;
    const auto action = parseAction(o["action"]);
    const auto rev = revision(o["revision"]);
    for (const auto* key : {"eventId", "requestId", "epoch", "actorId", "targetId", "caption", "bubble", "text"})
        if (!o[QLatin1String(key)].isString()) return {};
    if (!action || !rev || !o["finalX"].isDouble() || !o["queued"].isBool() || !o["busy"].isBool()) return {};
    SessionEvent event{o["eventId"].toString(), o["requestId"].toString(), o["epoch"].toString(), *rev,
        *action, o["actorId"].toString(), o["targetId"].toString(), o["caption"].toString(),
        o["bubble"].toString(), o["finalX"].toDouble(), o["queued"].toBool(),
        o["text"].toString(), o["busy"].toBool()};
    return validEvent(event) ? std::optional<SessionEvent>(event) : std::nullopt;
}

QByteArray encodeSnapshot(const SessionState& state)
{
    if (!isValidSession(state)) return {};
    QJsonArray peers;
    for (const auto& peer : state.peers) {
        QJsonArray inbox;
        for (const auto& item : peer.inbox)
            inbox.append(QJsonObject{{"eventId", item.eventId}, {"kind", actionName(item.kind)}, {"text", item.text}});
        peers.append(QJsonObject{{"id", peer.id}, {"reachable", peer.reachable}, {"busy", peer.busy},
            {"wip", peer.wip}, {"x", peer.x}, {"pinned", peer.pinned}, {"inbox", inbox}});
    }
    return writeObject({{"epoch", state.epoch}, {"revision", QString::number(state.revision)}, {"peers", peers}});
}

std::optional<SessionState> decodeSnapshot(const QByteArray& data)
{
    auto object = readObject(data);
    if (!object || !exactKeys(*object, {"epoch", "revision", "peers"})) return {};
    const auto rev = revision((*object)["revision"]);
    if (!rev || !(*object)["epoch"].isString() || !(*object)["peers"].isArray()) return {};
    const auto peers = (*object)["peers"].toArray();
    if (peers.size() != 2) return {};
    SessionState state{(*object)["epoch"].toString(), *rev, {}};
    for (const auto& value : peers) {
        if (!value.isObject()) return {};
        const auto p = value.toObject();
        if (!exactKeys(p, {"id", "reachable", "busy", "wip", "x", "pinned", "inbox"})
            || !p["id"].isString() || !p["reachable"].isBool() || !p["busy"].isBool()
            || !p["wip"].isString() || !p["x"].isDouble() || !p["pinned"].isBool()
            || !p["inbox"].isArray()) return {};
        PeerState peer{p["id"].toString(), p["reachable"].toBool(), p["busy"].toBool(),
            p["wip"].toString(), p["x"].toDouble(), p["pinned"].toBool(), {}};
        if (state.peers.contains(peer.id)) return {};
        for (const auto& itemValue : p["inbox"].toArray()) {
            if (!itemValue.isObject()) return {};
            const auto item = itemValue.toObject();
            const auto kind = parseAction(item["kind"]);
            if (!exactKeys(item, {"eventId", "kind", "text"}) || !kind
                || !item["eventId"].isString() || !item["text"].isString()) return {};
            peer.inbox.push_back({item["eventId"].toString(), *kind, item["text"].toString()});
        }
        state.peers.insert(peer.id, peer);
    }
    return isValidSession(state) ? std::optional<SessionState>(state) : std::nullopt;
}
