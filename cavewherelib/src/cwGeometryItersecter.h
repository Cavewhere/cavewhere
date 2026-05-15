/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGEOMETRYITERSECTER_H
#define CWGEOMETRYITERSECTER_H

//Qt includes
#include <QVector>
#include <QVector3D>
#include <QRay3D>
#include <QBox3D>
#include <cstdint>

//Our includes
#include "cwRayHit.h"
#include "cwGeometry.h"
#include "CaveWhereLibExport.h"
class cwRenderObject;

class CAVEWHERE_LIB_EXPORT cwGeometryItersecter
{
public:
    struct Key {
        cwRenderObject* parentObject = nullptr;
        uint64_t id = 0;
    };

    class Object {
    public:
        Object() { }

        // pickRadius is the world-space sphere radius around each vertex used
        // for ray-vs-point intersection. Only consulted when geometry.type()
        // == cwGeometry::Type::Points.
        Object(Key key,
               cwGeometry geometry,
               QMatrix4x4 modelMatrix = QMatrix4x4(),
               float pickRadius = 0.0f) :
            m_key(key),
            m_geometry(geometry),
            m_modelMatrix(modelMatrix),
            m_pickRadius(pickRadius)
        {}

        Key key() const { return m_key; }
        const cwGeometry& geometry() const { return m_geometry; }

        cwRenderObject* parent() const { return m_key.parentObject; }
        uint64_t id() const { return m_key.id; }

        void setModelMatrix(const QMatrix4x4 matrix) { m_modelMatrix = matrix; }
        const QMatrix4x4& modelMatrix() const { return m_modelMatrix; }

        float pickRadius() const { return m_pickRadius; }

    private:

        Key m_key;
        cwGeometry m_geometry;
        QMatrix4x4 m_modelMatrix;
        float m_pickRadius = 0.0f;
    };

    // BVH primitive handles + node layout are public nested types so the
    // build helpers in the .cpp's anonymous namespace can reach them. The
    // BVH arrays themselves are still private state.
    struct Primitive {
        enum class Kind : uint8_t { Triangle, Point };
        Kind kind = Kind::Triangle;
        uint32_t nodeIndex = 0;       // index into Nodes
        uint32_t primitiveIndex = 0;  // triangle: first index into indices(); point: vertex index
    };

    struct BvhNode {
        QBox3D bbox;
        // Inner: left = leftChild, right = rightChild. Leaf: left = firstPrim, right = primitiveCount.
        uint32_t left = 0;
        uint32_t right = 0;
        uint8_t splitAxis = 0;
        bool isLeaf = false;
    };

    cwGeometryItersecter();

    void addObject(const cwGeometryItersecter::Object& object);
    void clear(cwRenderObject* parentObject = nullptr);
    void removeObject(cwRenderObject* parentObject, uint64_t id);
    void removeObject(const Key& objectKey);

    void setModelMatrix(cwRenderObject* parentObject, uint64_t id, const QMatrix4x4& modelMatrix);
    void setModelMatrix(const Key& objectKey, const QMatrix4x4& modelMatrix);

    QBox3D boundingBox(const Key& objectKey) const;

    double intersects(const QRay3D& ray) const;
    cwRayHit intersectsDetailed(const QRay3D& ray) const;

private:

    class Node {
    public:
        Node();
        Node(const cwGeometryItersecter::Object& object, int indexInIndexes);
        Node(const QBox3D boundingBox, const cwGeometryItersecter::Object& object);

        QBox3D BoundingBox;
        cwGeometryItersecter::Object Object;

        static QBox3D triangleToBoundingBox(const cwGeometryItersecter::Object & object, int indexInIndexes);
        static QBox3D triangleToBoundingBox(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3);
        static QBox3D lineToBoundingBox(const cwGeometryItersecter::Object & object, int indexInIndexes);
    };

    QList<Node> Nodes;

    // Flat BVH built lazily from Nodes' triangle + point primitives.
    // Lines stay on the linear nearestNeighbor() path (their existing
    // per-segment Node granularity already serves that fallback well).
    mutable QVector<BvhNode> m_bvhNodes;
    mutable QVector<Primitive> m_primitives;
    mutable bool m_bvhDirty = true;

    // BuildPrim is opaque to callers; defined in the .cpp's anonymous namespace.
    struct BuildPrim;

    void markBvhDirty() { m_bvhDirty = true; }
    void ensureBvh() const;
    void buildBvh() const;
    uint32_t buildBvhRecursive(QVector<BuildPrim>& prims, int begin, int end) const;
    QBox3D primitiveWorldBox(const Primitive& prim) const;
    void testPrimitive(const Primitive& prim, const QRay3D& ray, cwRayHit& best) const;

    template <typename Iterator>
    Iterator findNodeImpl(Iterator begin, Iterator end, const Key& objectKey) const {
        return std::find_if(begin, end, [&](const Node& node) {
            return node.Object.parent() == objectKey.parentObject &&
                   node.Object.id() == objectKey.id;
        });
    }

    auto findNode(const Key& objectKey) {
        return findNodeImpl(Nodes.begin(), Nodes.end(), objectKey);
    }

    auto findNode(const Key& objectKey) const {
        return findNodeImpl(Nodes.cbegin(), Nodes.cend(), objectKey);
    }

    void addTriangles(const cwGeometryItersecter::Object& object);
    void addLines(const cwGeometryItersecter::Object& object);
    void addPoints(const cwGeometryItersecter::Object& object);

    double nearestNeighbor(const QRay3D& ray) const;
//    double LineLineIntersect(QVector3D p1, QVector3D p2, QVector3D p3, QVector3D p4) const;

    // Helpers: transform point (w=1) and direction (w=0)
    static inline QVector3D mapPoint(const QMatrix4x4& m, const QVector3D& p) {
        const QVector4D hp = m * QVector4D(p, 1.0f);
        return (hp.w() != 0.0f) ? (hp.toVector3D() / hp.w()) : hp.toVector3D();
    }

    static inline QVector3D mapDirection(const QMatrix4x4& m, const QVector3D& v) {
        const QVector4D hv = m * QVector4D(v, 0.0f);
        return hv.toVector3D();
    }

    static inline QRay3D transformRayToModel(const QMatrix4x4& modelMatrix,
                                             const QRay3D& rayWorld)
    {
        const QMatrix4x4 modelToWorld = modelMatrix.inverted();
        const QVector3D originModel    = mapPoint(modelToWorld,    rayWorld.origin());
        const QVector3D directionModel = mapDirection(modelToWorld, rayWorld.direction());
        return QRay3D(originModel, directionModel);
    }

    static inline QVector3D transformNormalToWorld(const QMatrix4x4& worldFromModel,
                                                   const QVector3D& nModel)
    {
        const QMatrix4x4 modelFromWorld = worldFromModel.inverted();
        const QMatrix4x4 normalXform = modelFromWorld.transposed();
        const QVector4D hn = normalXform * QVector4D(nModel, 0.0f);
        return hn.toVector3D().normalized();
    }

    static cwRayHit rayTriangleMT(const QRay3D& rayModel,
                                  const QVector3D& a,
                                  const QVector3D& b,
                                  const QVector3D& c,
                                  bool cullBackfaces);
};

inline uint32_t qHash(const cwGeometryItersecter::Object& object) {
    return object.id();
}

#endif // CWGEOMETRYITERSECTER_H
