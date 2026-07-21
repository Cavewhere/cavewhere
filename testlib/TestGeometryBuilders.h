#ifndef TESTGEOMETRYBUILDERS_H
#define TESTGEOMETRYBUILDERS_H

// Shared builders for the position-only cwGeometry that intersecter/pick tests
// assemble. Each returns a finished geometry; tests wrap it in a
// cwGeometryItersecter::Object (or a render object) with their own
// parent/id/matrix conventions.

#include "cwGeometry.h"

#include <QVector>
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

} // namespace cwTestGeometry

#endif // TESTGEOMETRYBUILDERS_H
