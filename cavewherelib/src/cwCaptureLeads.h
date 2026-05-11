/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAPTURELEADS_H
#define CWCAPTURELEADS_H

// Qt includes
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
class cwCaptureLabelPlacer;

class CAVEWHERE_LIB_EXPORT cwCaptureLeads : public QGraphicsItem
{
public:
    struct LeadDrawData {
        QPointF markerPos;
        QString text;
        QRectF  labelRect;
        QPointF leaderStart;
        QPointF leaderEnd;
        bool    hasLeader = false;
    };

    explicit cwCaptureLeads(QGraphicsItem* parent = nullptr);

    void setRegion(cwCavingRegion* region);
    void setCamera(cwCamera* camera);
    void setViewport(const QRect& viewport);
    void setImageScale(double scale);
    void setExportDpi(int dpi);
    void setPlacer(cwCaptureLabelPlacer* placer);

    // Scale factor for converting paper-pixels-at-export-DPI into the item's
    // local coord system. Lets a single 300-DPI-paper-px constant produce the
    // same scene-inch size in both preview and full-res paths.
    void setPaperPxToLocal(double scale);

    qreal markerRadius() const;
    QVector<QPointF> leadMarkerPositions() const;

    void placeLeadLabels();

    const QVector<LeadDrawData>& layout() const { return m_leads; }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QFont scaledGlyphFont() const;
    QFont scaledLabelFont() const;

    void rebuildGeometry();

    QPointer<cwCavingRegion> m_region;
    cwCamera* m_camera;
    QRect m_viewport;
    QRectF m_boundingRect;
    qreal m_imageScale;
    int m_exportDpi = 96;
    cwCaptureLabelPlacer* m_placer = nullptr;
    QVector<LeadDrawData> m_leads;
    QPen m_glyphPen;
    QFont m_glyphFont;
    QPen m_labelPen;
    QFont m_labelFont;
    qreal m_textMaxWidth;
    double m_paperPxToLocal = 1.0;
};

#endif // CWCAPTURELEADS_H
