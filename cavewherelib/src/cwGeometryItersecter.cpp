/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwDebug.h"
#include "cwGeometryItersecter.h"
#include "cwRenderObject.h"

//Std limits
#include <limits>
#include <math.h>

//Qt includes
#include <QtNumeric>
#include <QPlane3D>
#include <QSphere3D>

namespace {
    // Multiplier applied to a point's pickRadius when expanding the cloud's
    // broad-phase AABB so that rays passing tangentially through the
    // outermost spheres aren't rejected by the box test.
    constexpr float kPointAabbPadScale = 1.0f;

    // Visibility guard shared by every traversal entry point. A null parent
    // means the object isn't owned by a cwRenderObject (test fixtures), so
    // treat it as pickable.
    bool isPickable(const cwGeometryItersecter::Object& object) {
        const cwRenderObject* parent = object.parent();
        return parent == nullptr || parent->isVisible();
    }
}

cwGeometryItersecter::cwGeometryItersecter()
{
}

/**
 * @brief cwGeometryItersecter::addTriangles
 * @param object
 *
 * Add the object to the itersector
 */
void cwGeometryItersecter::addObject(const cwGeometryItersecter::Object &object)
{
    switch(object.geometry().type()) {
    case cwGeometry::Type::Triangles:
        addTriangles(object);
        break;
    case cwGeometry::Type::Lines:
        addLines(object);
        break;
    case cwGeometry::Type::Points:
        addPoints(object);
        break;
    default:
        break;
    }
}

/**
 * @brief cwGeometryItersecter::clearObjects
 * @param parentObject
 *
 * This removes all objects that have parent object of parentObject
 *
 * If parentObject is null, this will clear objects
 */
void cwGeometryItersecter::clear(cwRenderObject *parentObject)
{
    if(parentObject == nullptr) {
        Nodes.clear();
        return;
    }

    QList<Node>::iterator iter = Nodes.begin();
    while(iter != Nodes.end()) {
        Node& currentNode = *iter;
        if(currentNode.Object.parent() == parentObject) {
            iter = Nodes.erase(iter);
        } else {
            iter++;
        }
    }
}

/**
 * @brief cwGeometryItersecter::removeObject
 * @param parentObject
 * @param id
 *
 * Removes the geometry that makes up parentObject and id
 */
void cwGeometryItersecter::removeObject(cwRenderObject *parentObject, uint64_t id)
{
    removeObject(Key(parentObject, id));
}

void cwGeometryItersecter::removeObject(const Key &objectKey)
{
    auto iter = findNode(objectKey);
    if (iter != Nodes.end()) {
        Nodes.erase(iter);
    }
}

void cwGeometryItersecter::setModelMatrix(cwRenderObject *parentObject, uint64_t id, const QMatrix4x4 &modelMatrix)
{
    setModelMatrix(Key(parentObject, id), modelMatrix);
}

void cwGeometryItersecter::setModelMatrix(const Key &objectKey, const QMatrix4x4& modelMatrix)
{
    auto iter = findNode(objectKey);
    if (iter != Nodes.end()) {
        iter->Object.setModelMatrix(modelMatrix);
    }
}

QBox3D cwGeometryItersecter::boundingBox(const Key &objectKey) const
{
    auto iter = findNode(objectKey);
    if (iter != Nodes.end()) {
        return iter->BoundingBox.transformed(iter->Object.modelMatrix());
    } else {
        return QBox3D();
    }
}

/**
 * @brief cwGeometryItersecter::intersects
 * @param ray
 * @return Closes intersection to on the ray, or if no match, use nearest neighbor search
 */
double cwGeometryItersecter::intersects(const QRay3D &ray) const
{
    const cwRayHit hit = intersectsDetailed(ray);
    if (hit.hit()) {
        return hit.tWorld();
    }
    return nearestNeighbor(ray);


    // QList<double> intersections;

    // qDebug() << "Test!" << ray;
    // for(const Node& node : Nodes) {
    //     double t = node.BoundingBox.intersection(ray);
    //     qDebug() << "Node:" << node.BoundingBox << t;
    //     if(!qIsNaN(t)) {
    //         if(node.Object.type() == Triangles) {
    //             Q_ASSERT(node.Object.indexes().size() % 3 == 0);
    //             for(int i = 0; i < node.Object.indexes().size(); i+=3) {
    //                 QBox3D box = Node::triangleToBoundingBox(node.Object, i);
    //                 t = box.intersection(ray);
    //                 if(!qIsNaN(t)) {
    //                     intersections.append(t);
    //                 }
    //             }
    //         } else {
    //             //Line nodes, are direct
    //             intersections.append(t);
    //         }
    //     }
    // }

    // qDebug() << "Intersections:" << intersections.size() << intersections;

    // //See if we've intersected anything
    // if(intersections.size() == 0) {
    //     return nearestNeighbor(ray); //Do a nearest neighbor search
    // }

    // //Find the max value int intersections
    // double maxValue = -std::numeric_limits<double>::max();
    // for(double t : intersections) {
    //     maxValue = std::max(t, maxValue);
    // }

    // return maxValue;
}

