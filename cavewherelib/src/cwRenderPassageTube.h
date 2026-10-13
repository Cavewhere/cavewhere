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
#include <QVector4D>
#include <QVector>

// Draws a cwPassageTubeMesh: flat shaded and two-sided, opaque or translucent.
// A clip plane cuts the tube open, discarding the side where
// dot((world, 1), clipPlane) > 0. The tube is deliberately absent from
// picking, so station snapping keeps hitting the line plot inside it.
class cwRenderPassageTube : public cwRenderObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderPassageTube)

    Q_PROPERTY(bool wireframe READ wireframe WRITE setWireframe NOTIFY wireframeChanged)
    Q_PROPERTY(double opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(QVector4D clipPlane READ clipPlane WRITE setClipPlane NOTIFY clipPlaneChanged)
    Q_PROPERTY(bool clipEnabled READ clipEnabled WRITE setClipEnabled NOTIFY clipEnabledChanged)

    friend class cwRHIPassageTube;

public:
    explicit cwRenderPassageTube(QObject* parent = nullptr);

    // New geometry resets visibility to every trip visible, its trip boundary
    // discs hidden; the owner re-applies any hidden trip ranges afterward.
    void setMesh(const cwPassageTubeMesh& mesh);
    void clear();

    // Hides or shows the triangles in indices [firstIndex, firstIndex + count),
    // e.g. one entry of tripIndexRanges(). A trip boundary disc then shows
    // exactly while its trip is visible and its neighbor trip hidden. The
    // per-index mask is published to the scene visibility store, and
    // cwRHIPassageTube compacts the index buffer from it. The published buffer
    // is shared with the store, so it is replaced, never mutated in place.
    void setRangeVisible(int firstIndex, int count, bool visible);

    bool wireframe() const;
    void setWireframe(bool wireframe);

    // Below 1 the walls draw blended in the transparent pass without writing
    // depth, so interior faces show through.
    double opacity() const;
    void setOpacity(double opacity);

    QVector4D clipPlane() const;
    void setClipPlane(const QVector4D& clipPlane);

    bool clipEnabled() const;
    void setClipEnabled(bool clipEnabled);

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
    void opacityChanged();
    void clipPlaneChanged();
    void clipEnabledChanged();

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
    void applyTripBoundaryVisibility(QVector<quint8>& visibility) const;

    cwTracked<GeometryState> m_geometry;
    bool m_wireframe = false;
    double m_opacity = 1.0;
    QVector4D m_clipPlane;
    bool m_clipEnabled = false;
    QVector<cwPassageTubeMesh::VertexRange> m_tripIndexRanges;
    QVector<cwPassageTubeMesh::TripBoundary> m_tripBoundaryRanges;

    // Authoring copy of the per-index mask; consumers read the published store entry.
    QVector<quint8> m_visibility;

    cwLedgeredBytes m_cpuBytes {cwRenderMemoryLedger::Category::PassageTubeGeometry,
                                cwRenderMemoryLedger::Residency::Cpu};
};

inline bool cwRenderPassageTube::wireframe() const
{
    return m_wireframe;
}

inline double cwRenderPassageTube::opacity() const
{
    return m_opacity;
}

inline QVector4D cwRenderPassageTube::clipPlane() const
{
    return m_clipPlane;
}

inline bool cwRenderPassageTube::clipEnabled() const
{
    return m_clipEnabled;
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
