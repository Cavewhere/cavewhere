/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSketchTestHelpers.h"

#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwUnits.h"

#include <QColor>
#include <QPointF>
#include <QUuid>

cwSketchData makeSampleSketchData()
{
    cwSketchData data;
    data.name     = QStringLiteral("Round-trip sketch");
    data.id       = QUuid::createUuid();
    data.viewType = cwSketchData::Plan;

    data.mapScale.scaleNumerator.unit    = cwUnits::Meters;
    data.mapScale.scaleNumerator.value   = 1.0;
    data.mapScale.scaleDenominator.unit  = cwUnits::Meters;
    data.mapScale.scaleDenominator.value = 500.0;

    cwPenStroke wall;
    wall.kind  = cwPenStroke::Wall;
    wall.width = 2.0;
    wall.color = QColor("#ff112233");
    wall.id    = QUuid::createUuid();
    for (int i = 0; i < 5; ++i) {
        wall.points.append(cwPenPoint(QPointF(i * 1.0, i * 2.0), 0.5 + i * 0.05, 1000 + i));
    }
    data.strokes.append(wall);

    cwPenStroke feature;
    feature.kind  = cwPenStroke::Feature;
    feature.width = 0.75;
    feature.color = QColor("#80aabbcc");
    feature.id    = QUuid::createUuid();
    for (int i = 0; i < 3; ++i) {
        feature.points.append(cwPenPoint(QPointF(i * -1.5, 3.0), 0.9, 2000 + i));
    }
    data.strokes.append(feature);

    return data;
}
