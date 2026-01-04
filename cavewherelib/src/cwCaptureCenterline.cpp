/**************************************************************************
**
**    Copyright (C) 2025
**
**************************************************************************/

// Our includes
#include "cwCaptureCenterline.h"
#include "cwCamera.h"

// Qt includes
#include <QPainter>
#include <QtGlobal>

cwCaptureCenterline::cwCaptureCenterline(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_camera(nullptr)
    , m_linePen(QColor(200, 200, 200))
    , m_stationPen(QColor(20, 20, 20))
    , m_stationBrush(QColor(20, 20, 20))
    , m_labelPen(QColor(20, 20, 20))
    , m_imageScale(1.0)
    , m_baseStationRadius(2.0)
    , m_baseLabelOffset(4.0, -4.0)
{
    m_linePen.setWidthF(1.0);
    m_stationPen.setWidthF(1.0);
    m_labelFont.setPointSizeF(8.0);
    setFlag(QGraphicsItem::ItemClipsToShape, true);
}

void cwCaptureCenterline::setNetwork(const cwSurveyNetwork& network)
{
    if(m_network == network) {
        return;
    }

    m_network = network;
    rebuildGeometry();
}

void cwCaptureCenterline::setCamera(cwCamera* camera)
{
    if(m_camera == camera) {
        return;
    }

    m_camera = camera;
    rebuildGeometry();
}

void cwCaptureCenterline::setViewport(const QRect& viewport)
{
    if(m_viewport == viewport) {
        return;
    }

    prepareGeometryChange();
    m_viewport = viewport;
    m_boundingRect = QRectF(QPointF(0.0, 0.0), QSizeF(m_viewport.size()) * m_imageScale);
    rebuildGeometry();
}

void cwCaptureCenterline::setImageScale(double scale)
{
    if(qFuzzyCompare(m_imageScale, scale)) {
        return;
    }

    prepareGeometryChange();
    m_imageScale = scale;
    m_boundingRect = QRectF(QPointF(0.0, 0.0), QSizeF(m_viewport.size()) * m_imageScale);
    rebuildGeometry();
}

QRectF cwCaptureCenterline::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath cwCaptureCenterline::shape() const
{
    QPainterPath path;
    path.addRect(m_boundingRect);
    return path;
}

void cwCaptureCenterline::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if(m_lines.isEmpty() && m_stationData.isEmpty()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setClipRect(m_boundingRect);

    painter->setPen(m_linePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawLines(m_lines);

    const qreal stationRadius = m_baseStationRadius;
    const QPointF labelOffset = m_baseLabelOffset;

    painter->setPen(m_stationPen);
    painter->setBrush(m_stationBrush);
    for(const auto& station : std::as_const(m_stationData)) {
        if(!m_boundingRect.contains(station.position)) {
            continue;
        }
        painter->drawEllipse(station.position, stationRadius, stationRadius);
    }

    painter->setPen(m_labelPen);
    painter->setFont(m_labelFont);
    for(const auto& station : std::as_const(m_stationData)) {
        if(!m_boundingRect.contains(station.position)) {
            continue;
        }
        painter->drawText(station.position + labelOffset, station.name);
    }

    painter->restore();
}

void cwCaptureCenterline::rebuildGeometry()
{
    m_lines.clear();
    m_stationData.clear();

    if(m_camera == nullptr) {
        update();
        return;
    }

    if(m_viewport.width() <= 0 || m_viewport.height() <= 0) {
        update();
        return;
    }

    if(m_network.isEmpty()) {
        update();
        return;
    }

    const QStringList stationNames = m_network.stations();
    QHash<QString, QPointF> stationPoints;
    stationPoints.reserve(stationNames.size());

    for(const QString& station : stationNames) {
        if(!m_network.hasPosition(station)) {
            continue;
        }

        const QVector3D position3d = m_network.position(station);
        const QPointF projected = m_camera->project(position3d);
        const QPointF localPoint = (projected - m_viewport.topLeft()) * m_imageScale;

        stationPoints.insert(station, localPoint);
        m_stationData.append({station, localPoint});
    }

    for(const QString& station : stationNames) {
        auto stationIt = stationPoints.constFind(station);
        if(stationIt == stationPoints.constEnd()) {
            continue;
        }

        const QStringList neighbors = m_network.neighbors(station);
        for(const QString& neighbor : neighbors) {
            if(station.compare(neighbor) >= 0) {
                continue;
            }

            auto neighborIt = stationPoints.constFind(neighbor);
            if(neighborIt == stationPoints.constEnd()) {
                continue;
            }

            m_lines.append(QLineF(*stationIt, *neighborIt));
        }
    }

    update();
}
