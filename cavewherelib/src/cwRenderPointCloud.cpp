/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwRenderPointCloud.h"
#include "cwRHIPointCloud.h"

cwRenderPointCloud::cwRenderPointCloud(QObject* parent) :
    cwRenderObject(parent)
{
}

void cwRenderPointCloud::setGeometry(cwGeometry geometry,
                                     QVector3D bboxMin,
                                     QVector3D bboxMax)
{
    Data data = m_data.value();
    data.geometry = std::move(geometry);
    data.bboxMin = bboxMin;
    data.bboxMax = bboxMax;
    m_data.setValue(data);
    update();
}

void cwRenderPointCloud::clear()
{
    Data data;
    data.pointSize = m_data.value().pointSize;
    m_data.setValue(data);
    update();
}

void cwRenderPointCloud::setPointSize(float pointSize)
{
    Data data = m_data.value();
    if (qFuzzyCompare(data.pointSize, pointSize)) {
        return;
    }
    data.pointSize = pointSize;
    m_data.setValue(data);
    update();
}

cwRHIObject* cwRenderPointCloud::createRHIObject()
{
    return new cwRHIPointCloud();
}
