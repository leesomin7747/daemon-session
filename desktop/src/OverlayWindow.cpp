#include "OverlayWindow.h"
#include <QGuiApplication>
#include <QScreen>
#include <cmath>

#ifdef DAEMON_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

OverlayWindow::OverlayWindow(QScreen* screen, QObject* parent)
{
    setScreen(screen);
    QObject::setParent(parent);
    setColor(Qt::transparent);
    setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
             | Qt::NoDropShadowWindowHint | Qt::WindowDoesNotAcceptFocus
             | Qt::WindowTransparentForInput);
    setResizeMode(QQuickView::SizeRootObjectToView);
    resize(800, 180);
#ifdef DAEMON_LAYER_SHELL
    if (QGuiApplication::platformName().startsWith("wayland")) {
        auto* layer = LayerShellQt::Window::get(this);
        layer->setAnchors(LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
                          | LayerShellQt::Window::AnchorRight);
        layer->setLayer(LayerShellQt::Window::LayerTop);
        layer->setExclusiveZone(0);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        layer->setActivateOnShow(false);
        layer->setDesiredSize(QSize(0, 180));
    }
#endif
    connect(this, &QWindow::widthChanged, this, &OverlayWindow::updateInputRegion);
    connect(this, &QWindow::heightChanged, this, &OverlayWindow::updateInputRegion);
    if (screen) {
        connect(screen, &QScreen::availableGeometryChanged, this, &OverlayWindow::updatePlacement);
        updatePlacement();
    }
}

void OverlayWindow::setInteractiveRegions(const QVector<QRectF>& regions)
{
    m_regions = regions;
    updateInputRegion();
}

double OverlayWindow::petX(double x, double petWidth, double margin) const
{
    return projectX(x, width(), petWidth, margin);
}

void OverlayWindow::updateInputRegion()
{
    QRegion region;
    const QRectF bounds(0, 0, width(), height());
    for (const auto& rect : m_regions) {
        if (!std::isfinite(rect.x()) || !std::isfinite(rect.y()) || !std::isfinite(rect.width())
            || !std::isfinite(rect.height()) || rect.width() <= 0 || rect.height() <= 0)
            continue;
        const auto clipped = rect.intersected(bounds);
        if (!clipped.isEmpty())
            region += clipped.toAlignedRect();
    }
    setMask(region);
    setFlag(Qt::WindowTransparentForInput, region.isEmpty());
}

void OverlayWindow::setEditorActive(bool active)
{
    if (m_editorActive == active && !active)
        return;
    m_editorActive = active;
    if (active && QGuiApplication::focusWindow() != this)
        m_previousWindow = QGuiApplication::focusWindow();
    const bool returnFocus = !active && isActive();
    setFlag(Qt::WindowDoesNotAcceptFocus, !active);
#ifdef DAEMON_LAYER_SHELL
    if (QGuiApplication::platformName().startsWith("wayland")) {
        LayerShellQt::Window::get(this)->setKeyboardInteractivity(
            active ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                   : LayerShellQt::Window::KeyboardInteractivityNone);
    }
#endif
    if (active && isVisible())
        requestActivate();
    else if (returnFocus && m_previousWindow && m_previousWindow->isVisible())
        m_previousWindow->requestActivate();
    if (!active)
        m_previousWindow.clear();
}

void OverlayWindow::updatePlacement()
{
    if (!screen())
        return;
    const auto area = screen()->availableGeometry();
#ifdef DAEMON_LAYER_SHELL
    if (QGuiApplication::platformName().startsWith("wayland")) {
        const auto full = screen()->geometry();
        LayerShellQt::Window::get(this)->setMargins(QMargins(
            area.left() - full.left(), 0, full.right() - area.right(), full.bottom() - area.bottom()));
    }
#endif
    setGeometry(area.x(), area.bottom() - 179, area.width(), 180);
}
