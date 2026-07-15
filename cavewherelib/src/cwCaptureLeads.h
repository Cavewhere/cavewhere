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
#include "cwCaptureLabelPlacer.h"
#include "cwGlobals.h"
#include "cwLabelPlacementControl.h"

class cwCamera;
class cwCavingRegion;

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

    // Scale factor for converting paper-pixels-at-export-DPI into the item's
    // local coord system. Lets a single 300-DPI-paper-px constant produce the
    // same scene-inch size in both preview and full-res paths.
    void setPaperPxToLocal(double scale);

    qreal markerRadius() const;
    QVector<QPointF> leadMarkerPositions() const;

    // Builds one placement request per lead with text (sorted top-to-bottom,
    // left-to-right by rebuildGeometry; text measured with the scaled label
    // font). Each request carries the leader-obstacle thickness and minimum
    // leader length so cwCaptureLabelPlacer::placeAll registers accepted
    // leaders as hard obstacles for every later label. Runs on the export
    // worker thread; the optional control's isCanceled() is polled per lead.
    // Hand the matching slice of placeAll's results to applyPlacements.
    QVector<cwCaptureLabelPlacer::LabelRequest> buildLabelRequests(
        const cwLabelPlacementControl& control = {});

    // Applies placeAll's results for the requests built by the last
    // buildLabelRequests call, in the same order.
    void applyPlacements(const QVector<cwCaptureLabelPlacer::Placement>& placements);

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
    QVector<LeadDrawData> m_leads;
    // Maps each request from the last buildLabelRequests() call back to its
    // m_leads index (empty-text leads produce no request).
    QVector<int> m_requestLeadIndex;
    QPen m_glyphPen;
    QFont m_glyphFont;
    QPen m_labelPen;
    QFont m_labelFont;
    qreal m_textMaxWidth;
    double m_paperPxToLocal = 1.0;
};

#endif // CWCAPTURELEADS_H