/**
 * @brief cwGeometryItersecter::addTriangles
 * @param object
 *
 * Adds the object as a triangle, if the object already exist is the
 * interecter, the object will be replaced.
 */
void cwGeometryItersecter::addTriangles(const cwGeometryItersecter::Object &object)
{
    //Make sure the object has the right number of indices
    if(object.geometry().indices().size() % 3 != 0) {
        qDebug() << "Can't add object" << object.parent() << object.id() << "because it has an invalid indexes" << LOCATION;
        return;
    }

    removeObject(object.parent(), object.id());

    auto positionAttribute = object.geometry().attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);

    QBox3D box;
    for(int i = 0; i < object.geometry().vertexCount(); i++) {
        box.unite(object.geometry().value<QVector3D>(positionAttribute, i));
    }

    Nodes.append(Node(box, object));
}

/**
 * @brief cwGeometryItersecter::addLines
 * @param object
 *
 * Adds the object as a lines.
 *
 * If the object already exists in the intersecter the object will be replace with
 * the new data.
 */
void cwGeometryItersecter::addLines(const cwGeometryItersecter::Object &object)
{
    //Make sure the object has the right number of indices
    if(object.geometry().indices().size() % 2 != 0) {
        qDebug() << "Can't add object" << object.parent() << object.id() << "because it has an invalid indexes" << LOCATION;
        return;
    }

    removeObject(object.parent(), object.id());

    for(int i = 0; i < object.geometry().indices().size(); i+=2) {
        Node node = Node(object, i);

        //Make sure the node is valid
        if(!node.BoundingBox.isNull()) {
            Nodes.append(node);
        }
    }
}

/**
 * @brief cwGeometryItersecter::addPoints
 * @param object
 *
 * Adds the object as a point cloud. One Node per cloud with a combined AABB
 * expanded by Object::pickRadius() so that broad-phase ray rejection doesn't
 * miss rays that graze the outermost sphere of a point.
 */
void cwGeometryItersecter::addPoints(const cwGeometryItersecter::Object &object)
{
    const cwGeometry& geometry = object.geometry();
    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    if (positionAttribute == nullptr) {
        qDebug() << "Can't add point object" << object.parent() << object.id() << "— missing Position attribute" << LOCATION;
        return;
    }

    const qsizetype vertexCount = geometry.vertexCount();
    if (vertexCount <= 0) {
        return;
    }

    removeObject(object.parent(), object.id());

    const char* base = geometry.vertexData().constData() + positionAttribute->byteOffset;
    const int stride = geometry.vertexStride();

    QBox3D box;
    for (qsizetype i = 0; i < vertexCount; ++i) {
        const float* p = reinterpret_cast<const float*>(base + i * stride);
        box.unite(QVector3D(p[0], p[1], p[2]));
    }

    const float pad = object.pickRadius() * kPointAabbPadScale;
    const QVector3D padVec(pad, pad, pad);

    Nodes.append(Node(QBox3D(box.minimum() - padVec, box.maximum() + padVec), object));
}


/**
 * @brief cwGeometryItersecter::nearestNeighbor
 * @param ray
 * @return Finds the point on the ray that's the nearest neigbor of the ray
 */
double cwGeometryItersecter::nearestNeighbor(const QRay3D &ray) const
{
    std::array<QVector3D, 8> points;

    double bestT = 0.0;
    double bestDistance = std::numeric_limits<double>::max();

    for(const Node& node : Nodes) {
        if (!isPickable(node.Object)) {
            continue;
        }

        if(node.Object.geometry().type() == cwGeometry::Type::Triangles) {
            continue;
        }

        QVector3D min = node.BoundingBox.minimum();
        QVector3D max = node.BoundingBox.maximum();

        points[0] = min;
        points[1] = QVector3D(max.x(), min.y(), min.z());
        points[2] = QVector3D(max.x(), max.y(), min.z());
        points[3] = QVector3D(min.x(), max.y(), min.z());
        points[4] = QVector3D(min.x(), min.y(), max.z());
        points[5] = QVector3D(max.x(), min.y(), max.z());
        points[6] = QVector3D(min.x(), max.y(), max.z());
        points[7] = max;

        for(const QVector3D& point : points) {
            double distance = ray.distance(point);
            if(distance < bestDistance) {
                double t = ray.projectedDistance(point);
                if(t > 0.0) {
                    bestDistance = distance;
                    bestT = t;
                }
            }
        }
    }

    if(bestT == 0.0) {
        return qSNaN();
    }

    return bestT;
}

