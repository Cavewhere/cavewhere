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

//Our includes
#include "CaveWhereLibExport.h"

struct CAVEWHERE_LIB_EXPORT cwSketchScrapOutline {
    QUuid     sourceStrokeId;
    QPolygonF tripLocalPolygon;      // closed, CCW, simplified
};

#endif // CWSKETCHSCRAPOUTLINE_H
