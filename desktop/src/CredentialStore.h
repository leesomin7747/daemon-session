#pragma once
#include "Protocol.h"
#include <QObject>
#include <QUrl>
#include <functional>

Q_DECLARE_METATYPE(DeviceCredential)
class SecretBackend : public QObject {
public:
    using Callback=std::function<void(QByteArray,QString)>;
    using QObject::QObject;
    virtual void read(const QString& key,Callback)=0;
    virtual void write(const QString& key,const QByteArray&,Callback)=0;
    virtual void erase(const QString& key,Callback)=0;
};
class CredentialStore : public QObject {
    Q_OBJECT
public:
    explicit CredentialStore(const QString& profile="default",QObject* parent=nullptr);
    CredentialStore(SecretBackend*,const QString& profile="default",QObject* parent=nullptr);
    bool setEndpoint(const QUrl&);
    QString scopeKey() const;
    bool busy() const;
    void load();
    void save(const DeviceCredential&);
    void clear();
signals:
    void loaded(DeviceCredential credential,bool found);
    void saved();
    void cleared();
    void failed(QString code);
private:
    SecretBackend* m_backend=nullptr;
    QString m_profile,m_key;
    bool m_busy=false;
};
