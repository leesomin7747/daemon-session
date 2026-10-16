#pragma once
#include "Session.h"

#include <QQuickView>
#include <QRectF>
#include <QVector>
#include <QPointer>

class OverlayWindow : public QQuickView
{
    Q_OBJECT

public:
    explicit OverlayWindow(QScreen* screen, QObject* parent = nullptr);
    Q_INVOKABLE void setInteractiveRegions(const QVector<QRectF>& regions);
    Q_INVOKABLE void setEditorActive(bool active);
    Q_INVOKABLE double petX(double x, double petWidth, double margin) const;
    Q_INVOKABLE void setContentHeight(int height);

private:
    QVector<QRectF> m_regions;
    QPointer<QWindow> m_previousWindow;
    bool m_editorActive = false;
    int m_contentHeight = 180;
    void updateInputRegion();
    void updatePlacement();
};
