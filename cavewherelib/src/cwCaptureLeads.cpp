/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwCaptureLeads.h"
#include "cwCamera.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwLead.h"
#include "cwCollisionRectKdTree.h"

// Qt includes
#include <QFontMetricsF>
#include <QPainter>
#include <QtGlobal>
#include <QtMath>

// Std includes
#include <algorithm>

namespace {
const QColor ForegroundColor(20, 20, 20);
const QColor BackgroundColor(255, 255, 255);
constexpr qreal PenWidth = 1.0;
constexpr qreal FontPointSize = 8.0;
constexpr qreal GlyphPadding = 1.5;
constexpr qreal TextMaxWidth = 120.0;

constexpr qreal ObstacleSampleSpacing = 6.0;
constexpr qreal ObstacleSampleHalfSize = 2.0;
constexpr qreal ObstacleStationHalfSize = 4.0;
constexpr qreal LabelCollisionPadding = 3.0;
constexpr qreal MinLeaderLength = 4.0;

constexpr qreal CandidateDistanceMultipliers[3] = {1.5, 3.0, 5.0};

QPointF clampPointToRect(const QRectF& r, const QPointF& p) {
    return QPointF(qBound(r.left(), p.x(), r.right()),
                   qBound(r.top(), p.y(), r.bottom()));
}

// Place a label rect so its edge nearest `anchor` sits at `anchor`,
// with the rect extending in the direction (dx, dy).
QRectF anchorLabelRect(const QPointF& anchor, qreal dx, qreal dy, const QSizeF& size) {
    constexpr qreal eps = 1e-3;
    qreal x;
    if(dx > eps)        x = anchor.x();
    else if(dx < -eps)  x = anchor.x() - size.width();
    else                x = anchor.x() - size.width() * 0.5;

    qreal y;
    if(dy > eps)        y = anchor.y();
    else if(dy < -eps)  y = anchor.y() - size.height();
    else                y = anchor.y() - size.height() * 0.5;

    return QRectF(QPointF(x, y), size);
}
}

