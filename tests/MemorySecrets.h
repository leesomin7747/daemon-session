#pragma once
#include "CredentialStore.h"
#include <QHash>
#include <QTimer>
class MemorySecrets : public SecretBackend {
public:
    QHash<QString,QByteArray> values;
    QString readError,writeError,eraseError;
    int writes=0,reads=0,erases=0;
    void read(const QString& key,Callback callback) override {
        ++reads;
        QTimer::singleShot(0,this,[=]{callback(values.value(key),readError.isEmpty() && !values.contains(key)?"missing":readError);});
    }
    void write(const QString& key,const QByteArray& value,Callback callback) override {
        ++writes;
        QTimer::singleShot(0,this,[=]{if(writeError.isEmpty()) values[key]=value; callback({},writeError);});
    }
    void erase(const QString& key,Callback callback) override {
        ++erases;
        QTimer::singleShot(0,this,[=]{if(eraseError.isEmpty()) values.remove(key); callback({},eraseError);});
    }
};
