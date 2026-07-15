/**************************************************************************
**
**    Copyright (C) 2025
**
**************************************************************************/

#ifndef CWCAPTURECENTERLINE_H
#define CWCAPTURECENTERLINE_H

// Qt includes
#include <QBrush>
#include <QPen>
#include <QVector>

// Our includes
#include "cwCaptureLabelItem.h"
#include "cwCaptureLabelPlacer.h"
#include "cwGlobals.h"
#include "cwLabelPlacementControl.h"
#include "cwSurveyNetwork.h"

class CAVEWHERE_LIB_EXPORT cwCaptureCenterline : public cwCaptureLabelItem
{
public:
    // Stroke thickness used when rendering centerline legs, in paper pixels
    // at the export DPI. Exposed so the placer can treat the centerline as
    // a soft obstacle with the right inflation.
    static constexpr qreal LinePenWidthPaperPx = 1.0;

    explicit cwCaptureCenterline(QGraphicsItem* parent = nullptr);

    void setNetwork(const cwSurveyNetwork& network);

    // Builds one placement request per named station (sorted top-to-bottom,
    // left-to-right by rebuildGeometry; text measured with the scaled label
    // font). Runs on the export worker thread; the optional control's
    // isCanceled() is polled per station. Pass the SAME PlacementViewport
    // given to the placer's setPlacementViewport so off-viewport stations
    // skip measurement without changing placements (see the struct's
    // comment); a default-constructed viewport disables that cull. Feed the
    // returned requests to cwCaptureLabelPlacer::placeAll and hand the
    // matching slice of its results to applyPlacements.
    QVector<cwCaptureLabelPlacer::LabelRequest> buildLabelRequests(
        const cwLabelPlacementControl& control = {},
        const cwCaptureLabelPlacer::PlacementViewport& viewport = {});

    // Applies placeAll's results for the requests built by the last
    // buildLabelRequests call, in the same order.
    void applyPlacements(const QVector<cwCaptureLabelPlacer::Placement>& placements);

    QVector<QPointF> stationPositions() const;
    qreal stationDotRadius() const;

    const QVector<QLineF>& lines() const { return m_lines; }

    QVector<QPair<QString, QRectF>> placedLabels() const;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

protected:
    void rebuildGeometry() override;

private:
    cwSurveyNetwork m_network;
    QVector<QLineF> m_lines;
    QVector<LabelDrawData> m_stationData;
    QPen m_linePen;
    QPen m_stationPen;
    QBrush m_stationBrush;
    qreal m_baseStationRadius;
};

#endif // CWCAPTURECENTERLINE_H
