#include "OverlayWindow.h"
#include "SceneModel.h"
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QRandomGenerator>
#include <QTimer>
#include <QUuid>

int main(int argc,char** argv)
{
    qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
    QGuiApplication app(argc,argv);
    QCoreApplication::setApplicationName("Daemon Session — Local Preview");
#ifdef Q_OS_MAC
    app.setFont(QFont("Apple SD Gothic Neo"));
#endif
    QCommandLineParser parser; parser.addHelpOption();
    parser.setApplicationDescription("Local pet scene preview; no online connection yet");
    parser.addOption({"reduced-motion","Disable idle walking and action motion"});
    parser.addOption({"menu","Open my pet menu explicitly at launch"});
    parser.addOption({"demo-receive","Preview a received event every five seconds"});
    parser.addOption({"smoke-test","Load the scene then exit"});
    parser.process(app);
#ifdef Q_OS_LINUX
    if(!parser.isSet("smoke-test") && !QGuiApplication::platformName().startsWith("wayland")) {
        qCritical("Use native Wayland for the desktop overlay."); return 1;
    }
#endif
    if(!app.primaryScreen()) return 1;
    qmlRegisterType<SceneModel>("Daemon",1,0,"SceneModel");
    SceneModel model; model.setLocalPeerId("a");
    SessionState state{"local-preview",0,{}};
    PeerState a; a.id="a"; a.reachable=true; a.x=0.25; a.wip="알고리즘 과제";
    PeerState b; b.id="b"; b.reachable=true; b.x=0.75; b.wip="OO프로젝트 UX 점검";
    state.peers.insert(a.id,a); state.peers.insert(b.id,b); model.replaceSnapshot(state);
    CopyCatalog copy;
    const auto apply=[&](const PeerId& actor,Action action,const PeerId& target,const QString& text,bool busy) {
        Command command{QUuid::createUuid().toString(QUuid::WithoutBraces),action,target,text,busy};
        const auto result=applyCommand(state,actor,command,copy,[](int n){return QRandomGenerator::global()->bounded(n);});
        if(result.event) {state=result.state; model.applyEvent(*result.event);}
    };
    QObject::connect(&model,&SceneModel::actionRequested,&app,[&](Action action,PeerId target,QString text,bool busy) {
        apply(model.localPeerId(),action,target,text,busy);
    });
    OverlayWindow window(app.primaryScreen()); window.setContentHeight(380);
    window.setTitle("Daemon Session — Local Preview");
    window.setInitialProperties({{"model",QVariant::fromValue(&model)},
        {"nativeOverlay",QVariant::fromValue(&window)},{"reducedMotion",parser.isSet("reduced-motion")}});
    window.setSource(QUrl("qrc:/desktop/Main.qml"));
    if(window.status()!=QQuickView::Ready) return 1;
    QObject::connect(window.engine(),&QQmlEngine::quit,&app,&QCoreApplication::quit);
    QTimer incoming;
    if(parser.isSet("demo-receive")) {
        incoming.setInterval(5000);
        QObject::connect(&incoming,&QTimer::timeout,&app,[&] {
            const bool before=window.isActive();
            apply("b",Action::Coffee,"a",{},false);
            QTimer::singleShot(150,&app,[&,before] {
                qInfo().nospace()<<"PREVIEW_FOCUS before="<<before<<" after="<<window.isActive()
                    <<" appActive="<<(app.applicationState()==Qt::ApplicationActive)<<" revision="<<state.revision;
            });
        });
        incoming.start();
    }
    window.show();
    if(parser.isSet("menu")) QTimer::singleShot(0,&app,[&] {
        QMetaObject::invokeMethod(window.rootObject(),"openMenu",Q_ARG(QVariant,QVariant("a")));
    });
    if(parser.isSet("smoke-test")) QTimer::singleShot(120,&app,&QCoreApplication::quit);
    return app.exec();
}
