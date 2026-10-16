#include "WireProtocol.h"
#include <QJsonDocument>
#include <QSet>
#include <QStringDecoder>

namespace {
bool valid(const WireFrame& f)
{
    static const QSet<QString> operations = {"device.register", "auth", "invite.create",
        "invite.join", "invite.approve", "pair.revoke", "session.command", "session.snapshot",
        "result", "error", "session.event", "session.notice", "join.requested"};
    if (!operations.contains(f.op)) return false;
    const bool push = f.op == "session.event" || f.op == "session.notice" || f.op == "join.requested";
    if (push) return f.requestId.isEmpty();
    if (f.op == "session.snapshot" && f.requestId.isEmpty()) return true;
    if (f.requestId.isEmpty() || f.requestId != f.requestId.trimmed()
        || f.requestId.toUcs4().size() > 128) return false;
    for (qsizetype i=0;i<f.requestId.size();++i) {
        const auto ch=f.requestId[i];
        if (ch.category()==QChar::Other_Control || ch.isLowSurrogate()) return false;
        if (ch.isHighSurrogate()) {
            if (i+1>=f.requestId.size() || !f.requestId[i+1].isLowSurrogate()) return false;
            ++i;
        }
    }
    return true;
}
}
QByteArray encodeFrame(const WireFrame& f)
{
    if (!valid(f)) return {};
    auto bytes = QJsonDocument(QJsonObject{{"v",1},{"op",f.op},
        {"requestId",f.requestId},{"body",f.body}}).toJson(QJsonDocument::Compact);
    bytes.replace(QByteArray::fromHex("efbbbf"),"\\ufeff");
    return bytes.size() <= MaxFrameBytes ? bytes : QByteArray();
}
std::optional<WireFrame> decodeFrame(const QByteArray& bytes)
{
    if (bytes.isEmpty() || bytes.size() > MaxFrameBytes) return {};
    QStringDecoder utf8(QStringDecoder::Utf8);
    const QString text = utf8(bytes);
    Q_UNUSED(text);
    if (utf8.hasError()) return {};
    QJsonParseError error;
    QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError) return {};
    auto escaped=bytes;
    escaped.replace(QByteArray::fromHex("efbbbf"),"\\ufeff");
    const auto doc = QJsonDocument::fromJson(escaped,&error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return {};
    const auto o = doc.object();
    if (o.size() != 4 || !o["v"].isDouble() || o["v"].toDouble() != 1
        || !o["op"].isString() || !o["requestId"].isString() || !o["body"].isObject()) return {};
    WireFrame f{o["op"].toString(),o["requestId"].toString(),o["body"].toObject()};
    return valid(f) ? std::optional<WireFrame>(f) : std::nullopt;
}
