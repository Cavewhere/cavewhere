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
#include <QPen>
#include <QPointer>
#include <QVector>

// Our includes
#include "cwCaptureLabelItem.h"
#include "cwCaptureLabelPlacer.h"
#include "cwGlobals.h"
#include "cwLabelPlacementControl.h"

class cwCavingRegion;

class CAVEWHERE_LIB_EXPORT cwCaptureLeads : public cwCaptureLabelItem
{
public:
    struct LeadDrawData : cwCaptureLabelItem::LabelDrawData {
        QPointF leaderStart;
        QPointF leaderEnd;
        bool    hasLeader = false;

        // Statically shadow the base hooks (see LabelDrawData): the request
        // builder and placement apply run on LeadDrawData, so these versions
        // reset/apply the leader state alongside the label rect.
        void resetPlacement()
        {
            LabelDrawData::resetPlacement();
            hasLeader = false;
        }

        void applyPlacement(const cwCaptureLabelPlacer::Placement& placement)
        {
            LabelDrawData::applyPlacement(placement);
            if(placement.placed) {
                leaderStart = placement.leaderStart;
                leaderEnd   = placement.leaderEnd;
                // hasLeader mirrors the placer's registration rule: the
                // leader met the request's minimum drawable length.
                hasLeader   = placement.hasLeader;
            }
        }
    };

    explicit cwCaptureLeads(QGraphicsItem* parent = nullptr);

    void setRegion(cwCavingRegion* region);

    qreal markerRadius() const;
    QVector<QPointF> leadMarkerPositions() const;

    // Builds one placement request per lead with text (sorted top-to-bottom,
    // left-to-right by rebuildGeometry; text measured with the scaled label
    // font). Each request carries the leader-obstacle thickness and minimum
    // leader length so cwCaptureLabelPlacer::placeAll registers accepted
    // leaders as hard obstacles for every later label. Runs on the export
    // worker thread; the optional control's isCanceled() is polled per lead.
    // Pass the SAME PlacementViewport given to the placer's
    // setPlacementViewport so off-viewport leads skip measurement without
    // changing placements (see the struct's comment); a default-constructed
    // viewport disables that cull. Hand the matching slice of placeAll's
    // results to applyPlacements.
    QVector<cwCaptureLabelPlacer::LabelRequest> buildLabelRequests(
        const cwLabelPlacementControl& control = {},
        const cwCaptureLabelPlacer::PlacementViewport& viewport = {});

    // Applies placeAll's results for the requests built by the last
    // buildLabelRequests call, in the same order.
    void applyPlacements(const QVector<cwCaptureLabelPlacer::Placement>& placements);

    const QVector<LeadDrawData>& layout() const { return m_leads; }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

protected:
    void rebuildGeometry() override;

private:
    QFont scaledGlyphFont() const;

    QPointer<cwCavingRegion> m_region;
    QVector<LeadDrawData> m_leads;
    QPen m_glyphPen;
    QFont m_glyphFont;
    qreal m_textMaxWidth;
};

#endif // CWCAPTURELEADS_H
