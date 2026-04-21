/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPENSTROKE_H
#define CWPENSTROKE_H

//Qt includes
#include <QColor>
#include <QMetaType>
#include <QQmlEngine>
#include <QRectF>
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

class CAVEWHERE_LIB_EXPORT cwPenStroke {
    Q_GADGET

public:
    enum Kind {
        Wall = 0,
        Feature = 1,
        ScrapOutline = 2,
        Eraser = 3
    };
    Q_ENUM(Kind)

    cwPenStroke() = default;

    Kind              kind  = Feature;
    double            width = 2.5;
    QColor            color;
    QVector<cwPenPoint> points;
    QUuid             id;

    QRectF boundingBox() const;
};

Q_DECLARE_METATYPE(cwPenStroke)

// Re-export cwPenStroke as a QML namespace so its Kind enum is reachable as
// `PenStroke.Wall` / `PenStroke.Feature`. QML_FOREIGN_NAMESPACE avoids the
// "value type names should begin with a lowercase letter" warning emitted
// when a Q_GADGET is registered directly with QML_NAMED_ELEMENT.
struct cwPenStrokeForeign
{
    Q_GADGET
    QML_FOREIGN_NAMESPACE(cwPenStroke)
    QML_NAMED_ELEMENT(PenStroke)
};

#endif // CWPENSTROKE_H
