#include "SessionBroker.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc,argv);
    QCoreApplication::setApplicationName("daemon-session-server");
    QCoreApplication::setOrganizationName("daemon-session");
    QCommandLineParser parser;
    parser.setApplicationDescription("Local daemon-session WebSocket development server");
    parser.addHelpOption();
    parser.addOption({"db","Persistent SQLite database path","path"});
    parser.addOption({"port","Loopback port (0 selects an available port)","port","8787"});
    parser.process(app);
    bool validPort=false;
    const auto port=parser.value("port").toUInt(&validPort);
    if (!validPort || port>65535 || !parser.positionalArguments().isEmpty()) {
        QTextStream(stderr)<<"Invalid server options.\n"; return 2;
    }
    QString path=parser.value("db");
    if (!parser.isSet("db")) {
        const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (directory.isEmpty() || !QDir().mkpath(directory)) {
            QTextStream(stderr)<<"Cannot prepare persistent storage.\n"; return 2;
        }
        path=QDir(directory).filePath("pairing.sqlite");
    }
    if (path.isEmpty() || path==":memory:") {
        QTextStream(stderr)<<"A persistent database path is required.\n"; return 2;
    }
    try {
        PairStore store(path);
        SessionBroker broker(store);
        if (!broker.listen(QHostAddress::LocalHost,quint16(port))) {
            QTextStream(stderr)<<"Cannot listen on the local port.\n"; return 2;
        }
        QTextStream(stdout)<<"Listening on ws://127.0.0.1:"<<broker.serverPort()<<"/\n"<<Qt::flush;
        return app.exec();
    } catch (const std::exception&) {
        QTextStream(stderr)<<"Cannot open persistent storage.\n"; return 2;
    }
}
