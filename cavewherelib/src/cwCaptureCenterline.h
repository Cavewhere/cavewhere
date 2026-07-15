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
#include "cwCaptureLabelPlacer.h"
#include "cwGlobals.h"
#include "cwLabelPlacementControl.h"
#include "cwSurveyNetwork.h"

class cwCamera;

class CAVEWHERE_LIB_EXPORT cwCaptureCenterline : public QGraphicsItem
{
public:
    // Stroke thickness used when rendering centerline legs, in paper pixels
    // at the export DPI. Exposed so the placer can treat the centerline as
    // a soft obstacle with the right inflation.
    static constexpr qreal LinePenWidthPaperPx = 1.0;

    explicit cwCaptureCenterline(QGraphicsItem* parent = nullptr);

    void setNetwork(const cwSurveyNetwork& network);
    void setCamera(cwCamera* camera);
    void setViewport(const QRect& viewport);
    void setImageScale(double scale);
    void setExportDpi(int dpi);

    // Scale factor for converting paper-pixels-at-export-DPI into the item's
    // local coord system. Lets a single 300-DPI-paper-px constant produce the
    // same scene-inch size in both preview and full-res paths.
    void setPaperPxToLocal(double scale);

    // Builds one placement request per named station (sorted top-to-bottom,
    // left-to-right; text measured with the scaled label font). Runs on the
    // export worker thread. The optional control's isCanceled() is polled per
    // station so a canceled export bails during measurement too; progress
    // ticks happen later, in the placer's placeAll. Feed the returned requests
    // to cwCaptureLabelPlacer::placeAll and hand the matching slice of its
    // results to applyPlacements.
    QVector<cwCaptureLabelPlacer::LabelRequest> buildLabelRequests(
        const cwLabelPlacementControl& control = {});

    // Applies placeAll's results for the requests built by the last
    // buildLabelRequests call, in the same order.
    void applyPlacements(const QVector<cwCaptureLabelPlacer::Placement>& placements);

    QVector<QPointF> stationPositions() const;
    qreal stationDotRadius() const;

    const QVector<QLineF>& lines() const { return m_lines; }

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
    QVector<QLineF> m_lines;
    QVector<StationDrawData> m_stationData;
    // Maps each request from the last buildLabelRequests() call back to its
    // m_stationData index (empty-named stations produce no request).
    QVector<int> m_requestStationIndex;
    QPen m_linePen;
    QPen m_stationPen;
    QBrush m_stationBrush;
    QPen m_labelPen;
    QFont m_labelFont;
    qreal m_baseStationRadius;
    double m_paperPxToLocal = 1.0;
};

#endif // CWCAPTURECENTERLINE_H
