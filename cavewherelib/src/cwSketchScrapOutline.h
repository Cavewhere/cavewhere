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
};

#endif // CWSKETCHSCRAPOUTLINE_H
