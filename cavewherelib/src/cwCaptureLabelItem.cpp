/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwCaptureLabelItem.h"
#include "cwCamera.h"

cwCaptureLabelItem::cwCaptureLabelItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
    setFlag(QGraphicsItem::ItemClipsToShape, true);
}

void cwCaptureLabelItem::setCamera(cwCamera* camera)
{
    if(m_camera == camera) {
        return;
    }
    m_camera = camera;
    rebuildGeometry();
}

void cwCaptureLabelItem::setViewport(const QRect& viewport)
{
    if(m_viewport == viewport) {
        return;
    }
    prepareGeometryChange();
    m_viewport = viewport;
    m_boundingRect = QRectF(QPointF(0.0, 0.0), QSizeF(m_viewport.size()) * m_imageScale);
    rebuildGeometry();
}

void cwCaptureLabelItem::setImageScale(double scale)
{
    if(qFuzzyCompare(m_imageScale, scale)) {
        return;
    }
    prepareGeometryChange();
    m_imageScale = scale;
    m_boundingRect = QRectF(QPointF(0.0, 0.0), QSizeF(m_viewport.size()) * m_imageScale);
    rebuildGeometry();
}

void cwCaptureLabelItem::setExportDpi(int dpi)
{
    m_exportDpi = qMax(1, dpi);
}

void cwCaptureLabelItem::setPaperPxToLocal(double scale)
{
    m_paperPxToLocal = qMax(0.0, scale);
}

void cwCaptureLabelItem::setLabelFont(const QFont& font)
{
    if(m_labelFont == font) {
        return;
    }
    m_labelFont = font;
    update();
}

QRectF cwCaptureLabelItem::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath cwCaptureLabelItem::shape() const
{
    QPainterPath path;
    path.addRect(m_boundingRect);
    return path;
}

QFont cwCaptureLabelItem::scaledLabelFont() const
{
    return cwCaptureLabelPlacer::scaledFont(m_labelFont, m_exportDpi);
}

QPointF cwCaptureLabelItem::projectToLocal(const QVector3D& world) const
{
    const QPointF projected = m_camera->project(world);
    return (projected - m_viewport.topLeft()) * m_imageScale;
}
