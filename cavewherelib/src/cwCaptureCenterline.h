/**************************************************************************
**
**    Copyright (C) 2025
**
**************************************************************************/

#ifndef CWCAPTURECENTERLINE_H
#define CWCAPTURECENTERLINE_H

// Qt includes
#include <QBrush>
#include <QFont>
#include <QGraphicsItem>
#include <QHash>
#include <QPainterPath>
#include <QPen>
#include <QVector>

// Our includes
#include "cwGlobals.h"
#include "cwSurveyNetwork.h"

class cwCamera;

class CAVEWHERE_LIB_EXPORT cwCaptureCenterline : public QGraphicsItem
{
public:
    explicit cwCaptureCenterline(QGraphicsItem* parent = nullptr);

    void setNetwork(const cwSurveyNetwork& network);
    void setCamera(cwCamera* camera);
    void setViewport(const QRect& viewport);
    void setImageScale(double scale);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    struct StationDrawData {
        QString name;
        QPointF position;
    };

    void rebuildGeometry();

    cwSurveyNetwork m_network;
    cwCamera* m_camera;
    QRect m_viewport;
    QRectF m_boundingRect;
    qreal m_imageScale;
    QVector<QLineF> m_lines;
    QVector<StationDrawData> m_stationData;
    QPen m_linePen;
    QPen m_stationPen;
    QBrush m_stationBrush;
    QPen m_labelPen;
    QFont m_labelFont;
    qreal m_baseStationRadius;
    QPointF m_baseLabelOffset;
};

#endif // CWCAPTURECENTERLINE_H
