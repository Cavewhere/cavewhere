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
    switch(object.Type) {
    case Triangles:
        addTriangles(object);
        break;
    case Lines:

        break;
    default:
        break;
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

void cwGeometryItersecter::addTriangles(const cwGeometryItersecter::Object &object)
{
    //Make sure the object has the right number of indices
    if(object.Indexes.size() % 3 != 0) {
        qDebug() << "Can't add object" << object.Id << "because it has an invalid indexes" << LOCATION;
        return;
    }

    for(int i = 0; i < object.Indexes.size(); i+=3) {
        Nodes.append(Node(object, i));
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
            double distance = ray.distanceTo(point);
            if(distance < bestDistance) {
                double t = ray.fromPoint(point);
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


    //    QVector<QPlane3D> planes;
//    planes.reserve(6);
//    planes.append(QPlane3D(QVector3D(), QVector3D(1.0, 0.0, 0.0)));
//    planes.append(QPlane3D(QVector3D(), QVector3D(0.0, 1.0, 0.0)));
//    planes.append(QPlane3D(QVector3D(), QVector3D(0.0, 0.0, 1.0)));
//    planes.append(QPlane3D(QVector3D(), QVector3D(1.0, 0.0, 0.0)));
//    planes.append(QPlane3D(QVector3D(), QVector3D(0.0, 1.0, 0.0)));
//    planes.append(QPlane3D(QVector3D(), QVector3D(0.0, 0.0, 1.0)));

//    double t;
//    double bestT = 0.0;

//    foreach(Node node, Nodes) {

//        QVector3D minOrigin = node.BoundingBox.minimum();
//        QVector3D maxOrigin = node.BoundingBox.maximum();

//        for(int i = 0; i < 3; i++) {
//            planes[i].setOrigin(minOrigin);
//        }

//        for(int i = 3; i < 6; i++) {
//            planes[i].setOrigin(maxOrigin);
//        }

//        double minDistance = std::numeric_limits<double>::max();

//        //Loop through all the planes and find the nears point
//        for(int i = 0; i < 6; i++) {
//            if(!qIsNaN(t = planes.at(i).intersection(ray)) && t > 0.0) {
//                QVector3D position = ray.point(t);

//                //Loop through all perpindicular planes
//                int p1 = (i + 1) % 6;
//                int p2 = (i + 2) % 6;
//                int p3 = (i + 4) % 6;
//                int p4 = (i + 5) % 6;

//                //Find the min height
//                double newMinDistance = qMin(fabs(planes.at(p1).distanceTo(position)),
//                                             qMin(fabs(planes.at(p2).distanceTo(position)),
//                                                  qMin(fabs(planes.at(p3).distanceTo(position)),
//                                                       fabs(planes.at(p4).distanceTo(position)))));

//                if(newMinDistance < minDistance) {
//                    bestT = t;
//                }
//            }
//        }
//    }

//    if(bestT == 0.0) {
//        return qSNaN();
//    }

//    qDebug() << "BestT:" << bestT;

//    return bestT;
}


cwGeometryItersecter::Node::Node(const cwGeometryItersecter::Object &object, int indexInIndexes) :
    Object(object),
    IndexInIndexes(indexInIndexes)
{

    switch(object.Type) {
    case Triangles:
        BoundingBox = triangleToBoundingBox();
        break;
    case Lines:

        break;
    default:
        break;
    }


}

QBox3D cwGeometryItersecter::Node::triangleToBoundingBox() const
{
    QVector3D p1 = Object.Points.at(Object.Indexes.at(IndexInIndexes));
    QVector3D p2 = Object.Points.at(Object.Indexes.at(IndexInIndexes + 1));
    QVector3D p3 = Object.Points.at(Object.Indexes.at(IndexInIndexes + 2));

    QVector3D maxPoint(qMax(p1.x(), qMax(p2.x(), p3.x())),
                       qMax(p1.y(), qMax(p2.y(), p3.y())),
                       qMax(p1.z(), qMax(p2.z(), p3.z())));

    QVector3D minPoint(qMin(p1.x(), qMin(p2.x(), p3.x())),
                       qMin(p1.y(), qMin(p2.y(), p3.y())),
                       qMin(p1.z(), qMin(p2.z(), p3.z())));

    return QBox3D(minPoint, maxPoint);
}

/*
   Calculate the line segment PaPb that is the shortest route between
   two lines P1P2 and P3P4. Calculate also the values of mua and mub where
      Pa = P1 + mua (P2 - P1)
      Pb = P3 + mub (P4 - P3)
   Return FALSE if no solution exists.

   Return NaN if no solution, or the square length of the line
*/
//double cwGeometryItersecter::LineLineIntersect(
//   QVector3D p1,QVector3D p2, QVector3D p3, QVector3D p4) const
//{
//   QVector3D p13,p43,p21;
//   double d1343,d4321,d1321,d4343,d2121;
//   double numer,denom;
//   double mua, mub;
//   double EPS = 0.000001;

//   QVector3D pa, pb;

//   p13.setX(p1.x() - p3.x());
//   p13.setY(p1.y() - p3.y());
//   p13.setZ(p1.z() - p3.z());
//   p43.setX(p4.x() - p3.x());
//   p43.setY(p4.y() - p3.y());
//   p43.setZ(p4.z() - p3.z());
//   if (fabs(p43.x()) < EPS && fabs(p43.y()) < EPS && fabs(p43.z()) < EPS)
//      return qSNaN();
//   p21.setX(p2.x() - p1.x());
//   p21.setY(p2.y() - p1.y());
//   p21.setZ(p2.z() - p1.z());
//   if (fabs(p21.x()) < EPS && fabs(p21.y()) < EPS && fabs(p21.z()) < EPS)
//      return qSNaN();

//   d1343 = p13.x() * p43.x() + p13.y() * p43.y() + p13.z() * p43.z();
//   d4321 = p43.x() * p21.x() + p43.y() * p21.y() + p43.z() * p21.z();
//   d1321 = p13.x() * p21.x() + p13.y() * p21.y() + p13.z() * p21.z();
//   d4343 = p43.x() * p43.x() + p43.y() * p43.y() + p43.z() * p43.z();
//   d2121 = p21.x() * p21.x() + p21.y() * p21.y() + p21.z() * p21.z();

//   denom = d2121 * d4343 - d4321 * d4321;
//   if (fabs(denom) < EPS)
//      return qSNaN();
//   numer = d1343 * d4321 - d1321 * d4343;

//   *mua = numer / denom;
//   *mub = (d1343 + d4321 * (*mua)) / d4343;

//   pa.setX(p1.x() + *mua * p21.x());
//   pa.setY(p1.y() + *mua * p21.y());
//   pa.setZ(p1.z() + *mua * p21.z());
//   pb.setX(p3.x() + *mub * p43.x());
//   pb.setY(p3.y() + *mub * p43.y());
//   pb.setZ(p3.z() + *mub * p43.z());

//   double squaredDistance = (pa.x() - pb.x()) * (pa.x() - pb.x()) +
//           (pa.y() - pb.y()) * (pa.y() - pb.y()) +
//           (pa.z() - pb.z()) * (pa.z() - pb.z());
//   return squaredDistance;
//}

