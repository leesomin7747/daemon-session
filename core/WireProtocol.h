#pragma once
#include "Protocol.h"
#include <QJsonObject>
struct WireFrame { QString op; QString requestId; QJsonObject body; };
QByteArray encodeFrame(const WireFrame&);
std::optional<WireFrame> decodeFrame(const QByteArray&);
