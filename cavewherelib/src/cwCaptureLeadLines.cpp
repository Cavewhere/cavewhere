/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwCaptureLeadLines.h"
#include "cwCaptureLeads.h"

// Qt includes
#include <QPainter>

namespace {
const QColor LeaderColor(80, 80, 80);
}

cwCaptureLeadLines::cwCaptureLeadLines(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_leads(nullptr)
    , m_leaderPen(LeaderColor)
{
    m_leaderPen.setWidthF(LeaderPenWidthPaperPx);
    m_leaderPen.setCapStyle(Qt::RoundCap);
    setFlag(QGraphicsItem::ItemClipsToShape, true);
}

void cwCaptureLeadLines::setLeads(const cwCaptureLeads* leads)
{
    m_leads = leads;
    update();
}

void cwCaptureLeadLines::setBoundingRect(const QRectF& rect)
{
    if(m_boundingRect == rect) {
        return;
    }
    prepareGeometryChange();
    m_boundingRect = rect;
}

QRectF cwCaptureLeadLines::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath cwCaptureLeadLines::shape() const
{
    QPainterPath path;
    path.addRect(m_boundingRect);
    return path;
}

void cwCaptureLeadLines::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if(m_leads == nullptr) {
        return;
    }

    const auto& layout = m_leads->layout();
    if(layout.isEmpty()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setClipRect(m_boundingRect);
    painter->setPen(m_leaderPen);
    painter->setBrush(Qt::NoBrush);

    for(const auto& lead : layout) {
        if(!lead.hasLeader) {
            continue;
        }
        painter->drawLine(QLineF(lead.leaderStart, lead.leaderEnd));
    }

    painter->restore();
}
