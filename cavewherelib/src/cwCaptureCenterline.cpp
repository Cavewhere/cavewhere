/**************************************************************************
**
**    Copyright (C) 2025
**
**************************************************************************/

// Our includes
#include "cwCaptureCenterline.h"
#include "cwCaptureLabelPlacer.h"

// Qt includes
#include <QFontMetricsF>
#include <QPainter>
#include <QtGlobal>

// Std includes
#include <algorithm>

namespace {
const QColor LineColor(200, 200, 200);
const QColor ForegroundColor(20, 20, 20);
constexpr qreal LabelFontPointSize = 8.0;
constexpr qreal BaseStationRadius = 2.0;
}

cwCaptureCenterline::cwCaptureCenterline(QGraphicsItem* parent)
    : cwCaptureLabelItem(parent)
    , m_linePen(LineColor)
    , m_stationPen(ForegroundColor)
    , m_stationBrush(ForegroundColor)
    , m_baseStationRadius(BaseStationRadius)
{
    m_linePen.setWidthF(cwCaptureCenterline::LinePenWidthPaperPx);
    m_stationPen.setWidthF(cwCaptureCenterline::LinePenWidthPaperPx);
    m_labelPen.setColor(ForegroundColor);
    m_labelFont.setPointSizeF(LabelFontPointSize);
}

void cwCaptureCenterline::setNetwork(const cwSurveyNetwork& network)
{
    if(m_network == network) {
        return;
    }
    m_network = network;
    rebuildGeometry();
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
        positions.append(station.anchor);
    }
    return positions;
}

QVector<QPair<QString, QRectF>> cwCaptureCenterline::placedLabels() const
{
    QVector<QPair<QString, QRectF>> labels;
    labels.reserve(m_stationData.size());
    for(const auto& station : m_stationData) {
        if(!station.labelRect.isEmpty()) {
            labels.append({station.text, station.labelRect});
        }
    }
    return labels;
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
        if(!m_boundingRect.contains(station.anchor)) {
            continue;
        }
        painter->drawEllipse(station.anchor, stationRadius, stationRadius);
    }

    painter->setPen(m_labelPen);
    const QFont renderFont = scaledLabelFont();
    painter->setFont(renderFont);
    const QFontMetricsF paintMetrics(renderFont);
    for(const auto& station : std::as_const(m_stationData)) {
        if(!m_boundingRect.contains(station.anchor)) {
            continue;
        }
        if(station.labelRect.isEmpty()) {
            continue;
        }
        // The placer reserved a rect tightly sized to glyph ink; draw at the
        // baseline-left point that puts the painter's own tight ink rect at
        // labelRect's top-left.
        const QRectF tight = paintMetrics.tightBoundingRect(station.text);
        painter->drawText(
            cwCaptureLabelPlacer::baselineForGlyphInkRect(station.labelRect, tight),
            station.text);
    }

    painter->restore();
}

void cwCaptureCenterline::rebuildGeometry()
{
    m_lines.clear();
    m_stationData.clear();
    clearRequestIndex();

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

        const QPointF localPoint = projectToLocal(m_network.position(station));
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

    std::sort(m_stationData.begin(), m_stationData.end(), anchorOrder);

    update();
}

QVector<cwCaptureLabelPlacer::LabelRequest> cwCaptureCenterline::buildLabelRequests(
    const cwLabelPlacementControl& control,
    const cwCaptureLabelPlacer::PlacementViewport& viewport)
{
    // Note: station dots are seeded into the placer's obstacle set by
    // cwCaptureViewport before the placer is finalized, so this method does
    // NOT call addObstacleRect or finalize.
    return buildRequests(m_stationData, control, viewport);
}

void cwCaptureCenterline::applyPlacements(
    const QVector<cwCaptureLabelPlacer::Placement>& placements)
{
    applyPlacementsTo(m_stationData, placements);
}
