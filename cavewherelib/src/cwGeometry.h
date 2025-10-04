#pragma once

#include <QVector>
#include <QVector3D>
#include <QMatrix4x4>
#include <QMetaType>

struct cwGeometry
{
    enum PrimitiveType {
        Triangles,
        Lines,
        None
    };

    PrimitiveType type = None;
    QVector<QVector3D> vertices;
    QVector<uint> indices;
    QMatrix4x4 transform;
    bool cullBackfaces = true;

    bool isEmpty() const {
        return vertices.isEmpty() || indices.isEmpty();
    }

private:
    // Reserved for any internal data if needed later
};
