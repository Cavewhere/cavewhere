/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAPTURELEADS_H
#define CWCAPTURELEADS_H

// Qt includes
#include <QBrush>
#include <QFont>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QPen>
#include <QPointer>
#include <QString>
#include <QVector>

// Our includes
#include "cwGlobals.h"

class cwCamera;
class cwCavingRegion;

class CAVEWHERE_LIB_EXPORT cwCaptureLeads : public QGraphicsItem
{
public:
    explicit cwCaptureLeads(QGraphicsItem* parent = nullptr);

    void setRegion(cwCavingRegion* region);
    void setCamera(cwCamera* camera);
    void setViewport(const QRect& viewport);
    void setImageScale(double scale);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    struct LeadDrawData {
        QPointF position;
        QString text;
    };

    void rebuildGeometry();

    QPointer<cwCavingRegion> m_region;
    cwCamera* m_camera;
    QRect m_viewport;
    QRectF m_boundingRect;
    qreal m_imageScale;
    QVector<LeadDrawData> m_leads;
    QPen m_glyphPen;
    QBrush m_glyphBrush;
    QFont m_glyphFont;
    QPen m_labelPen;
    QFont m_labelFont;
    qreal m_textOffsetX;
    qreal m_textMaxWidth;
};

#endif // CWCAPTURELEADS_H
