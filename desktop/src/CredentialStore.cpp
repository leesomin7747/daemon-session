#include "CredentialStore.h"
#include <qt6keychain/keychain.h>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

namespace {
class KeychainBackend final : public SecretBackend {
public:
    using SecretBackend::SecretBackend;
    template<class Job> Job* job(const QString& key) {
        auto* value=new Job("daemon-session",this);
        value->setKey(key); value->setInsecureFallback(false); value->setAutoDelete(true);
        return value;
    }
    void read(const QString& key,Callback callback) override {
        auto* value=job<QKeychain::ReadPasswordJob>(key);
        connect(value,&QKeychain::Job::finished,this,[value,callback]{
            if(value->error()==QKeychain::EntryNotFound) callback({},"missing");
            else if(value->error()!=QKeychain::NoError) callback({},"secret_store");
            else callback(value->binaryData(),{});
        }); value->start();
    }
    void write(const QString& key,const QByteArray& data,Callback callback) override {
        auto* value=job<QKeychain::WritePasswordJob>(key); value->setBinaryData(data);
        connect(value,&QKeychain::Job::finished,this,[value,callback]{callback({},value->error()==QKeychain::NoError?QString():QString("secret_store"));});
        value->start();
    }
    void erase(const QString& key,Callback callback) override {
        auto* value=job<QKeychain::DeletePasswordJob>(key);
        connect(value,&QKeychain::Job::finished,this,[value,callback]{callback({},value->error()==QKeychain::NoError || value->error()==QKeychain::EntryNotFound?QString():QString("secret_store"));});
        value->start();
    }
};
bool valid(const DeviceCredential& value) {
    return value.token.size()==32 && isValidCommand({"credential",Action::Ping,value.id,{},false});
}
}
CredentialStore::CredentialStore(const QString& p,QObject* parent):CredentialStore(nullptr,p,parent) {m_backend=new KeychainBackend(this);}
CredentialStore::CredentialStore(SecretBackend* b,const QString& p,QObject* parent):QObject(parent),m_backend(b),m_profile(p) {qRegisterMetaType<DeviceCredential>();}
bool CredentialStore::setEndpoint(const QUrl& url) {
    if(m_busy) return false;
    m_key="device-"+QString::fromLatin1(QCryptographicHash::hash(m_profile.toUtf8()+'\0'+url.toEncoded(QUrl::FullyEncoded),QCryptographicHash::Sha256).toHex());
    return true;
}
QString CredentialStore::scopeKey() const {return m_key;}
bool CredentialStore::busy() const {return m_busy;}
void CredentialStore::load() {
    if(m_busy || m_key.isEmpty() || !m_backend) {emit failed("secret_busy"); return;}
    m_busy=true; QPointer<CredentialStore> guard(this);
    m_backend->read(m_key,[guard](QByteArray data,QString error){
        if(!guard) return; guard->m_busy=false;
        if(error=="missing") {emit guard->loaded({},false); return;}
        if(!error.isEmpty() || data.size()>4096) {emit guard->failed("secret_store"); return;}
        QJsonParseError parseError; const auto document=QJsonDocument::fromJson(data,&parseError);
        const auto o=document.object(); const auto encoded=o["token"].toString().toLatin1();
        const auto bytes=QByteArray::fromBase64(encoded,QByteArray::Base64UrlEncoding);
        DeviceCredential value{o["peerId"].toString(),bytes};
        if(parseError.error!=QJsonParseError::NoError || !document.isObject() || o.size()!=3 || o["v"]!=1
            || !o["peerId"].isString() || !o["token"].isString() || !valid(value)
            || bytes.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)!=encoded) {emit guard->failed("secret_corrupt"); return;}
        emit guard->loaded(value,true);
    });
}
void CredentialStore::save(const DeviceCredential& value) {
    if(m_busy || m_key.isEmpty() || !m_backend) {emit failed("secret_busy"); return;}
    if(!valid(value)) {emit failed("invalid_credential"); return;}
    const auto data=QJsonDocument(QJsonObject{{"v",1},{"peerId",value.id},{"token",QString::fromLatin1(value.token.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals))}}).toJson(QJsonDocument::Compact);
    m_busy=true; QPointer<CredentialStore> guard(this);
    m_backend->write(m_key,data,[guard](QByteArray,QString error){if(!guard) return; guard->m_busy=false; if(error.isEmpty()) emit guard->saved(); else emit guard->failed("secret_store");});
}
void CredentialStore::clear() {
    if(m_busy || m_key.isEmpty() || !m_backend) {emit failed("secret_busy"); return;}
    m_busy=true; QPointer<CredentialStore> guard(this);
    m_backend->erase(m_key,[guard](QByteArray,QString error){if(!guard) return; guard->m_busy=false; if(error.isEmpty() || error=="missing") emit guard->cleared(); else emit guard->failed("secret_store");});
}