cwRayHit cwGeometryItersecter::intersectsDetailed(const QRay3D &ray) const
{
    cwRayHit best;

    for (const Node& node : Nodes) {
        if (!isPickable(node.Object)) {
            continue;
        }

        const cwGeometry::Type type = node.Object.geometry().type();
        if (type != cwGeometry::Type::Triangles && type != cwGeometry::Type::Points) {
            continue;
        }

        const QMatrix4x4& worldFromModel = node.Object.modelMatrix();
        const QRay3D rayModel = transformRayToModel(worldFromModel, ray);

        // Optional broad-phase: skip whole object if its model-space AABB misses
        if (qIsNaN(node.BoundingBox.intersection(rayModel))) {
            continue;
        }

        const cwGeometry& geometry = node.Object.geometry();

        auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
        Q_ASSERT(positionAttribute);

        if (type == cwGeometry::Type::Triangles) {
            const QVector<uint32_t>& indices = geometry.indices();

            for (int i = 0; i < indices.size(); i += 3) {
                const QVector3D a = geometry.value<QVector3D>(positionAttribute, indices.at(i + 0));
                const QVector3D b = geometry.value<QVector3D>(positionAttribute, indices.at(i + 1));
                const QVector3D c = geometry.value<QVector3D>(positionAttribute, indices.at(i + 2));

                cwRayHit local = rayTriangleMT(rayModel, a, b, c, node.Object.geometry().cullBackfaces());
                if (!local.hit()) {
                    continue;
                }

                const QVector3D pWorld = mapPoint(worldFromModel, local.pointModel());
                const QVector3D nWorld = transformNormalToWorld(worldFromModel, local.normalModel());
                const double tWorld = ray.projectedDistance(pWorld);

                if (tWorld > 0.0 && (!best.hit() || tWorld < best.tWorld())) {
                    best = local;
                    best.m_hit = true;
                    best.m_pointWorld = pWorld;
                    best.m_normalWorld = nWorld;
                    best.m_tWorld = tWorld;
                    best.m_object = node.Object.parent();
                    best.m_objectId = node.Object.id();
                    best.m_firstIndex = i;
                }
            }
        } else {
            // Type::Points — ray-vs-sphere per vertex, closest hit (near root) wins.
            const float radius = node.Object.pickRadius();
            if (radius <= 0.0f) {
                continue;
            }

            const char* base = geometry.vertexData().constData() + positionAttribute->byteOffset;
            const int stride = geometry.vertexStride();
            const QVector3D worldNormal = -ray.direction().normalized();
            const QVector3D modelNormal = -rayModel.direction().normalized();

            const qsizetype vertexCount = geometry.vertexCount();
            for (qsizetype i = 0; i < vertexCount; ++i) {
                const float* p = reinterpret_cast<const float*>(base + i * stride);
                const QVector3D center(p[0], p[1], p[2]);

                float tNear = 0.0f;
                float tFar = 0.0f;
                if (!QSphere3D(center, radius).intersection(rayModel, &tNear, &tFar)) {
                    continue;
                }
                if (tNear <= 0.0f) {
                    // Ray points away from the sphere, or origin sits inside
                    // it — skip rather than report a back-face hit.
                    continue;
                }

                const QVector3D pWorld = mapPoint(worldFromModel, center);
                const double tWorld = ray.projectedDistance(pWorld);
                if (tWorld <= 0.0 || (best.hit() && tWorld >= best.tWorld())) {
                    continue;
                }

                best.m_hit = true;
                best.m_tModel = tNear;
                best.m_u = std::numeric_limits<float>::quiet_NaN();
                best.m_v = std::numeric_limits<float>::quiet_NaN();
                best.m_pointModel = center;
                best.m_normalModel = modelNormal;
                best.m_pointWorld = pWorld;
                best.m_normalWorld = worldNormal;
                best.m_tWorld = tWorld;
                best.m_object = node.Object.parent();
                best.m_objectId = node.Object.id();
                best.m_firstIndex = static_cast<int>(i);
            }
        }
    }

    return best;
}

