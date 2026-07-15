/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwCaptureLeads.h"
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
#include <QtGlobal>

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
    : cwCaptureLabelItem(parent)
    , m_glyphPen(ForegroundColor)
    , m_textMaxWidth(TextMaxWidth)
{
    m_glyphPen.setWidthF(PenWidth);
    m_glyphFont.setPointSizeF(FontPointSize);
    m_glyphFont.setBold(true);
    m_labelPen.setColor(ForegroundColor);
    m_labelFont.setPointSizeF(FontPointSize);
}

void cwCaptureLeads::setRegion(cwCavingRegion* region)
{
    if(m_region.data() == region) {
        return;
    }
    m_region = region;
    rebuildGeometry();
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

QVector<QPointF> cwCaptureLeads::leadMarkerPositions() const
{
    QVector<QPointF> positions;
    positions.reserve(m_leads.size());
    for(const auto& lead : m_leads) {
        positions.append(lead.anchor);
    }
    return positions;
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
        if(!m_boundingRect.contains(lead.anchor)) {
            continue;
        }

        painter->setPen(m_glyphPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(lead.anchor, radius, radius);

        const QRectF glyphRect(lead.anchor.x() - radius,
                               lead.anchor.y() - radius,
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
    clearRequestIndex();

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
                        entry.anchor = projectToLocal(leadPositions.at(i));
                        entry.text = formatLeadText(lead);
                        entry.hasLeader = false;
                        draws.append(entry);
                    }
                }
            }
        }
    }

    std::sort(draws.begin(), draws.end(), anchorOrder);

    m_leads = draws;
    update();
}

QVector<cwCaptureLabelPlacer::LabelRequest> cwCaptureLeads::buildLabelRequests(
    const cwLabelPlacementControl& control,
    const cwCaptureLabelPlacer::PlacementViewport& viewport)
{
    // placeAll registers each accepted leader as a hard line obstacle so
    // every later label (remaining leads + all stations, in any DT window)
    // avoids sitting on the drawn line. Leaders shorter than
    // MinLeaderLengthPaperPx aren't drawn, so they aren't registered.
    return buildRequests(m_leads, control, viewport,
                         cwCaptureLeadLines::LeaderPenWidthPaperPx * m_paperPxToLocal,
                         MinLeaderLengthPaperPx);
}

void cwCaptureLeads::applyPlacements(
    const QVector<cwCaptureLabelPlacer::Placement>& placements)
{
    applyPlacementsTo(m_leads, placements);
}
