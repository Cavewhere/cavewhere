/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHSCRAPOUTLINE_H
#define CWSKETCHSCRAPOUTLINE_H

//Qt includes
#include <QPolygonF>
#include <QString>
#include <QUuid>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"

struct CAVEWHERE_LIB_EXPORT cwSketchScrapOutline {
    // Sorted ascending so the vector itself is the canonical identity of the
    // outline — stable across chain-walk direction and across edits that
    // don't change membership.
    QVector<QUuid> memberStrokeIds;
    QPolygonF      tripLocalPolygon; // closed, CCW, simplified

    bool operator==(const cwSketchScrapOutline&) const = default;
};

// Reason tags for cwSketchScrapRejectedStroke::reason. Shared as string
// literals so overlay labels and (future) log lines always agree.
namespace cwSketchScrapRejectReasons {
    // Detector-level rejections.
    constexpr auto TooFewPoints       = "too-few-points";
    constexpr auto RingTooSmall       = "ring-too-small";
    constexpr auto SimplifiedCollapse = "simplified-collapse";
    constexpr auto SalvageEmpty       = "salvage-empty";

    // cwScrapManager-level rejections (detector emitted an outline, but the
    // manager's downstream pipeline dropped it).
    constexpr auto NoAnchorStations = "no-anchor-stations";
}

struct CAVEWHERE_LIB_EXPORT cwSketchScrapRejectedStroke {
    QUuid     id;
    QString   reason;             // one of cwSketchScrapRejectReasons
    QPolygonF tripLocalPolyline;  // raw stroke points

    bool operator==(const cwSketchScrapRejectedStroke&) const = default;
};

struct CAVEWHERE_LIB_EXPORT cwSketchScrapDetectResult {
    QVector<cwSketchScrapOutline>         outlines;
    QVector<cwSketchScrapRejectedStroke>  rejected;

    // Populated only when the detector was asked for diagnostics. Keyed
    // by stroke id; callers reuse this to tag post-detector rejections
    // (e.g. outlines dropped for lack of anchor stations) without
    // re-copying stroke polylines.
    QHash<QUuid, QPolygonF>               rawStrokesById;
};

#endif // CWSKETCHSCRAPOUTLINE_H
