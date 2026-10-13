/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwRenderPassageTube.h"
#include "cwRHIPassageTube.h"
#include "cwSceneVisibility.h"

cwRenderPassageTube::cwRenderPassageTube(QObject* parent) :
    cwRenderObject(parent)
{
}

void cwRenderPassageTube::setMesh(const cwPassageTubeMesh& mesh)
{
    GeometryState state;
    state.positions = mesh.positions;
    state.indices = mesh.indices;
    for (const QVector3D& position : mesh.positions) {
        state.bounds.unite(position);
    }

    m_cpuBytes.setBytes(qint64(state.positions.size()) * qint64(sizeof(QVector3D))
                        + qint64(state.indices.size()) * qint64(sizeof(quint32)));

    m_visibility = QVector<quint8>(state.indices.size(), kVisible);
    m_tripIndexRanges = mesh.tripIndexRanges;
    m_tripBoundaryRanges = mesh.tripBoundaryRanges;
    applyTripBoundaryVisibility(m_visibility);
    m_geometry.setValue(std::move(state));

    publishMask();
    update();
}

void cwRenderPassageTube::clear()
{
    setMesh(cwPassageTubeMesh());
}

void cwRenderPassageTube::setRangeVisible(int firstIndex, int count, bool visible)
{
    // Written as a subtraction so a huge count cannot overflow the sum.
    const qsizetype indexCount = m_visibility.size();
    if (firstIndex < 0 || count <= 0 || count > indexCount - firstIndex) {
        return;
    }

    QVector<quint8> visibility = m_visibility;
    const quint8 flag = visible ? kVisible : kHidden;
    for (int i = firstIndex; i < firstIndex + count; ++i) {
        visibility[i] = flag;
    }
    applyTripBoundaryVisibility(visibility);
    if (visibility == m_visibility) {
        return;
    }

    m_visibility = visibility;
    publishMask();
    update();
}

void cwRenderPassageTube::setWireframe(bool wireframe)
{
    if (m_wireframe == wireframe) {
        return;
    }
    m_wireframe = wireframe;
    emit wireframeChanged();
    update();
}

void cwRenderPassageTube::setOpacity(double opacity)
{
    if (m_opacity == opacity) {
        return;
    }
    m_opacity = opacity;
    emit opacityChanged();
    update();
}

void cwRenderPassageTube::setClipPlane(const QVector4D& clipPlane)
{
    if (m_clipPlane == clipPlane) {
        return;
    }
    m_clipPlane = clipPlane;
    emit clipPlaneChanged();
    update();
}

void cwRenderPassageTube::setClipEnabled(bool clipEnabled)
{
    if (m_clipEnabled == clipEnabled) {
        return;
    }
    m_clipEnabled = clipEnabled;
    emit clipEnabledChanged();
    update();
}

void cwRenderPassageTube::updateVisibility()
{
    cwRenderObject::updateVisibility();
    publishMask();
}

void cwRenderPassageTube::publishMask()
{
    if (auto* visibilityStore = sceneVisibility()) {
        // An empty mask is the store's all-visible fast path.
        const bool anyHidden = m_visibility.contains(kHidden);
        visibilityStore->setMask(renderObjectId(), kSubId,
                                 anyHidden ? m_visibility : QVector<quint8>());
    }
}

void cwRenderPassageTube::applyTripBoundaryVisibility(QVector<quint8>& visibility) const
{
    const auto isTripVisible = [&](int trip) {
        const cwPassageTubeMesh::VertexRange& range = m_tripIndexRanges.at(trip);
        return range.count > 0 && visibility.at(range.first) == kVisible;
    };
    for (const cwPassageTubeMesh::TripBoundary& boundary : std::as_const(m_tripBoundaryRanges)) {
        const bool shown = isTripVisible(boundary.trip) && !isTripVisible(boundary.neighborTrip);
        const quint8 flag = shown ? kVisible : kHidden;
        for (int i = boundary.range.first; i < boundary.range.first + boundary.range.count; ++i) {
            visibility[i] = flag;
        }
    }
}

cwRHIObject* cwRenderPassageTube::createRHIObject()
{
    return new cwRHIPassageTube();
}
