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
void cwGeometryItersecter::clear(cwGLObject *parentObject)
{
    if(parentObject == NULL) {
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
void cwGeometryItersecter::removeObject(cwGLObject *parentObject, uint id)
{
    QList<Node>::iterator iter = Nodes.begin();
    while(iter != Nodes.end()) {
        Node& currentNode = *iter;
        if(currentNode.Object.parent() == parentObject &&
                currentNode.Object.id() == id)
        {
            iter = Nodes.erase(iter);
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

    foreach(Node node, Nodes) {

        double t = node.BoundingBox.intersection(ray);
        if(!qIsNaN(t)) {
            intersections.append(t);
        }
    }

    //See if we've intersected anything
    if(intersections.size() == 0) {
        return nearestNeighbor(ray); //Do a nearest neighbor search
    }

    //Find the max value int intersections
    double maxValue = -std::numeric_limits<double>::max();
    foreach(double t, intersections) {
        maxValue = qMax(t, maxValue);
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

    for(int i = 0; i < object.indexes().size(); i+=3) {
        Nodes.append(Node(object, i));
    }
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

    QVector<QVector3D> points;
    points.resize(8);

    double bestT = 0.0;
    double bestDistance = std::numeric_limits<double>::max();

    foreach(Node node, Nodes) {

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

        for(int i = 0; i < 8; i++) {
            const QVector3D& point = points.at(i);
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
    Object(object),
    IndexInIndexes(indexInIndexes)
{

    switch(object.type()) {
    case Triangles:
        BoundingBox = triangleToBoundingBox();
        break;
    case Lines:
        BoundingBox = lineToBoundingBox();
        break;
    default:
        break;
    }

}

/**
 * @brief cwGeometryItersecter::Node::triangleToBoundingBox
 * @return Creates a bounding box around a triangle
 */
QBox3D cwGeometryItersecter::Node::triangleToBoundingBox() const
{
    QVector3D p1 = Object.points().at(Object.indexes().at(IndexInIndexes));
    QVector3D p2 = Object.points().at(Object.indexes().at(IndexInIndexes + 1));
    QVector3D p3 = Object.points().at(Object.indexes().at(IndexInIndexes + 2));

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
QBox3D cwGeometryItersecter::Node::lineToBoundingBox() const
{
    QVector3D p1 = Object.points().at(Object.indexes().at(IndexInIndexes));
    QVector3D p2 = Object.points().at(Object.indexes().at(IndexInIndexes + 1));

    QVector3D maxPoint(qMax(p1.x(), p2.x()),
                       qMax(p1.y(), p2.y()),
                       qMax(p1.z(), p2.z()));

    QVector3D minPoint(qMin(p1.x(), p2.x()),
                       qMin(p1.y(), p2.y()),
                       qMin(p1.z(), p2.z()));

    return QBox3D(minPoint, maxPoint);
}
