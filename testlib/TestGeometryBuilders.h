#ifndef TESTGEOMETRYBUILDERS_H
#define TESTGEOMETRYBUILDERS_H

// Shared cwGeometry builders that intersecter/pick and textured-item tests
// assemble. Each returns a finished geometry; tests wrap it in a
// cwGeometryItersecter::Object (or a render object) with their own
// parent/id/matrix conventions.

#include "cwGeometry.h"

#include <QVector>
#include <QVector2D>
#include <QVector3D>

#include <numeric>

namespace cwTestGeometry {

inline cwGeometry positionGeometry(const QVector<QVector3D>& points)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    return geometry;
}

inline cwGeometry points(const QVector<QVector3D>& positions)
{
    cwGeometry geometry = positionGeometry(positions);
    geometry.setType(cwGeometry::Type::Points);
    return geometry;
}

// A polyline registered the way cwRenderLinePlot::setGeometry does:
// consecutive pairs with a synthesized iota index list, so each segment's
// first index equals its from-vertex index.
inline cwGeometry lines(const QVector<QVector3D>& positions)
{
    cwGeometry geometry = positionGeometry(positions);
    QVector<uint32_t> indices(positions.size());
    std::iota(indices.begin(), indices.end(), 0u);
    geometry.setIndices(std::move(indices));
    geometry.setType(cwGeometry::Type::Lines);
    return geometry;
}

inline cwGeometry triangles(const QVector<QVector3D>& positions,
                            const QVector<uint32_t>& indices,
                            bool cullBackfaces = true)
{
    cwGeometry geometry = positionGeometry(positions);
    geometry.setIndices(indices);
    geometry.setType(cwGeometry::Type::Triangles);
    geometry.setCullBackfaces(cullBackfaces);
    return geometry;
}

// A quad in the z = 0 plane carrying the full 0..1 UV range, so its uv density
// is 1 over a world area of (2 * halfExtent)^2.
inline cwGeometry texturedQuad(float halfExtent)
{
    cwGeometry geometry({
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3},
        {cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2}
    });

    geometry.resizeVertices(4);
    const auto* position = geometry.attribute(cwGeometry::Semantic::Position);
    const auto* texCoord = geometry.attribute(cwGeometry::Semantic::TexCoord0);

    const QVector<QVector3D> positions = {
        QVector3D(-halfExtent, -halfExtent, 0.0f),
        QVector3D( halfExtent, -halfExtent, 0.0f),
        QVector3D( halfExtent,  halfExtent, 0.0f),
        QVector3D(-halfExtent,  halfExtent, 0.0f),
    };
    const QVector<QVector2D> uvs = {
        QVector2D(0.0f, 0.0f), QVector2D(1.0f, 0.0f),
        QVector2D(1.0f, 1.0f), QVector2D(0.0f, 1.0f),
    };
    for (int i = 0; i < positions.size(); i++) {
        geometry.set(position, i, positions.at(i));
        geometry.set(texCoord, i, uvs.at(i));
    }

    geometry.setIndices({0u, 1u, 2u, 0u, 2u, 3u});
    geometry.setType(cwGeometry::Type::Triangles);
    return geometry;
}

} // namespace cwTestGeometry

#endif // TESTGEOMETRYBUILDERS_H
