#pragma once
#include <QMenu>
#include <QSystemTrayIcon>

class DesktopTray : public QObject {
    Q_OBJECT
public:
    explicit DesktopTray(QObject* parent=nullptr);
    QMenu* menu() { return &m_menu; }
    void show();
signals:
    void ownPetRequested();
    void friendPetRequested();
    void inboxRequested();
    void connectionRequested();
    void quitRequested();
private:
    QMenu m_menu;
    QSystemTrayIcon m_icon;
};
