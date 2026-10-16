#include "CredentialStore.h"
#include "MemorySecrets.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QSettings>
#include <QUuid>
#include <QtTest>
class CredentialsTest : public QObject {
    Q_OBJECT
private slots:
    void credentialRoundTrip() {
        MemorySecrets backend; CredentialStore store(&backend,"test");
        QVERIFY(store.setEndpoint(QUrl("wss://example.test/")));
        QSignalSpy saved(&store,&CredentialStore::saved),loaded(&store,&CredentialStore::loaded),cleared(&store,&CredentialStore::cleared);
        const DeviceCredential device{"a",QByteArray(32,'x')}; store.save(device);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(),1,600); store.load(); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(),1,600);
        QVERIFY(loaded[0][1].toBool()); const auto read=qvariant_cast<DeviceCredential>(loaded[0][0]);
        QCOMPARE(read.id,device.id); QCOMPARE(read.token,device.token);
        store.clear(); QTRY_COMPARE_WITH_TIMEOUT(cleared.size(),1,600); store.load(); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(),2,600);
        QVERIFY(!loaded[1][1].toBool()); QVERIFY(backend.values.isEmpty());
    }
    void secretStoreFailureHasNoPlaintextFallback() {
        QTemporaryDir temporary; QVERIFY(temporary.isValid());
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
        MemorySecrets backend; backend.writeError="denied";
        CredentialStore store(&backend); store.setEndpoint(QUrl("wss://example.test/"));
        QSignalSpy failed(&store,&CredentialStore::failed),saved(&store,&CredentialStore::saved);
        store.save({"a",QByteArray(32,'s')}); QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,600);
        QCOMPARE(saved.size(),0); QVERIFY(backend.values.isEmpty());
        QVERIFY(QDir(temporary.path()).entryList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot).isEmpty());
        backend.readError="unavailable"; store.load(); QTRY_COMPARE_WITH_TIMEOUT(failed.size(),2,600);
        QCOMPARE(backend.writes,1);
    }
    void endpointAndProfileIsolateCredentials() {
        MemorySecrets backend; CredentialStore a(&backend,"one"),b(&backend,"two");
        a.setEndpoint(QUrl("wss://first.test/")); b.setEndpoint(QUrl("wss://first.test/"));
        QVERIFY(!a.scopeKey().isEmpty()); QVERIFY(a.scopeKey()!=b.scopeKey());
        const auto first=a.scopeKey(); a.setEndpoint(QUrl("wss://second.test/")); QVERIFY(a.scopeKey()!=first);
        QVERIFY(!a.scopeKey().contains("first.test"));
    }
    void corruptSecretsDoNotBecomeNewIdentity() {
        MemorySecrets backend; CredentialStore store(&backend); store.setEndpoint(QUrl("wss://example.test/"));
        backend.values[store.scopeKey()]="invalid";
        QSignalSpy failed(&store,&CredentialStore::failed),loaded(&store,&CredentialStore::loaded);
        store.load(); QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,600); QCOMPARE(loaded.size(),0); QCOMPARE(backend.writes,0);
    }
    void nativeOsRoundTrip() {
        if(!qEnvironmentVariableIsSet("DAEMON_KEYCHAIN_NATIVE")) QSKIP("Explicit native keychain verification");
        CredentialStore store("test-"+QUuid::createUuid().toString(QUuid::WithoutBraces));
        QVERIFY(store.setEndpoint(QUrl("wss://keychain-test.invalid/")));
        QSignalSpy saved(&store,&CredentialStore::saved),loaded(&store,&CredentialStore::loaded),cleared(&store,&CredentialStore::cleared),errors(&store,&CredentialStore::failed);
        store.save({"native-test",QByteArray(32,'k')}); QTRY_VERIFY_WITH_TIMEOUT(!saved.isEmpty() || !errors.isEmpty(),2000);
        QVERIFY2(errors.isEmpty(),"Native keychain returned a storage error"); QCOMPARE(saved.size(),1);
        store.load(); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(),1,10000);
        const auto device=qvariant_cast<DeviceCredential>(loaded[0][0]);
        store.clear(); QTRY_COMPARE_WITH_TIMEOUT(cleared.size(),1,10000);
        QCOMPARE(device.id,QString("native-test")); QCOMPARE(device.token,QByteArray(32,'k'));
    }

};
QTEST_MAIN(CredentialsTest)
#include "tst_credentials.moc"
