/**************************************************************************
**
**    Copyright (C) 2025
**
**************************************************************************/

// Our includes
#include "cwCaptureCenterline.h"
#include "cwCamera.h"
#include "cwCaptureLabelPlacer.h"

// Qt includes
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QtGlobal>
#include <QtMath>

// Std includes
#include <algorithm>

namespace {
const QColor LineColor(200, 200, 200);
const QColor ForegroundColor(20, 20, 20);
constexpr qreal LabelFontPointSize = 8.0;
constexpr qreal BaseStationRadius = 2.0;
}

cwCaptureCenterline::cwCaptureCenterline(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_camera(nullptr)
    , m_linePen(LineColor)
    , m_stationPen(ForegroundColor)
    , m_stationBrush(ForegroundColor)
    , m_labelPen(ForegroundColor)
    , m_imageScale(1.0)
    , m_baseStationRadius(BaseStationRadius)
{
    m_linePen.setWidthF(cwCaptureCenterline::LinePenWidthPaperPx);
    m_stationPen.setWidthF(cwCaptureCenterline::LinePenWidthPaperPx);
    m_labelFont.setPointSizeF(LabelFontPointSize);
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

void cwCaptureCenterline::setExportDpi(int dpi)
{
    m_exportDpi = qMax(1, dpi);
}

void cwCaptureCenterline::setPlacer(cwCaptureLabelPlacer* placer)
{
    m_placer = placer;
}

void cwCaptureCenterline::setPaperPxToLocal(double scale)
{
    m_paperPxToLocal = qMax(0.0, scale);
}

qreal cwCaptureCenterline::stationDotRadius() const
{
    return m_baseStationRadius * m_paperPxToLocal;
}

QVector<QPointF> cwCaptureCenterline::stationPositions() const
{
    QVector<QPointF> positions;
    positions.reserve(m_stationData.size());
    for(const auto& station : m_stationData) {
        positions.append(station.position);
    }
    return positions;
}

QFont cwCaptureCenterline::scaledLabelFont() const
{
    return cwCaptureLabelPlacer::scaledFont(m_labelFont, m_exportDpi);
}

QVector<QPair<QString, QRectF>> cwCaptureCenterline::placedLabels() const
{
    QVector<QPair<QString, QRectF>> labels;
    labels.reserve(m_stationData.size());
    for(const auto& station : m_stationData) {
        if(!station.labelRect.isEmpty()) {
            labels.append({station.name, station.labelRect});
        }
    }
    return labels;
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

    const qreal stationRadius = stationDotRadius();

    painter->setPen(m_stationPen);
    painter->setBrush(m_stationBrush);
    for(const auto& station : std::as_const(m_stationData)) {
        if(!m_boundingRect.contains(station.position)) {
            continue;
        }
        painter->drawEllipse(station.position, stationRadius, stationRadius);
    }

    painter->setPen(m_labelPen);
    const QFont renderFont = scaledLabelFont();
    painter->setFont(renderFont);
    const QFontMetricsF paintMetrics(renderFont);
    for(const auto& station : std::as_const(m_stationData)) {
        if(!m_boundingRect.contains(station.position)) {
            continue;
        }
        if(station.labelRect.isEmpty()) {
            continue;
        }
        // The placer reserved a rect tightly sized to glyph ink; draw at the
        // baseline-left point that puts the painter's own tight ink rect at
        // labelRect's top-left.
        const QRectF tight = paintMetrics.tightBoundingRect(station.name);
        painter->drawText(
            cwCaptureLabelPlacer::baselineForGlyphInkRect(station.labelRect, tight),
            station.name);
    }

    painter->restore();
}

void cwCaptureCenterline::rebuildGeometry()
{
    m_lines.clear();
    m_stationData.clear();

    if(m_camera == nullptr
       || m_viewport.width() <= 0 || m_viewport.height() <= 0
       || m_network.isEmpty()) {
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
        m_stationData.append({station, localPoint, QRectF()});
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

void cwCaptureCenterline::placeStationLabels()
{
    if(m_placer == nullptr || m_stationData.isEmpty()) {
        return;
    }

    // Use the same scaled font for placement that paint() uses, so the
    // placer's reserved rect matches the painter's rendered glyph rect.
    const QFont placementFont = scaledLabelFont();

    // Stable order: top-to-bottom, left-to-right. Deterministic placements
    // across rebuilds and across exports.
    std::sort(m_stationData.begin(), m_stationData.end(),
              [](const StationDrawData& a, const StationDrawData& b) {
                  if(a.position.y() != b.position.y()) {
                      return a.position.y() < b.position.y();
                  }
                  return a.position.x() < b.position.x();
              });

    // Note: station dots are seeded into the placer's obstacle set by
    // cwCaptureViewport before the placer is finalized, so this method does
    // NOT call addObstacleRect or finalize.

    int placed = 0;
    int dropped = 0;
    for(StationDrawData& station : m_stationData) {
        if(station.name.isEmpty()) {
            continue;
        }

        QPainterPath path;
        path.addText(QPointF(0.0, 0.0), placementFont, station.name);
        const QRectF tightInk = path.boundingRect();
        if(tightInk.isEmpty()) {
            continue;
        }

        cwCaptureLabelPlacer::LabelRequest req{
            station.name,
            station.position,
            tightInk.size()
        };
        const cwCaptureLabelPlacer::Placement p = m_placer->placeLabel(req);
        if(p.placed) {
            // Painter draws at labelRect.topLeft() with AlignLeft|AlignTop;
            // its glyph baseline lands at top + ascent. Adjust the rect so
            // that the rect's TOP is the glyph's ink top (not baseline-
            // ascent). Specifically: placer returned a rect tightly sized to
            // glyph ink; that already matches what the painter renders.
            station.labelRect = p.labelRect;
            placed++;
        } else {
            dropped++;
        }
    }

    qDebug() << "[centerline-labels] stations=" << m_stationData.size()
             << "placed=" << placed
             << "dropped=" << dropped;
}
