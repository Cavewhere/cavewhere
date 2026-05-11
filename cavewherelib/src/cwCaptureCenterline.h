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
class cwCaptureLabelPlacer;

class CAVEWHERE_LIB_EXPORT cwCaptureCenterline : public QGraphicsItem
{
public:
    explicit cwCaptureCenterline(QGraphicsItem* parent = nullptr);

    void setNetwork(const cwSurveyNetwork& network);
    void setCamera(cwCamera* camera);
    void setViewport(const QRect& viewport);
    void setImageScale(double scale);
    void setExportDpi(int dpi);
    void setPlacer(cwCaptureLabelPlacer* placer);

    // Scale factor for converting paper-pixels-at-export-DPI into the item's
    // local coord system. Lets a single 300-DPI-paper-px constant produce the
    // same scene-inch size in both preview and full-res paths.
    void setPaperPxToLocal(double scale);

    void placeStationLabels();

    QVector<QPointF> stationPositions() const;
    qreal stationDotRadius() const;

    QVector<QPair<QString, QRectF>> placedLabels() const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    struct StationDrawData {
        QString name;
        QPointF position;
        QRectF  labelRect;
    };

    QFont scaledLabelFont() const;

    void rebuildGeometry();

    cwSurveyNetwork m_network;
    cwCamera* m_camera;
    QRect m_viewport;
    QRectF m_boundingRect;
    qreal m_imageScale;
    int m_exportDpi = 96;
    cwCaptureLabelPlacer* m_placer = nullptr;
    QVector<QLineF> m_lines;
    QVector<StationDrawData> m_stationData;
    QPen m_linePen;
    QPen m_stationPen;
    QBrush m_stationBrush;
    QPen m_labelPen;
    QFont m_labelFont;
    qreal m_baseStationRadius;
    double m_paperPxToLocal = 1.0;
};

#endif // CWCAPTURECENTERLINE_H
