/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAPTURELEADLINES_H
#define CWCAPTURELEADLINES_H

// Qt includes
#include <QGraphicsItem>
#include <QPainterPath>
#include <QPen>
#include <QRectF>

// Our includes
#include "cwGlobals.h"

class cwCaptureLeads;

// Sibling of cwCaptureLeads. Renders just the leader-line segments
// connecting each marker to its auto-placed label, sitting at a low
// z-value so transparent regions of the captured tile let the lines
// show through while opaque cave-passage ink hides them.
class CAVEWHERE_LIB_EXPORT cwCaptureLeadLines : public QGraphicsItem
{
public:
    explicit cwCaptureLeadLines(QGraphicsItem* parent = nullptr);

    void setLeads(const cwCaptureLeads* leads);
    void setBoundingRect(const QRectF& rect);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    const cwCaptureLeads* m_leads;
    QRectF m_boundingRect;
    QPen   m_leaderPen;
};

#endif // CWCAPTURELEADLINES_H
