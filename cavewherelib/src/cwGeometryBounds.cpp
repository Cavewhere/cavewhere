/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwGeometryBounds.h"

//Qt includes
#include <QVector3D>

namespace cw::geometry {

std::optional<QBox3D> positionBounds(const cwGeometry& geometry)
{
    const auto* positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    const qsizetype vertexCount = geometry.vertexCount();
    if(!positionAttribute || vertexCount == 0) {
        return std::nullopt;
    }

    QBox3D bounds;
    for(qsizetype index = 0; index < vertexCount; index++) {
        bounds.unite(geometry.value<QVector3D>(positionAttribute, index));
    }

    return bounds;
}

}
