/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Std includes
#include <limits>

// Our includes
#include "cwRenderLinePlot.h"
#include "cwRHILinePlot.h"
#include "cwScene.h"

cwRenderLinePlot::cwRenderLinePlot(QObject *parent) :
    cwRenderObject(parent)
{
}

void cwRenderLinePlot::setGeometry(QVector<QVector3D> pointData,
                                   QVector<unsigned int> indexData)
{
    Data data;

    // Find the max and min Z values
    data.maxZValue = -std::numeric_limits<float>::max();
    data.minZValue = std::numeric_limits<float>::max();

    for(const auto& point : pointData) {
        data.maxZValue = qMax(data.maxZValue, point.z());
        data.minZValue = qMin(data.minZValue, point.z());
    }

    data.points = pointData;
    data.indexes = indexData;

    m_data.setValue(data);
    update();
}

cwRHIObject* cwRenderLinePlot::createRHIObject()
{
    return new cwRHILinePlot();
}


