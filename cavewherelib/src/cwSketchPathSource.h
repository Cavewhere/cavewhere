/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHPATHSOURCE_H
#define CWSKETCHPATHSOURCE_H

//Qt includes
#include <QColor>
#include <QList>
#include <QPainterPath>
#include <Qt>

//Our includes
#include "CaveWhereLibExport.h"

// Read-only supply of renderable paths for cwSketchPainter. A plain interface,
// not a QAbstractItemModel: sketch rendering moved from QML Shapes to C++, so
// model machinery would be dead weight on this surface. Consumed value-style
// (a snapshot copy per frame), so change notification lives on the concrete
// QObject implementations (e.g. each model's pathsChanged()), not here.
class CAVEWHERE_LIB_EXPORT cwSketchPathSource
{
public:
    struct Path {
        QPainterPath painterPath;
        QColor strokeColor;
        // strokeWidth <= 0 flags a fill pass (fillPath with strokeColor)
        // instead of a stroked outline.
        double strokeWidth = 1.0;

        // false = strokeWidth is in screen pixels (grid/line-plot/legacy: the
        // painter cancels mapScale via strokePenScale). true = world metres
        // (brush widths, paper-mm already baked to world-m): drawn straight
        // under worldToItem with no penScale.
        bool widthInWorldMetres = false;

        Qt::PenCapStyle  cap  = Qt::RoundCap;
        Qt::PenJoinStyle join = Qt::RoundJoin;
        // Dash deliberately omitted: the live RHI canvas (cwSketchDrawCanvas)
        // cannot dash, so it would always render solid. Re-add a dash arg to
        // setStrokePen when an export backend actually renders dashed strokes.

        double z = 0.0; // paint order; strokes are stable-sorted by z
    };

    virtual ~cwSketchPathSource();

    virtual QList<Path> paths() const = 0;
};

#endif // CWSKETCHPATHSOURCE_H
