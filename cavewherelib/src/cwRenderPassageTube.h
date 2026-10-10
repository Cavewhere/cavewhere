/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERPASSAGETUBE_H
#define CWRENDERPASSAGETUBE_H

// Our includes
#include "cwPassageTubeMesh.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderObject.h"
#include "cwTracked.h"

// Qt includes
#include <QBox3D>
#include <QQmlEngine>
#include <QVector3D>
#include <QVector>

// Draws a cwPassageTubeMesh: flat shaded, two-sided, opaque. The tube is
// deliberately absent from picking, so station snapping keeps hitting the
// line plot inside it.
class cwRenderPassageTube : public cwRenderObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderPassageTube)

    Q_PROPERTY(bool wireframe READ wireframe WRITE setWireframe NOTIFY wireframeChanged)

    friend class cwRHIPassageTube;

public:
    explicit cwRenderPassageTube(QObject* parent = nullptr);

    // New geometry resets visibility to all-visible; the owner re-applies any
    // hidden trip ranges afterward.
    void setMesh(const cwPassageTubeMesh& mesh);
    void clear();

    // Hides or shows the triangles in indices [firstIndex, firstIndex + count),
    // e.g. one entry of tripIndexRanges(). The per-index mask is published to the
    // scene visibility store, and cwRHIPassageTube compacts the index buffer from
    // it. The published buffer is shared with the store, so it is replaced, never
    // mutated in place.
    void setRangeVisible(int firstIndex, int count, bool visible);

    bool wireframe() const;
    void setWireframe(bool wireframe);

    QVector<QVector3D> positions() const;
    QVector<quint32> indices() const;
    QVector<cwPassageTubeMesh::VertexRange> tripIndexRanges() const;
    QVector<quint8> visibility() const;

    static constexpr quint8 kVisible = 0xFF;
    static constexpr quint8 kHidden = 0x00;

    // The whole tube occupies one sub-slot in the visibility store.
    static constexpr uint64_t kSubId = 0;

signals:
    void wireframeChanged();

protected:
    cwRHIObject* createRHIObject() override;
    void updateVisibility() override;

private:
    struct GeometryState {
        QVector<QVector3D> positions;
        QVector<quint32> indices;
        QBox3D bounds;

        // setMesh runs once per rebuild, so a deep compare would only cost time.
        bool operator!=(const GeometryState&) const { return true; }
    };

    void publishMask();

    cwTracked<GeometryState> m_geometry;
    bool m_wireframe = false;
    QVector<cwPassageTubeMesh::VertexRange> m_tripIndexRanges;

    // Authoring copy of the per-index mask; consumers read the published store entry.
    QVector<quint8> m_visibility;

    cwLedgeredBytes m_cpuBytes {cwRenderMemoryLedger::Category::PassageTubeGeometry,
                                cwRenderMemoryLedger::Residency::Cpu};
};

inline bool cwRenderPassageTube::wireframe() const
{
    return m_wireframe;
}

inline QVector<QVector3D> cwRenderPassageTube::positions() const
{
    return m_geometry.value().positions;
}

inline QVector<quint32> cwRenderPassageTube::indices() const
{
    return m_geometry.value().indices;
}

inline QVector<cwPassageTubeMesh::VertexRange> cwRenderPassageTube::tripIndexRanges() const
{
    return m_tripIndexRanges;
}

inline QVector<quint8> cwRenderPassageTube::visibility() const
{
    return m_visibility;
}

#endif // CWRENDERPASSAGETUBE_H
