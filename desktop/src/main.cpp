#include "OverlayWindow.h"
#include "SceneModel.h"
#include "DesktopTray.h"
#include "SessionClient.h"
#include <QCommandLineParser>
#include <QApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QRandomGenerator>
#include <QTimer>
#include <QUuid>
#include <QSettings>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>
#include <QLockFile>
#include <memory>

int main(int argc,char** argv)
{
    qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
    QApplication app(argc,argv);
    app.setQuitOnLastWindowClosed(false);
    QCoreApplication::setOrganizationName("daemon-session");
    QCoreApplication::setApplicationName("daemon-session");
#ifdef Q_OS_MAC
    app.setFont(QFont("Apple SD Gothic Neo"));
#endif
    QCommandLineParser parser; parser.addHelpOption();
    parser.setApplicationDescription("Paired desktop companion session");
    parser.addOption({"preview","Run the fixed local scene without a server"});
    parser.addOption({"server","Connect to a WSS server, or loopback development WS","url"});
    parser.addOption({"profile","Separate non-secret settings and OS credential namespace","name","default"});
    parser.addOption({"reduced-motion","Disable idle walking and action motion"});
    parser.addOption({"menu","Open my pet menu explicitly at launch"});
    parser.addOption({"demo-receive","Preview a received event every five seconds"});
    parser.addOption({"smoke-test","Load the scene then exit"});
    parser.process(app);
    const bool smoke=parser.isSet("smoke-test");
    const bool preview=parser.isSet("preview") || parser.isSet("demo-receive");
    const auto profile=parser.value("profile");
    if(!QRegularExpression("^[A-Za-z0-9_-]{1,32}$").match(profile).hasMatch()) {qCritical("Invalid profile name."); return 2;}
    QSettings settings("daemon-session","desktop"); settings.beginGroup(profile);
    std::unique_ptr<QLockFile> instanceLock;
    if(!preview && !smoke) {
        const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if(!QDir().mkpath(directory)) {qCritical("Cannot create application data directory."); return 2;}
        instanceLock=std::make_unique<QLockFile>(directory+"/"+profile+".lock");
        if(!instanceLock->tryLock(0)) {qCritical("Another instance uses this profile."); return 2;}
    }
#ifdef Q_OS_LINUX
    if(!parser.isSet("smoke-test") && !QGuiApplication::platformName().startsWith("wayland")) {
        qCritical("Use native Wayland for the desktop overlay."); return 1;
    }
#endif
    if(!app.primaryScreen()) return 1;
    qmlRegisterType<SceneModel>("Daemon",1,0,"SceneModel");
    CredentialStore credentials(profile);
    SessionClient client(&credentials);
    const auto server=parser.isSet("server")?parser.value("server"):settings.value("serverUrl","ws://127.0.0.1:8787/").toString();
    if(!client.endpointAllowed(QUrl(server))) {qCritical("Unsafe or invalid server URL."); return 2;}
    SceneModel model; model.setLocalPeerId(preview?"a":"local-device");
    model.setShowLocalOffline(settings.value("showLocalOffline",true).toBool());
    model.setTransportConnected(preview);
    SessionState state{"local-preview",0,{}};
    PeerState a; a.id="a"; a.reachable=true; a.x=0.25; a.wip="알고리즘 과제";
    PeerState b; b.id="b"; b.reachable=true; b.x=0.75; b.wip="OO프로젝트 UX 점검";
    state.peers.insert(a.id,a); state.peers.insert(b.id,b); if(preview) model.replaceSnapshot(state);
    CopyCatalog copy;
    const auto apply=[&](const PeerId& actor,Action action,const PeerId& target,const QString& text,bool busy,const QString& request) {
        Command command{request,action,target,text,busy};
        const auto result=applyCommand(state,actor,command,copy,[](int n){return QRandomGenerator::global()->bounded(n);});
        if(result.event) {state=result.state; model.applyEvent(*result.event);}
        else if(!result.error.isEmpty()) model.requestFailed(request,result.error);
    };
    QObject::connect(&model,&SceneModel::actionRequested,&client,[&](Action action,PeerId target,QString text,bool busy,QString request) {
        if(preview) apply(model.localPeerId(),action,target,text,busy,request);
        else client.send({request,action,target,text,busy});
    });
    QObject::connect(&client,&SessionClient::snapshotReceived,&model,[&](const SessionState& snapshot){model.setLocalPeerId(client.peerId()); model.setTransportConnected(true); model.replaceSnapshot(snapshot);});
    QObject::connect(&client,&SessionClient::eventReceived,&model,&SceneModel::applyEvent);
    QObject::connect(&client,&SessionClient::error,&model,&SceneModel::requestFailed);
    QObject::connect(&model,&SceneModel::snapshotRequired,&client,&SessionClient::requestSnapshot);
    QString previousIdentity;
    QObject::connect(&client,&SessionClient::connectionChanged,&model,[&]{
        if(preview) return;
        const auto identity=client.peerId();
        if(!identity.isEmpty() && identity!=previousIdentity) {model.resetSession(); previousIdentity=identity; model.setLocalPeerId(identity);}
        if(identity.isEmpty() && (client.status()=="offline" || client.status()=="revoked") && !previousIdentity.isEmpty()) {
            model.resetSession(); previousIdentity.clear(); model.setLocalPeerId("local-device"); if(!smoke) settings.setValue("autoConnect",false);
        }
        model.setTransportConnected(client.paired());
        if(client.connected() && !smoke) {settings.setValue("serverUrl",client.serverUrl()); settings.setValue("autoConnect",true);}
    });
    QObject::connect(&model,&SceneModel::showLocalOfflineChanged,&model,[&]{if(!preview && !smoke) settings.setValue("showLocalOffline",model.showLocalOffline());});
    OverlayWindow window(app.primaryScreen()); window.setContentHeight(380);
    window.setTitle(preview?"Daemon Session — Local Preview":"Daemon Session");
    window.setInitialProperties({{"model",QVariant::fromValue(&model)},
        {"nativeOverlay",QVariant::fromValue(&window)},{"reducedMotion",parser.isSet("reduced-motion")},
        {"sessionClient",QVariant::fromValue(static_cast<QObject*>(preview?nullptr:&client))},{"serverUrl",server}});
    window.setSource(QUrl("qrc:/desktop/Main.qml"));
    if(window.status()!=QQuickView::Ready) return 1;
    QObject::connect(window.engine(),&QQmlEngine::quit,&app,&QCoreApplication::quit);
    DesktopTray tray;
    const auto openPanel=[&](const QString& kind,const QString& peer) {
        QMetaObject::invokeMethod(window.rootObject(),"openPanel",Q_ARG(QVariant,QVariant(kind)),Q_ARG(QVariant,QVariant(peer)));
    };
    const auto openConnection=[&]{QMetaObject::invokeMethod(window.rootObject(),"openConnection");};
    QObject::connect(&tray,&DesktopTray::ownPetRequested,&app,[&]{openPanel("wip",model.localPeerId());});
    QObject::connect(&tray,&DesktopTray::friendPetRequested,&app,[&]{
        const auto peers=model.peerStates(); for(auto it=peers.cbegin();it!=peers.cend();++it) if(it.key()!=model.localPeerId()) {openPanel("wip",it.key()); return;}
        openConnection();
    });
    QObject::connect(&tray,&DesktopTray::inboxRequested,&app,[&]{openPanel("inbox",model.localPeerId());});
    QObject::connect(&tray,&DesktopTray::quitRequested,&app,&QCoreApplication::quit);
    QObject::connect(&tray,&DesktopTray::connectionRequested,&app,openConnection);
    if(!parser.isSet("smoke-test")) tray.show();
    QTimer incoming;
    if(parser.isSet("demo-receive")) {
        incoming.setInterval(5000);
        QObject::connect(&incoming,&QTimer::timeout,&app,[&] {
            const bool before=window.isActive();
            apply("b",Action::Coffee,"a",{},false,model.newRequestId());
            QTimer::singleShot(150,&app,[&,before] {
                qInfo().nospace()<<"PREVIEW_FOCUS before="<<before<<" after="<<window.isActive()
                    <<" appActive="<<(app.applicationState()==Qt::ApplicationActive)<<" revision="<<state.revision;
            });
        });
        incoming.start();
    }
    window.show();
    const bool autoConnect=!preview && !smoke && (parser.isSet("server") || settings.value("autoConnect",false).toBool());
    if(autoConnect) QTimer::singleShot(0,&app,[&]{client.connectTo(QUrl(server));});
    if(!preview && !smoke && !autoConnect) QTimer::singleShot(0,&app,openConnection);
    if(parser.isSet("menu")) QTimer::singleShot(0,&app,[&] {
        QMetaObject::invokeMethod(window.rootObject(),"openMenu",Q_ARG(QVariant,QVariant(model.localPeerId())));
    });
    if(parser.isSet("smoke-test")) QTimer::singleShot(120,&app,&QCoreApplication::quit);
    return app.exec();
}