cwRayHit cwGeometryItersecter::rayTriangleMT(const QRay3D &rayModel,
                                             const QVector3D &a,
                                             const QVector3D &b,
                                             const QVector3D &c,
                                             bool cullBackfaces)
{
    cwRayHit result;

    const QVector3D edge1 = b - a;
    const QVector3D edge2 = c - a;

    const QVector3D pvec = QVector3D::crossProduct(rayModel.direction(), edge2);
    const float det = QVector3D::dotProduct(edge1, pvec);

    const float kEps = 1e-7f;

    if (cullBackfaces) {
        if (det <= kEps) {
            return result;
        }
    } else {
        if (std::fabs(det) < kEps) {
            return result; // parallel or nearly parallel
        }
    }

    const float invDet = 1.0f / det;
    const QVector3D tvec = rayModel.origin() - a;

    const float u = QVector3D::dotProduct(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) {
        return result;
    }

    const QVector3D qvec = QVector3D::crossProduct(tvec, edge1);
    const float v = QVector3D::dotProduct(rayModel.direction(), qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) {
        return result;
    }

    const float t = QVector3D::dotProduct(edge2, qvec) * invDet;
    if (t <= 0.0f) {
        return result;
    }

    result.m_hit = true;
    result.m_tModel = t;
    result.m_u = u;
    result.m_v = v;
    result.m_pointModel = rayModel.origin() + t * rayModel.direction();
    // Unnormalized normal; normalize for safety
    result.m_normalModel = QVector3D::normal(a, b, c).normalized();
    return result;
}


cwGeometryItersecter::Node::Node(const cwGeometryItersecter::Object &object, int indexInIndexes) :
    Object(object)
{

    switch(object.geometry().type()) {
    case cwGeometry::Type::Triangles:
        BoundingBox = triangleToBoundingBox(object, indexInIndexes);
        break;
    case cwGeometry::Type::Lines:
        BoundingBox = lineToBoundingBox(object, indexInIndexes);
        break;
    default:
        break;
    }

}

cwGeometryItersecter::Node::Node(const QBox3D boundingBox,
                                 const cwGeometryItersecter::Object &object) :
    BoundingBox(boundingBox),
    Object(object)
{

}

/**
 * @brief cwGeometryItersecter::Node::triangleToBoundingBox
 * @return Creates a bounding box around a triangle
 */
QBox3D cwGeometryItersecter::Node::triangleToBoundingBox(const cwGeometryItersecter::Object & object, int indexInIndexes)
{
    auto positionAttribute = object.geometry().attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);
    QVector3D p1 = object.geometry().value<QVector3D>(positionAttribute, object.geometry().indices().at(indexInIndexes));
    QVector3D p2 = object.geometry().value<QVector3D>(positionAttribute, object.geometry().indices().at(indexInIndexes + 1));
    QVector3D p3 = object.geometry().value<QVector3D>(positionAttribute, object.geometry().indices().at(indexInIndexes + 2));

    return triangleToBoundingBox(p1, p2, p3);
}
/**
 * @brief cwGeometryItersecter::Node::triangleToBoundingBox
 * @return Creates a bounding box around a triangle
 */
QBox3D cwGeometryItersecter::Node::triangleToBoundingBox(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3)
{

    QVector3D maxPoint(qMax(p1.x(), qMax(p2.x(), p3.x())),
                       qMax(p1.y(), qMax(p2.y(), p3.y())),
                       qMax(p1.z(), qMax(p2.z(), p3.z())));

    QVector3D minPoint(qMin(p1.x(), qMin(p2.x(), p3.x())),
                       qMin(p1.y(), qMin(p2.y(), p3.y())),
                       qMin(p1.z(), qMin(p2.z(), p3.z())));

    return QBox3D(minPoint, maxPoint);
}

/**
 * @brief cwGeometryItersecter::Node::lineToBoundingBox
 * @return Creates a bounding box around a line
 */
QBox3D cwGeometryItersecter::Node::lineToBoundingBox(const cwGeometryItersecter::Object & object, int indexInIndexes)
{
    auto positionAttribute = object.geometry().attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);
    QVector3D p1 = object.geometry().value<QVector3D>(positionAttribute, object.geometry().indices().at(indexInIndexes));
    QVector3D p2 = object.geometry().value<QVector3D>(positionAttribute, object.geometry().indices().at(indexInIndexes + 1));

    QVector3D maxPoint(qMax(p1.x(), p2.x()),
                       qMax(p1.y(), p2.y()),
                       qMax(p1.z(), p2.z()));

    QVector3D minPoint(qMin(p1.x(), p2.x()),
                       qMin(p1.y(), p2.y()),
                       qMin(p1.z(), p2.z()));

    return QBox3D(minPoint, maxPoint);
}
