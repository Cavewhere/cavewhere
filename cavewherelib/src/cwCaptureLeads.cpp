/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwCaptureLeads.h"
#include "cwCamera.h"
#include "cwCaptureLabelPlacer.h"
#include "cwCaptureLeadLines.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwLead.h"

// Qt includes
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QtGlobal>
#include <QtMath>

// Std includes
#include <algorithm>

namespace {
const QColor ForegroundColor(20, 20, 20);
constexpr qreal PenWidth = 1.0;
constexpr qreal FontPointSize = 8.0;
constexpr qreal GlyphPadding = 1.5;
constexpr qreal TextMaxWidth = 120.0;
constexpr qreal MinLeaderLengthPaperPx = 4.0;
}

cwCaptureLeads::cwCaptureLeads(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_camera(nullptr)
    , m_imageScale(1.0)
    , m_glyphPen(ForegroundColor)
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

void cwCaptureLeads::setExportDpi(int dpi)
{
    m_exportDpi = qMax(1, dpi);
}

void cwCaptureLeads::setPlacer(cwCaptureLabelPlacer* placer)
{
    m_placer = placer;
}

void cwCaptureLeads::setPaperPxToLocal(double scale)
{
    m_paperPxToLocal = qMax(0.0, scale);
}

qreal cwCaptureLeads::markerRadius() const
{
    const QFontMetricsF metrics(scaledGlyphFont());
    const QRectF ink = metrics.tightBoundingRect(QStringLiteral("?"));
    return qMax(ink.width(), ink.height()) * 0.5 + GlyphPadding * m_paperPxToLocal;
}

QFont cwCaptureLeads::scaledGlyphFont() const
{
    return cwCaptureLabelPlacer::scaledFont(m_glyphFont, m_exportDpi);
}

QFont cwCaptureLeads::scaledLabelFont() const
{
    return cwCaptureLabelPlacer::scaledFont(m_labelFont, m_exportDpi);
}

QVector<QPointF> cwCaptureLeads::leadMarkerPositions() const
{
    QVector<QPointF> positions;
    positions.reserve(m_leads.size());
    for(const auto& lead : m_leads) {
        positions.append(lead.markerPos);
    }
    return positions;
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

    const QFont glyphRenderFont = scaledGlyphFont();
    painter->setFont(glyphRenderFont);
    const qreal radius = markerRadius();

    for(const auto& lead : std::as_const(m_leads)) {
        if(!m_boundingRect.contains(lead.markerPos)) {
            continue;
        }

        painter->setPen(m_glyphPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(lead.markerPos, radius, radius);

        const QRectF glyphRect(lead.markerPos.x() - radius,
                               lead.markerPos.y() - radius,
                               radius * 2.0,
                               radius * 2.0);
        painter->drawText(glyphRect, Qt::AlignCenter, QStringLiteral("?"));
    }

    painter->setPen(m_labelPen);
    const QFont labelRenderFont = scaledLabelFont();
    painter->setFont(labelRenderFont);
    const QFontMetricsF labelPaintMetrics(labelRenderFont);
    for(const auto& lead : std::as_const(m_leads)) {
        if(lead.text.isEmpty() || lead.labelRect.isEmpty()) {
            continue;
        }

        const QRectF tight = labelPaintMetrics.tightBoundingRect(lead.text);
        painter->drawText(
            cwCaptureLabelPlacer::baselineForGlyphInkRect(lead.labelRect, tight),
            lead.text);
    }

    painter->restore();
}

void cwCaptureLeads::rebuildGeometry()
{
    m_leads.clear();

    if(m_camera == nullptr
       || m_viewport.width() <= 0 || m_viewport.height() <= 0
       || m_region.isNull()) {
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

    // Stable order: top-to-bottom, left-to-right.
    std::sort(draws.begin(), draws.end(),
              [](const LeadDrawData& a, const LeadDrawData& b) {
                  if(a.markerPos.y() != b.markerPos.y()) {
                      return a.markerPos.y() < b.markerPos.y();
                  }
                  return a.markerPos.x() < b.markerPos.x();
              });

    m_leads = draws;
    update();
}

void cwCaptureLeads::placeLeadLabels()
{
    if(m_placer == nullptr || m_leads.isEmpty()) {
        return;
    }

    // Use the same scaled font for placement that paint() uses, so the
    // placer's reserved rect matches the painter's rendered glyph rect.
    const QFont placementLabelFont = scaledLabelFont();

    int placed = 0;
    int dropped = 0;
    for(LeadDrawData& entry : m_leads) {
        if(entry.text.isEmpty()) {
            entry.labelRect = QRectF();
            entry.hasLeader = false;
            continue;
        }

        // Compute the tight glyph rect for the single-line label. Multi-line
        // wrapping would split into multiple <text> runs on the SVG side; for
        // a placer-driven layout we treat the label as one block of its full
        // single-line size and let viewBox-clamp drop labels that don't fit.
        QPainterPath path;
        path.addText(QPointF(0.0, 0.0), placementLabelFont, entry.text);
        const QRectF tightInk = path.boundingRect();
        if(tightInk.isEmpty()) {
            entry.labelRect = QRectF();
            entry.hasLeader = false;
            continue;
        }

        cwCaptureLabelPlacer::LabelRequest req{
            entry.text,
            entry.markerPos,
            tightInk.size()
        };
        const cwCaptureLabelPlacer::Placement p = m_placer->placeLabel(req);
        if(p.placed) {
            entry.labelRect = p.labelRect;
            entry.leaderStart = p.leaderStart;
            entry.leaderEnd = p.leaderEnd;
            const qreal leaderLength = QLineF(entry.leaderStart, entry.leaderEnd).length();
            entry.hasLeader = leaderLength >= MinLeaderLengthPaperPx;
            if(entry.hasLeader) {
                // Register the leader so subsequent label placements (later
                // leads + all stations) avoid sitting on the drawn line.
                m_placer->addLineObstacle(
                    QLineF(entry.leaderStart, entry.leaderEnd),
                    cwCaptureLeadLines::LeaderPenWidthPaperPx * m_paperPxToLocal);
            }
            placed++;
        } else {
            entry.labelRect = QRectF();
            entry.hasLeader = false;
            dropped++;
        }
    }

    qDebug() << "[leads-labels] leads=" << m_leads.size()
             << "placed=" << placed
             << "dropped=" << dropped;
}
