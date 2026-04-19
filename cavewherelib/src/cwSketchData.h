/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHDATA_H
#define CWSKETCHDATA_H

//Qt includes
#include <QString>
#include <QUuid>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPenStroke.h"
#include "cwScale.h"

class CAVEWHERE_LIB_EXPORT cwSketchData {
public:
    // Mirror of cwSketch::ViewType. Duplicated to avoid a circular include
    // (cwSketch::data() returns this struct by value, and Qt's QStringView
    // SFINAE probe requires a complete return type).
    enum ViewType {
        Plan = 0,
        RunningProfile = 1,
        ProjectedProfile = 2
    };

    QString  name;
    QUuid    id;
    ViewType viewType = Plan;
    cwScale::Data mapScale;
    QVector<cwPenStroke> strokes;
    QString  anchorStation;
};

#endif // CWSKETCHDATA_H
