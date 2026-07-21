/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERLINEPLOT_H
#define CWRENDERLINEPLOT_H

// Std includes
#include <optional>
#include <utility>

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
    // both vertices of a shot share the value). setRangeVisible flips a
    // contiguous span (one trip's vertices); the manager ANDs overlapping
    // groups CPU-side. Mirrors cwRenderTexturedItems::setItemVisible.
    //
    // The buffer is published to the scene visibility store
    // (cwSceneVisibility::setMask), which is the single consumer-facing truth:
    // the intersecter reads it per pick query, and cwRHILinePlot uploads the
    // GPU vertex attribute from the frame's snapshot, gated on the entry's
    // store version. The store shares this buffer rather than deriving its
    // own — so it is immutable once published: write to a copy and replace
    // it, never mutate it in place.
    void setRangeVisible(int start, int count, bool visible);

    float maxZValue() const;
    float minZValue() const;

    QVector<QVector3D> points() const;
    QVector<quint8> visibility() const;

    // Endpoints of the line segment whose first vertex is firstIndex — the value
    // cwRayHit::firstIndex carries for a line hit (under the iota index list it is
    // the from-vertex index). Returns {from, to} in model space, or nullopt when
    // firstIndex is out of range. Stations ARE these vertices, so this is the seam
    // clamp-to-station picking reads to snap a centerline hit onto a survey station.
    std::optional<std::pair<QVector3D, QVector3D>> segmentEndpoints(int firstIndex) const;

signals:
    // Emitted whenever setGeometry() replaces the vertex data (and therefore the
    // min/max Z extents). Lets owners reposition dependent objects, e.g. the grid
    // plane snapping to the lowest cave depth.
    void geometryChanged();

protected:
    virtual cwRHIObject* createRHIObject() override;
    void updateVisibility() override;

private:
    struct Data {
        float maxZValue = 0.0;
        float minZValue = 0.0;

        QVector<QVector3D> points;

        // This is needed for cwTracked to work, all ways changes
        bool operator!=(const Data& other) const { return true; }
    };

    cwTracked<Data> m_data;

    // Authoring copy of the per-vertex mask — the source for setRangeVisible's
    // range-flip math and for updateVisibility's re-seed. Consumers read the
    // published store entry, never this member.
    QVector<quint8> m_visibility;

public:
    // Per-vertex visibility sentinels: one byte per vertex, read as a vertex
    // attribute in the shader. Shared with cwRHILinePlot (its all-visible
    // fallback must agree on which value means "visible").
    static constexpr quint8 kVisible = 0xFF;
    static constexpr quint8 kHidden = 0x00;

    // The plot is one geometry blob, so it occupies a single sub-slot in both
    // the intersecter and the visibility store. Shared with cwRHILinePlot's
    // snapshot reads.
    static constexpr uint64_t kSubId = 0;
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
    return m_visibility;
}

#endif // CWRENDERLINEPLOT_H