cwCaptureLeads::cwCaptureLeads(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_camera(nullptr)
    , m_imageScale(1.0)
    , m_glyphPen(ForegroundColor)
    , m_glyphBrush(BackgroundColor)
    , m_labelPen(ForegroundColor)
    , m_textMaxWidth(TextMaxWidth)
{
    m_glyphPen.setWidthF(PenWidth);
    m_glyphFont.setPointSizeF(FontPointSize);
    m_glyphFont.setBold(true);
    m_labelFont.setPointSizeF(FontPointSize);
    setFlag(QGraphicsItem::ItemClipsToShape, true);
}

void cwCaptureLeads::setRegion(cwCavingRegion* region)
{
    if(m_region.data() == region) {
        return;
    }
    m_region = region;
    rebuildGeometry();
}

void cwCaptureLeads::setCamera(cwCamera* camera)
{
    if(m_camera == camera) {
        return;
    }
    m_camera = camera;
    rebuildGeometry();
}

void cwCaptureLeads::setViewport(const QRect& viewport)
{
    if(m_viewport == viewport) {
        return;
    }
    prepareGeometryChange();
    m_viewport = viewport;
    m_boundingRect = QRectF(QPointF(0.0, 0.0), QSizeF(m_viewport.size()) * m_imageScale);
    rebuildGeometry();
}

void cwCaptureLeads::setImageScale(double scale)
{
    if(qFuzzyCompare(m_imageScale, scale)) {
        return;
    }
    prepareGeometryChange();
    m_imageScale = scale;
    m_boundingRect = QRectF(QPointF(0.0, 0.0), QSizeF(m_viewport.size()) * m_imageScale);
    rebuildGeometry();
}

void cwCaptureLeads::setNetwork(const cwSurveyNetwork& network)
{
    if(m_network == network) {
        return;
    }
    m_network = network;
    rebuildGeometry();
}

QRectF cwCaptureLeads::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath cwCaptureLeads::shape() const
{
    QPainterPath path;
    path.addRect(m_boundingRect);
    return path;
}

void cwCaptureLeads::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if(m_leads.isEmpty()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setClipRect(m_boundingRect);

    // Derive the marker radius from the rendered "?" so the disk encloses
    // the glyph at any export resolution. The font is rendered at a fixed
    // physical size while the ellipse is in painter-local coords; without
    // this, SVG export (which preserves real text geometry) draws the
    // glyph far larger than a hard-coded radius would imply.
    painter->setFont(m_glyphFont);
    const QFontMetricsF glyphMetrics(painter->font(), painter->device());
    const QRectF glyphInk = glyphMetrics.tightBoundingRect(QStringLiteral("?"));
    const qreal radius = qMax(glyphInk.width(), glyphInk.height()) * 0.5 + GlyphPadding;

    // Markers (white-filled circles with a "?" centered)
    for(const auto& lead : std::as_const(m_leads)) {
        if(!m_boundingRect.contains(lead.markerPos)) {
            continue;
        }

        painter->setPen(m_glyphPen);
        painter->setBrush(m_glyphBrush);
        painter->drawEllipse(lead.markerPos, radius, radius);

        const QRectF glyphRect(lead.markerPos.x() - radius,
                               lead.markerPos.y() - radius,
                               radius * 2.0,
                               radius * 2.0);
        painter->drawText(glyphRect, Qt::AlignCenter, QStringLiteral("?"));
    }

    // Description / size labels at their auto-placed positions
    painter->setPen(m_labelPen);
    painter->setFont(m_labelFont);
    for(const auto& lead : std::as_const(m_leads)) {
        if(lead.text.isEmpty()) {
            continue;
        }
        if(lead.labelRect.isEmpty()) {
            continue;
        }

        painter->drawText(lead.labelRect,
                          Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                          lead.text);
    }

    painter->restore();
}

void cwCaptureLeads::rebuildGeometry()
{
    m_leads.clear();

    if(m_camera == nullptr) {
        update();
        return;
    }

    if(m_viewport.width() <= 0 || m_viewport.height() <= 0) {
        update();
        return;
    }

    if(m_region.isNull()) {
        update();
        return;
    }

    auto formatLeadText = [](const cwLead& lead) -> QString {
        QString description = lead.desciption();
        const QSizeF size = lead.size();
        const bool hasDimensions = size.isValid()
                                   && size.width() > 0.0
                                   && size.height() > 0.0;

        if(!hasDimensions) {
            return description;
        }

        const QString sizeStr = QStringLiteral("%1 x %2")
                                    .arg(size.width())
                                    .arg(size.height());

        if(description.isEmpty()) {
            return QStringLiteral("(") + sizeStr + QStringLiteral(")");
        }
        return description + QStringLiteral(" (") + sizeStr + QStringLiteral(")");
    };

    auto projectToPaper = [&](const QVector3D& world) -> QPointF {
        const QPointF projected = m_camera->project(world);
        return (projected - m_viewport.topLeft()) * m_imageScale;
    };

    // 1. Collect (markerPos, text) for every uncompleted lead with a position.
    QVector<LeadDrawData> draws;
    const QList<cwCave*> caves = m_region->caves();
    for(cwCave* cave : caves) {
        if(cave == nullptr) {
            continue;
        }

        const QList<cwTrip*> trips = cave->trips();
        for(cwTrip* trip : trips) {
            if(trip == nullptr || trip->notes() == nullptr) {
                continue;
            }

            const QList<cwNote*> notes = trip->notes()->notes();
            for(cwNote* note : notes) {
                if(note == nullptr) {
                    continue;
                }

                const QList<cwScrap*> scraps = note->scraps();
                for(cwScrap* scrap : scraps) {
                    if(scrap == nullptr) {
                        continue;
                    }

                    const QList<cwLead> leads = scrap->leads();
                    const QVector<QVector3D> leadPositions = scrap->leadPositions();
                    const int leadCount = leads.size();

                    for(int i = 0; i < leadCount; i++) {
                        const cwLead& lead = leads.at(i);
                        if(lead.completed()) {
                            continue;
                        }

                        if(i >= leadPositions.size()) {
                            continue;
                        }

                        LeadDrawData entry;
                        entry.markerPos = projectToPaper(leadPositions.at(i));
                        entry.text = formatLeadText(lead);
                        entry.hasLeader = false;
                        draws.append(entry);
                    }
                }
            }
        }
    }

    if(draws.isEmpty()) {
        update();
        return;
    }

    // 2. Build the obstacle kd-tree from the centerline (sampled shots + station rects).
    cwCollisionRectKdTree obstacles;

    const QStringList stations = m_network.stations();
    QHash<QString, QPointF> stationPaperPos;
    stationPaperPos.reserve(stations.size());
    for(const QString& s : stations) {
        if(!m_network.hasPosition(s)) {
            continue;
        }
        const QPointF p = projectToPaper(m_network.position(s));
        stationPaperPos.insert(s, p);

        QRectF stationRect(p.x() - ObstacleStationHalfSize,
                           p.y() - ObstacleStationHalfSize,
                           ObstacleStationHalfSize * 2.0,
                           ObstacleStationHalfSize * 2.0);
        obstacles.addRect(stationRect.toAlignedRect());
    }

    for(const QString& s : stations) {
        const auto sIt = stationPaperPos.constFind(s);
        if(sIt == stationPaperPos.constEnd()) {
            continue;
        }
        const QPointF p1 = *sIt;

        const QStringList neighbors = m_network.neighbors(s);
        for(const QString& n : neighbors) {
            // Process each shot from the alphabetically-lower endpoint to avoid duplicates.
            if(s.compare(n) >= 0) {
                continue;
            }
            const auto nIt = stationPaperPos.constFind(n);
            if(nIt == stationPaperPos.constEnd()) {
                continue;
            }
            const QPointF p2 = *nIt;

            const qreal length = QLineF(p1, p2).length();
            if(length <= 0.0) {
                continue;
            }
            const int samples = qMax(1, qFloor(length / ObstacleSampleSpacing));
            for(int k = 0; k <= samples; k++) {
                const qreal t = (samples > 0) ? (qreal(k) / qreal(samples)) : 0.0;
                const QPointF sample = p1 + (p2 - p1) * t;
                QRectF sampleRect(sample.x() - ObstacleSampleHalfSize,
                                  sample.y() - ObstacleSampleHalfSize,
                                  ObstacleSampleHalfSize * 2.0,
                                  ObstacleSampleHalfSize * 2.0);
                obstacles.addRect(sampleRect.toAlignedRect());
            }
        }
    }

    // 3. Estimate marker radius using non-device font metrics so placement
    //    geometry matches the painter's marker reasonably well at the eventual
    //    paint-time device. Small visual mismatch on high-DPI vector exports
    //    is acceptable for v1.
    const QFontMetricsF placementGlyphMetrics(m_glyphFont);
    const QRectF placementGlyphInk = placementGlyphMetrics.tightBoundingRect(QStringLiteral("?"));
    const qreal placementRadius = qMax(placementGlyphInk.width(),
                                       placementGlyphInk.height()) * 0.5 + GlyphPadding;

    const QFontMetricsF labelMetrics(m_labelFont);
    const QRectF wrapBox(0.0, 0.0, m_textMaxWidth, m_textMaxWidth * 4.0);

    // 4. Generate the 8-direction unit vectors (Y down: north is -y).
    const qreal diag = qSqrt(2.0) * 0.5;
    struct DirVec { qreal dx, dy; };
    const DirVec directions[8] = {
        { 1.0,  0.0},  // E
        { diag, -diag},// NE
        { diag,  diag},// SE
        { 0.0, -1.0},  // N
        { 0.0,  1.0},  // S
        {-diag, -diag},// NW
        {-diag,  diag},// SW
        {-1.0,  0.0}   // W
    };

    // 5. Stable iteration order: top-to-bottom, then left-to-right.
    std::sort(draws.begin(), draws.end(),
              [](const LeadDrawData& a, const LeadDrawData& b) {
                  if(a.markerPos.y() != b.markerPos.y()) {
                      return a.markerPos.y() < b.markerPos.y();
                  }
                  return a.markerPos.x() < b.markerPos.x();
              });

    // 6. Place each lead's label.
    m_leads.reserve(draws.size());
    for(LeadDrawData& entry : draws) {
        QSizeF textSize;
        if(!entry.text.isEmpty()) {
            const QRectF tight = labelMetrics.boundingRect(
                wrapBox, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, entry.text);
            textSize = tight.size();
        }

        if(textSize.width() <= 0.0 || textSize.height() <= 0.0) {
            // No text to place (or empty) — store the entry without a labelRect.
            m_leads.append(entry);
            continue;
        }

        const QSizeF collisionSize(textSize.width() + LabelCollisionPadding * 2.0,
                                   textSize.height() + LabelCollisionPadding * 2.0);

        QRectF chosenLabel;
        bool placed = false;

        for(qreal multiplier : CandidateDistanceMultipliers) {
            const qreal distance = placementRadius * multiplier;
            for(const DirVec& dir : directions) {
                const QPointF anchor(entry.markerPos.x() + dir.dx * distance,
                                     entry.markerPos.y() + dir.dy * distance);

                QRectF candidate = anchorLabelRect(anchor, dir.dx, dir.dy, textSize);
                QRectF collision = anchorLabelRect(anchor, dir.dx, dir.dy, collisionSize);
                // Center the collision rect on the candidate rect (extra padding all around).
                collision.moveCenter(candidate.center());

                if(obstacles.addRect(collision.toAlignedRect())) {
                    chosenLabel = candidate;
                    placed = true;
                    break;
                }
            }
            if(placed) {
                break;
            }
        }

        if(!placed) {
            // Fallback: rightward default at the snug distance, accept overlap.
            const qreal distance = placementRadius * CandidateDistanceMultipliers[0];
            const DirVec defaultDir = directions[0]; // E
            const QPointF anchor(entry.markerPos.x() + defaultDir.dx * distance,
                                 entry.markerPos.y() + defaultDir.dy * distance);
            QRectF candidate = anchorLabelRect(anchor, defaultDir.dx, defaultDir.dy, textSize);
            QRectF collision = anchorLabelRect(anchor, defaultDir.dx, defaultDir.dy, collisionSize);
            collision.moveCenter(candidate.center());
            obstacles.addRect(collision.toAlignedRect()); // best-effort, ignore result
            chosenLabel = candidate;
        }

        entry.labelRect = chosenLabel;
        entry.leaderStart = entry.markerPos;
        entry.leaderEnd = clampPointToRect(chosenLabel, entry.markerPos);
        const qreal leaderLength = QLineF(entry.leaderStart, entry.leaderEnd).length();
        entry.hasLeader = leaderLength >= MinLeaderLength;

        m_leads.append(entry);
    }

    update();
}
