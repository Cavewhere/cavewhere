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

//Our includes
class cwRenderObject;

class cwGeometryItersecter
{
public:

    enum PrimitiveType {
        Triangles,
        Lines,
        None
    };

    struct Key {
        cwRenderObject* parentObject = nullptr;
        uint64_t id = 0;
    };

    class Object {
    public:
        Object() : m_parent(nullptr), m_id(-1), m_type(None) { }
        Object(cwRenderObject* parent,
               uint64_t id,
               QVector<QVector3D> points,
               QVector<uint> indexes,
               PrimitiveType type,
               const QMatrix4x4& modelMatrix = QMatrix4x4(),
               bool cullBackfaces = false) :
            m_parent(parent),
            m_id(id),
            m_points(points),
            m_indexes(indexes),
            m_type(type),
            m_modelMatrix(modelMatrix),
            m_cullBackfaces(cullBackfaces)

        {}
        Object(Key key,
               QVector<QVector3D> points,
               QVector<uint> indexes,
               PrimitiveType type,
               const QMatrix4x4& modelMatrix = QMatrix4x4(),
               bool cullBackfaces = false) :
            m_parent(key.parentObject),
            m_id(key.id),
            m_points(points),
            m_indexes(indexes),
            m_type(type),
            m_modelMatrix(modelMatrix),
            m_cullBackfaces(cullBackfaces)
        {}

        cwRenderObject* parent() const { return m_parent; }
        uint64_t id() const { return m_id; }
        const QVector<QVector3D>& points() const { return m_points; }
        const QVector<uint>& indexes() const { return m_indexes; }
        PrimitiveType type() const { return m_type; }
        bool cullBackfaces() const { return m_cullBackfaces; }

        const QMatrix4x4& modelMatrix() const { return m_modelMatrix; }
        void setMatrix(const QMatrix4x4& modelMatrix) { m_modelMatrix = modelMatrix; }

    private:

        cwRenderObject* m_parent;
        uint64_t m_id;
        QVector<QVector3D> m_points;
        QVector<uint> m_indexes;
        PrimitiveType m_type;
        QMatrix4x4 m_modelMatrix;
        bool m_cullBackfaces = false;

    };

    struct RayTriangleHit {
        bool hit = false;

        // Model-space data
        float tModel = std::numeric_limits<float>::quiet_NaN(); // param on rayModel
        float u = std::numeric_limits<float>::quiet_NaN();      // barycentrics
        float v = std::numeric_limits<float>::quiet_NaN();
        QVector3D pointModel;
        QVector3D normalModel; // unit-length

        // World-space data
        double tWorld = std::numeric_limits<double>::quiet_NaN(); // projectedDistance on input world ray
        QVector3D pointWorld;
        QVector3D normalWorld; // unit-length

        uint64_t objectId = 0;
        int firstIndex = -1; // first index of the triangle in object's index array
    };

    cwGeometryItersecter();

    void addObject(const cwGeometryItersecter::Object& object);
    void clear(cwRenderObject* parentObject = nullptr);
    void removeObject(cwRenderObject* parentObject, uint64_t id);
    void removeObject(const Key& objectKey);

    void setModelMatrix(cwRenderObject* parentObject, uint64_t id, const QMatrix4x4& modelMatrix);
    void setModelMatrix(const Key& objectKey, const QMatrix4x4& modelMatrix);

    double intersects(const QRay3D& ray) const;
    RayTriangleHit intersectsTriangleDetailed(const QRay3D& ray) const;


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

    auto findNode(const Key& objectKey) {
        return std::find_if(Nodes.begin(), Nodes.end(),
                            [=](const Node& node) {
                                return node.Object.parent() == objectKey.parentObject &&
                                       node.Object.id() == objectKey.id;
                            });
    }

    void addTriangles(const cwGeometryItersecter::Object& object);
    void addLines(const cwGeometryItersecter::Object& object);

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

    static RayTriangleHit rayTriangleMT(const QRay3D& rayModel,
                                          const QVector3D& a,
                                          const QVector3D& b,
                                          const QVector3D& c,
                                          bool cullBackfaces);

    // static inline double worldT(const QRay3D& rayWorld, const QVector3D& hitWorld) {
    //     // Parameter/distance along the original world ray
    //     return rayWorld.projectedDistance(hitWorld);
    // }


};

inline uint qHash(const cwGeometryItersecter::Object& object) {
    return object.id();
}

#endif // CWGEOMETRYITERSECTER_H
