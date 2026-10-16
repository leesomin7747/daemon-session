#pragma once
#include "PairStore.h"
#include <QHostAddress>
#include <QObject>
#include <functional>
#include <memory>

struct BrokerClock {
    std::function<qint64()> monotonicMs;
    std::function<QDateTime()> utcNow;
};

class SessionBroker : public QObject {
public:
    explicit SessionBroker(PairStore&, QObject* parent = nullptr);
    SessionBroker(PairStore&, BrokerClock, QObject* parent = nullptr);
    ~SessionBroker() override;
    bool listen(const QHostAddress&, quint16);
    quint16 serverPort() const;
    void close();
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
