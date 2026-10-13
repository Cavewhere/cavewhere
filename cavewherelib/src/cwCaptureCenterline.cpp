/**************************************************************************
**
**    Copyright (C) 2025
**
**************************************************************************/

// Our includes
#include "cwCaptureCenterline.h"
#include "cwCaptureLabelPlacer.h"
#include "cwCave.h"
#include "cwCavingRegion.h"

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

QList<cwSurveyNetwork> cwCaptureCenterline::caveNetworks(const cwCavingRegion* region)
{
    if(region == nullptr) {
        return {};
    }

    QList<cwSurveyNetwork> networks;
    const QList<cwCave*> caves = region->caves();
    networks.reserve(caves.size());
    for(const cwCave* cave : caves) {
        if(cave == nullptr) {
            continue;
        }

        cwSurveyNetwork network = cave->network();
        const cwStationPositionLookup stationLookup = cave->stationPositionLookup();
        const QStringList stations = network.stations();
        for(const QString& station : stations) {
            if(stationLookup.hasPosition(station)) {
                network.setPosition(station, stationLookup.position(station));
            }
        }

        networks.append(network);
    }

    return networks;
}

void cwCaptureCenterline::setNetworks(const QList<cwSurveyNetwork>& networks)
{
    // cwSurveyNetwork::operator== compares topology only, so always rebuild to
    // pick up moved stations.
    m_networks = networks;
    rebuildGeometry();
}

void cwCaptureCenterline::setDotsVisible(bool visible)
{
    if(m_dotsVisible == visible) {
        return;
    }
    m_dotsVisible = visible;
    update();
}

void cwCaptureCenterline::setLegsVisible(bool visible)
{
    if(m_legsVisible == visible) {
        return;
    }
    m_legsVisible = visible;
    update();
}

void cwCaptureCenterline::setLabelsVisible(bool visible)
{
    if(m_labelsVisible == visible) {
        return;
    }
    m_labelsVisible = visible;
    update();
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

    if(m_legsVisible) {
        painter->setPen(m_linePen);
        painter->setBrush(Qt::NoBrush);
        painter->drawLines(m_lines);
    }

    if(m_dotsVisible) {
        const qreal stationRadius = stationDotRadius();

        painter->setPen(m_stationPen);
        painter->setBrush(m_stationBrush);
        for(const auto& station : std::as_const(m_stationData)) {
            if(!m_boundingRect.contains(station.anchor)) {
                continue;
            }
            painter->drawEllipse(station.anchor, stationRadius, stationRadius);
        }
    }

    if(m_labelsVisible) {
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
    }

    painter->restore();
}

void cwCaptureCenterline::rebuildGeometry()
{
    m_lines.clear();
    m_stationData.clear();
    clearRequestIndex();

    if(m_camera == nullptr
       || m_viewport.width() <= 0 || m_viewport.height() <= 0) {
        update();
        return;
    }

    // Each network is drawn on its own, so a station name shared by two caves
    // stays two stations.
    for(const cwSurveyNetwork& network : std::as_const(m_networks)) {
        const QStringList stationNames = network.stations();
        QHash<QString, QPointF> stationPoints;
        stationPoints.reserve(stationNames.size());

        for(const QString& station : stationNames) {
            if(!network.hasPosition(station)) {
                continue;
            }

            const QPointF localPoint = projectToLocal(network.position(station));
            stationPoints.insert(station, localPoint);
            m_stationData.append({station, localPoint, QRectF()});
        }

        for(const QString& station : stationNames) {
            auto stationIt = stationPoints.constFind(station);
            if(stationIt == stationPoints.constEnd()) {
                continue;
            }

            const QStringList neighbors = network.neighbors(station);
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
    }

    std::sort(m_stationData.begin(), m_stationData.end(), anchorOrder);

    update();
}

QVector<cwCaptureLabelPlacer::LabelRequest> cwCaptureCenterline::buildLabelRequests(
    const cwLabelPlacementControl& control,
    const cwCaptureLabelPlacer::PlacementViewport& viewport)
{
    if(m_labelsVisible) {
        // Note: station dots are seeded into the placer's obstacle set by
        // cwCaptureViewport before the placer is finalized, so this method does
        // NOT call addObstacleRect or finalize.
        return buildRequests(m_stationData, control, viewport);
    }

    // Hidden labels place nothing. Clear the index so the empty placement
    // slice applyPlacements receives matches it, and drop any earlier rects.
    clearRequestIndex();
    for(auto& station : m_stationData) {
        station.resetPlacement();
    }
    return {};
}

void cwCaptureCenterline::applyPlacements(
    const QVector<cwCaptureLabelPlacer::Placement>& placements)
{
    applyPlacementsTo(m_stationData, placements);
}
