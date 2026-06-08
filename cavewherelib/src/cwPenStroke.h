/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPENSTROKE_H
#define CWPENSTROKE_H

//Qt includes
#include <QMetaType>
#include <QRectF>
#include <QString>
#include <QUuid>
#include <QVector>

//Std includes
#include <algorithm>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPenPoint.h"

// Axis-aligned bounding box over a range via a projection from element to QPointF.
// Shared by cwPenStroke::boundingBox() (points of cwPenPoint) and the eraser
// path bbox (raw QPointF). Inline template — trivial cost, hot path.
template <typename Range, typename Project>
inline QRectF cwBoundingBoxOf(const Range &range, Project project)
{
    auto it = std::begin(range);
    const auto end = std::end(range);
    if (it == end) {
        return QRectF();
    }
    const QPointF first = project(*it);
    double minX = first.x();
    double maxX = first.x();
    double minY = first.y();
    double maxY = first.y();
    for (++it; it != end; ++it) {
        const QPointF p = project(*it);
        minX = std::min(minX, p.x());
        maxX = std::max(maxX, p.x());
        minY = std::min(minY, p.y());
        maxY = std::max(maxY, p.y());
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

// Uniform on-screen stroke width, in paper pixels at cwSketchScrapRasterizer's
// target DPI. A stopgap until commit 5 (offset-curve batching) resolves real
// per-brush paper-millimetre widths from the active palette; see the symbology
// palette plan. Every render path (live painter, scrap rasterizer, icon
// thumbnails, continuation hit-test) keys off this single value so they stay
// consistent in the meantime.
inline constexpr double kSketchStrokeRenderWidth = 2.5;

class CAVEWHERE_LIB_EXPORT cwPenStroke {
    Q_GADGET

public:
    cwPenStroke() = default;

    // Resolves to a cwLineBrush via the active palette; the brush defines the
    // stroke's entire look (Decision 2). A stable kebab-case name, not a UUID,
    // so swapping palettes re-skins the sketch by matching brush names.
    QString             brushName;
    QVector<cwPenPoint> points;
    QUuid               id;

    QRectF boundingBox() const;

    bool operator==(const cwPenStroke &o) const = default;
};

Q_DECLARE_METATYPE(cwPenStroke)

#endif // CWPENSTROKE_H
