#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVector>
#include <optional>

using PeerId = QString;
using PairId = QString;
struct DeviceCredential { PeerId id; QByteArray token; };

enum class Action { Ping, Rope, Lure, Blanket, Plane, Coffee, Energy, Note, SetBusy, SetWip, AckInbox };
struct Command {
    QString requestId;
    Action action = Action::Ping;
    PeerId targetId;
    QString text;
    bool busy = false;
};
struct InboxItem { QString eventId; Action kind = Action::Note; QString text; };
struct PeerState {
    PeerId id;
    bool reachable = false;
    bool busy = false;
    QString wip;
    double x = 0.5;
    bool pinned = false;
    QVector<InboxItem> inbox;
};
struct SessionState {
    QString epoch;
    quint64 revision = 0;
    QMap<PeerId, PeerState> peers;
};
struct SessionEvent {
    QString eventId;
    QString requestId;
    QString epoch;
    quint64 revision = 0;
    Action action = Action::Ping;
    PeerId actorId;
    PeerId targetId;
    QString caption;
    QString bubble;
    double finalX = 0.5;
    bool queued = false;
    QString text;
    bool busy = false;
};

inline constexpr qsizetype MaxFrameBytes = 16 * 1024;
bool isValidCommand(const Command& command);
bool isValidSession(const SessionState& state);
QString actionName(Action action);
QByteArray encodeCommand(const Command& command);
std::optional<Command> decodeCommand(const QByteArray& data);
QByteArray encodeEvent(const SessionEvent& event);
std::optional<SessionEvent> decodeEvent(const QByteArray& data);
QByteArray encodeSnapshot(const SessionState& state);
std::optional<SessionState> decodeSnapshot(const QByteArray& data);
