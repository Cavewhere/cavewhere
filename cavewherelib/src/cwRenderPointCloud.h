/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERPOINTCLOUD_H
#define CWRENDERPOINTCLOUD_H

// Our includes
#include "cwGeometry.h"
#include "cwRenderObject.h"
#include "cwTracked.h"

// Qt includes
#include <QQmlEngine>
#include <QVector3D>

class cwRenderPointCloud : public cwRenderObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderPointCloud)

    friend class cwRHIPointCloud;

public:
    // Bundle of fields handed across the model→render boundary in one shot.
    // Mirrors cwLazLoadResult / cwLazLayer's load-time outputs that the
    // renderer consumes together.
    struct GeometryData {
        cwGeometry geometry;
        QVector3D bboxMin;
        QVector3D bboxMax;
        float meanSpacingXY = 0.0f;
    };

    explicit cwRenderPointCloud(QObject* parent = nullptr);

    void setGeometry(GeometryData data);

    void clear();

    qsizetype pointCount() const;
    QVector3D bboxMin() const;
    QVector3D bboxMax() const;
    float pointSize() const;
    void setPointSize(float pointSize);
    float meanSpacingXY() const;
    float gapFudge() const;
    void setGapFudge(float gapFudge);

protected:
    cwRHIObject* createRHIObject() override;

private:
    struct Data {
        cwGeometry geometry;
        QVector3D bboxMin;
        QVector3D bboxMax;
        float pointSize = 2.0f;
        // Mean planar inter-point spacing in meters (sqrt(area / N)). Drives
        // per-cloud point radius in PointCloud.vert. 0 until first load.
        float meanSpacingXY = 0.0f;
        float gapFudge = 2.0f;

        // cwTracked compares with != to detect changes; geometry's
        // QByteArray COW makes this assignment cheap, and a coarse
        // "always different" matches cwRenderLinePlot's convention.
        bool operator!=(const Data& /*other*/) const { return true; }
    };

    cwTracked<Data> m_data;
};

inline qsizetype cwRenderPointCloud::pointCount() const
{
    return m_data.value().geometry.vertexCount();
}

inline QVector3D cwRenderPointCloud::bboxMin() const
{
    return m_data.value().bboxMin;
}

inline QVector3D cwRenderPointCloud::bboxMax() const
{
    return m_data.value().bboxMax;
}

inline float cwRenderPointCloud::pointSize() const
{
    return m_data.value().pointSize;
}

inline float cwRenderPointCloud::meanSpacingXY() const
{
    return m_data.value().meanSpacingXY;
}

inline float cwRenderPointCloud::gapFudge() const
{
    return m_data.value().gapFudge;
}

#endif // CWRENDERPOINTCLOUD_H
