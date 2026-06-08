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
        double z = 0.0;
    };

    virtual ~cwSketchPathSource();

    virtual QList<Path> paths() const = 0;
};

#endif // CWSKETCHPATHSOURCE_H
