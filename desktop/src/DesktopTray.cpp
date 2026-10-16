#include "DesktopTray.h"
#include <QPainter>
#include <QPainterPath>

DesktopTray::DesktopTray(QObject* parent):QObject(parent)
{
    const auto add=[this](const QString& text,auto signal) {
        auto* action=m_menu.addAction(text);
        connect(action,&QAction::triggered,this,signal);
    };
    add("내 펫 메뉴 열기",&DesktopTray::ownPetRequested);
    add("친구 펫 메뉴 열기",&DesktopTray::friendPetRequested);
    add("받은 것",&DesktopTray::inboxRequested);
    m_menu.addSeparator(); add("종료",&DesktopTray::quitRequested);
    QPixmap image(32,32); image.fill(Qt::transparent);
    QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath silhouette;
    silhouette.moveTo(5,16); silhouette.lineTo(6,5); silhouette.lineTo(12,9);
    silhouette.quadTo(16,7,20,9); silhouette.lineTo(26,5); silhouette.lineTo(27,16);
    silhouette.cubicTo(30,29,2,29,5,16); silhouette.closeSubpath();
#ifdef Q_OS_MAC
    painter.fillPath(silhouette,Qt::black);
#else
    painter.fillPath(silhouette,QColor("#bad8ba"));
#endif
    painter.end(); QIcon icon(image);
#ifdef Q_OS_MAC
    icon.setIsMask(true);
#endif
    m_icon.setIcon(icon); m_icon.setToolTip("Daemon Session · 로컬 미리보기"); m_icon.setContextMenu(&m_menu);
}
void DesktopTray::show() { m_icon.show(); }
