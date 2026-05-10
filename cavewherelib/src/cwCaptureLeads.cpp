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

// Qt includes
#include <QFontMetricsF>
#include <QPainter>
#include <QtGlobal>

namespace {
const QColor ForegroundColor(20, 20, 20);
const QColor BackgroundColor(255, 255, 255);
constexpr qreal PenWidth = 1.0;
constexpr qreal FontPointSize = 8.0;
constexpr qreal GlyphPadding = 1.5;
constexpr qreal TextOffsetX = 3.0;
constexpr qreal TextMaxWidth = 120.0;
}

cwCaptureLeads::cwCaptureLeads(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_camera(nullptr)
    , m_imageScale(1.0)
    , m_glyphPen(ForegroundColor)
    , m_glyphBrush(BackgroundColor)
    , m_labelPen(ForegroundColor)
    , m_textOffsetX(TextOffsetX)
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
        if(!m_boundingRect.contains(lead.position)) {
            continue;
        }

        painter->setPen(m_glyphPen);
        painter->setBrush(m_glyphBrush);
        painter->drawEllipse(lead.position, radius, radius);

        const QRectF glyphRect(lead.position.x() - radius,
                               lead.position.y() - radius,
                               radius * 2.0,
                               radius * 2.0);
        painter->drawText(glyphRect, Qt::AlignCenter, QStringLiteral("?"));
    }

    // Description / size labels next to each marker
    painter->setPen(m_labelPen);
    painter->setFont(m_labelFont);
    for(const auto& lead : std::as_const(m_leads)) {
        if(!m_boundingRect.contains(lead.position)) {
            continue;
        }
        if(lead.text.isEmpty()) {
            continue;
        }

        const QPointF textOrigin(lead.position.x() + radius + m_textOffsetX,
                                 lead.position.y() - m_textMaxWidth * 0.5);
        const QRectF textRect(textOrigin, QSizeF(m_textMaxWidth, m_textMaxWidth));
        painter->drawText(textRect,
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
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

                        const QVector3D position3d = leadPositions.at(i);
                        const QPointF projected = m_camera->project(position3d);
                        const QPointF localPoint = (projected - m_viewport.topLeft()) * m_imageScale;

                        m_leads.append({localPoint, formatLeadText(lead)});
                    }
                }
            }
        }
    }

    update();
}
