/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwDebug.h"
#include "cwGeometryItersecter.h"

//Std limits
#include <limits>
#include <math.h>

//Qt includes
#include <QtNumeric>
#include <QPlane3D>

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
    switch(object.type()) {
    case Triangles:
        addTriangles(object);
        break;
    case Lines:
        addLines(object);
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
    QList<Node>::iterator iter = Nodes.begin();
    while(iter != Nodes.end()) {
        Node& currentNode = *iter;
        if(currentNode.Object.parent() == parentObject &&
            currentNode.Object.id() == id)
        {
            Nodes.erase(iter);
            break;
        } else {
            ++iter;
        }
    }
}

/**
 * @brief cwGeometryItersecter::intersects
 * @param ray
 * @return Closes intersection to on the ray, or if no match, use nearest neighbor search
 */
double cwGeometryItersecter::intersects(const QRay3D &ray) const
{
    QList<double> intersections;

    for(const Node& node : Nodes) {
        double t = node.BoundingBox.intersection(ray);
        if(!qIsNaN(t)) {
            if(node.Object.type() == Triangles) {
                Q_ASSERT(node.Object.indexes().size() % 3 == 0);
                for(int i = 0; i < node.Object.indexes().size(); i+=3) {
                    QBox3D box = Node::triangleToBoundingBox(node.Object, i);
                    t = box.intersection(ray);
                    if(!qIsNaN(t)) {
                        intersections.append(t);
                    }
                }
            } else {
                //Line nodes, are direct
                intersections.append(t);
            }
        }
    }

    //See if we've intersected anything
    if(intersections.size() == 0) {
        return nearestNeighbor(ray); //Do a nearest neighbor search
    }

    //Find the max value int intersections
    double maxValue = -std::numeric_limits<double>::max();
    for(double t : intersections) {
        maxValue = std::max(t, maxValue);
    }

    return maxValue;
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
    if(object.indexes().size() % 3 != 0) {
        qDebug() << "Can't add object" << object.parent() << object.id() << "because it has an invalid indexes" << LOCATION;
        return;
    }

    removeObject(object.parent(), object.id());

    float min = -std::numeric_limits<float>::max();
    float max = -std::numeric_limits<float>::min();

    QVector3D maxPosition(min, min, min);
    QVector3D minPosition(max, max, max);

    for(const QVector3D position : object.points()) {
        maxPosition = QVector3D(std::max(maxPosition.x(), position.x()),
                                std::max(maxPosition.y(), position.y()),
                                std::max(maxPosition.z(), position.z()));
        minPosition = QVector3D(std::min(minPosition.x(), position.x()),
                                std::min(minPosition.y(), position.y()),
                                std::min(minPosition.z(), position.z()));
    }

    //Make sure the object is unique
    Q_ASSERT(std::find_if(Nodes.begin(), Nodes.end(), [object](const Node& node) {
        return node.Object.parent() == object.parent() && node.Object.id() == object.id();
    }) == Nodes.end());
    Nodes.append(Node(QBox3D(minPosition, maxPosition), object));
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
    if(object.indexes().size() % 2 != 0) {
        qDebug() << "Can't add object" << object.parent() << object.id() << "because it has an invalid indexes" << LOCATION;
        return;
    }

    removeObject(object.parent(), object.id());

    for(int i = 0; i < object.indexes().size(); i+=2) {
        Node node = Node(object, i);

        //Make sure the node is valid
        if(!node.BoundingBox.isNull()) {
            Nodes.append(node);
        }
    }
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
        if(node.Object.type() == Triangles) {
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


cwGeometryItersecter::Node::Node(const cwGeometryItersecter::Object &object, int indexInIndexes) :
    Object(object)
{

    switch(object.type()) {
    case Triangles:
        BoundingBox = triangleToBoundingBox(object, indexInIndexes);
        break;
    case Lines:
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
    QVector3D p1 = object.points().at(object.indexes().at(indexInIndexes));
    QVector3D p2 = object.points().at(object.indexes().at(indexInIndexes + 1));
    QVector3D p3 = object.points().at(object.indexes().at(indexInIndexes + 2));

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
    QVector3D p1 = object.points().at(object.indexes().at(indexInIndexes));
    QVector3D p2 = object.points().at(object.indexes().at(indexInIndexes + 1));

    QVector3D maxPoint(qMax(p1.x(), p2.x()),
                       qMax(p1.y(), p2.y()),
                       qMax(p1.z(), p2.z()));

    QVector3D minPoint(qMin(p1.x(), p2.x()),
                       qMin(p1.y(), p2.y()),
                       qMin(p1.z(), p2.z()));

    return QBox3D(minPoint, maxPoint);
}
