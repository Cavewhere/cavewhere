/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERLINEPLOT_H
#define CWRENDERLINEPLOT_H

// Our includes
#include "cwRenderObject.h"
#include "cwTracked.h"

// Qt includes
#include <QVector3D>
#include <QVector>
#include <QQmlEngine>

class cwRenderLinePlot : public cwRenderObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderLinePlot)

    friend class cwRHILinePlot;

public:
    explicit cwRenderLinePlot(QObject *parent = nullptr);

    // Points are a non-indexed line list: a drawn shot owns vertices [2i, 2i+1].
    // New geometry resets visibility to all-visible, one byte per vertex; the
    // owner re-applies any hidden ranges (cwLinePlotManager::reconcileTripKeywordItems).
    void setGeometry(QVector<QVector3D> pointData);

    // Per-vertex visibility, one byte per vertex (255 = visible, 0 = hidden;
    // both vertices of a shot share the value). Read directly as a vertex
    // attribute in the shader, which collapses hidden vertices off-clip.
    // Tracked independently of the geometry so a keyword toggle re-uploads only
    // the small visibility buffer, not the position buffer. setRangeVisible
    // flips a contiguous span (one trip's vertices); the manager ANDs
    // overlapping groups CPU-side. Mirrors cwRenderTexturedItems::setVisible.
    void setRangeVisible(int start, int count, bool visible);

    float maxZValue() const;
    float minZValue() const;

    QVector<QVector3D> points() const;
    QVector<quint8> visibility() const;

signals:
    // Emitted whenever setGeometry() replaces the vertex data (and therefore the
    // min/max Z extents). Lets owners reposition dependent objects, e.g. the grid
    // plane snapping to the lowest cave depth.
    void geometryChanged();

protected:
    virtual cwRHIObject* createRHIObject() override;

private:
    struct Data {
        float maxZValue = 0.0;
        float minZValue = 0.0;

        QVector<QVector3D> points;

        // This is needed for cwTracked to work, all ways changes
        bool operator!=(const Data& other) const { return true; }
    };

    cwTracked<Data> m_data;
    cwTracked<QVector<quint8>> m_visibility;

public:
    // Per-vertex visibility sentinels: one byte per vertex, read as a vertex
    // attribute in the shader. Shared with cwRHILinePlot (its empty-geometry
    // fallback must agree on which value means "visible").
    static constexpr quint8 kVisible = 0xFF;
    static constexpr quint8 kHidden = 0x00;
};

inline float cwRenderLinePlot::maxZValue() const
{
    return m_data.value().maxZValue;
}

inline float cwRenderLinePlot::minZValue() const
{
    return m_data.value().minZValue;
}

inline QVector<QVector3D> cwRenderLinePlot::points() const
{
    return m_data.value().points;
}

inline QVector<quint8> cwRenderLinePlot::visibility() const
{
    return m_visibility.value();
}

#endif // CWRENDERLINEPLOT_H
