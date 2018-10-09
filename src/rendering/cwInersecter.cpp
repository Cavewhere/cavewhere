//Our includes
#include "cwInersecter.h"
#include "cwDebug.h"

//Qt includes
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <QRay3D>
#include <QBox3D>
#include <QBuffer>
#include <QBufferDataGenerator>

using namespace Qt3DRender;
using namespace Qt3DCore;

cwInersecter::cwInersecter(QNode *parent) :
    Qt3DCore::QComponent(parent)
{

}

double cwInersecter::intersects(const QRay3D& ray) const
{


    auto createBoundingBoxes = [&]() {
        QVector<QBox3D> boxes;

        /**
          Takes an attribute and figure out the size in byte of the data it contains
          */
        auto byteSize = [](QAttribute* attribute) {
            switch(attribute->vertexBaseType()) {
            case QAttribute::Byte:
            case QAttribute::UnsignedByte:
                return 1;
            case QAttribute::Short:
            case QAttribute::UnsignedShort:
            case QAttribute::HalfFloat:
                return 2;
            case QAttribute::Int:
            case QAttribute::UnsignedInt:
            case QAttribute::Float:
                return 4;
            case QAttribute::Double:
                return 8;
            }
            return 0;
        };

        /**
          Returns the data from an QAttribute
          */
        auto data = [](QAttribute* attribute) {
            QByteArray byteArray = attribute->buffer()->data();
            if(byteArray.isEmpty() && attribute->count() > 0) {
                //Buffer is using a generator instead
                byteArray = attribute->buffer()->dataGenerator()->operator()(); //Generate the data
            }
            return byteArray;
        };

        /**
          Return the uint of the index at i in bytes
          */
        auto pointIndex = [&](QAttribute* indexAttribute, uint i)->uint {
            Q_ASSERT(indexAttribute->vertexSize() == 1);

            int index = indexAttribute->byteOffset() + i * byteSize(indexAttribute) + indexAttribute->byteStride();

            QByteArray byteArray = data(indexAttribute);

            Q_ASSERT(index < byteArray.size());

            const char* data = &(byteArray.constData()[index]);

            switch(indexAttribute->vertexBaseType()) {
            case Qt3DRender::QAttribute::Byte:
                return static_cast<uint>(data[index]);
            case Qt3DRender::QAttribute::UnsignedByte:
                return static_cast<uint>(*reinterpret_cast<const unsigned char*>(data));
            case Qt3DRender::QAttribute::Short:
                return static_cast<uint>(*reinterpret_cast<const short*>(data));
            case Qt3DRender::QAttribute::UnsignedShort:
                return static_cast<uint>(*reinterpret_cast<const unsigned short*>(data));
            case Qt3DRender::QAttribute::Int:
                return static_cast<uint>(*reinterpret_cast<const int*>(data));
            case Qt3DRender::QAttribute::UnsignedInt: {
                return static_cast<uint>(*(reinterpret_cast<const unsigned int*>(data)));
            }
            default:
                Q_ASSERT(false); //Index needs to be int type value
            }
            return 0;
        };

        /**
          Returns the QVector3D at index i
          */
        auto point = [&](QAttribute* pointAttribute, uint i) {
            int index = pointAttribute->byteOffset() + i * byteSize(pointAttribute) * pointAttribute->vertexSize() + pointAttribute->byteStride();

            QByteArray byteArray = data(pointAttribute);

            Q_ASSERT(index < byteArray.size());

            const char* data = &(byteArray.constData()[index]);

            switch(pointAttribute->vertexBaseType()) {
            case Qt3DRender::QAttribute::HalfFloat:
                Q_ASSERT(false); //Not supported
            case Qt3DRender::QAttribute::Float:
                return *reinterpret_cast<const QVector3D*>(data);
            case Qt3DRender::QAttribute::Double: {
                double x = *reinterpret_cast<const double*>(data);
                double y = *reinterpret_cast<const double*>((&data[8]));
                double z = *reinterpret_cast<const double*>((&data[16]));
                return QVector3D(x, y, z);
            }
            default:
                Q_ASSERT(false); //Index needs to be int type value
            }

            return QVector3D();
        };

        /**
          Creates bounding boxes for each triangle in pointsAttribute and indexAttribute
          */
        auto addTriangles = [&](QAttribute* pointsAttribute, QAttribute* indexAttribute) {
            if(indexAttribute->count() == 0) {
                return;
            }

            //Make sure the object has the right number of indices
            if(indexAttribute->count() % 3 != 0) {
                qDebug() << indexAttribute << "has an invalid indexes" << LOCATION;
                return;
            }

            for(uint i = 0; i < indexAttribute->count(); i+=3) {

                uint i0 = pointIndex(indexAttribute, i);
                uint i1 = pointIndex(indexAttribute, i+1);
                uint i2 = pointIndex(indexAttribute, i+2);

                QVector3D p1 = point(pointsAttribute, i0);
                QVector3D p2 = point(pointsAttribute, i1);
                QVector3D p3 = point(pointsAttribute, i2);

                QVector3D maxPoint(qMax(p1.x(), qMax(p2.x(), p3.x())),
                                   qMax(p1.y(), qMax(p2.y(), p3.y())),
                                   qMax(p1.z(), qMax(p2.z(), p3.z())));

                QVector3D minPoint(qMin(p1.x(), qMin(p2.x(), p3.x())),
                                   qMin(p1.y(), qMin(p2.y(), p3.y())),
                                   qMin(p1.z(), qMin(p2.z(), p3.z())));

                boxes.append(QBox3D(minPoint, maxPoint));
            }
        };

        /**
         * @brief cwGeometryItersecter::Node::lineToBoundingBox
         * @return Creates a bounding box around a line
         */
        auto addLines = [&](QAttribute* pointsAttribute, QAttribute* indexAttribute)
        {
            if(indexAttribute->count() == 0) {
                return;
            }

            //Make sure the object has the right number of indices
            if(indexAttribute->count() % 2 != 0) {
                qDebug() << indexAttribute << "has an invalid indexes" << LOCATION;
                return;
            }

            for(uint i = 0; i < indexAttribute->count(); i+=2) {

                uint i0 = pointIndex(indexAttribute, i);
                uint i1 = pointIndex(indexAttribute, i + 1);

                QVector3D p1 = point(pointsAttribute, i0);
                QVector3D p2 = point(pointsAttribute, i1);

                QVector3D maxPoint(qMax(p1.x(), p2.x()),
                                   qMax(p1.y(), p2.y()),
                                   qMax(p1.z(), p2.z()));

                QVector3D minPoint(qMin(p1.x(), p2.x()),
                                   qMin(p1.y(), p2.y()),
                                   qMin(p1.z(), p2.z()));

                boxes.append(QBox3D(minPoint, maxPoint));
            }
        };

        //Go through all the entries and find the geometry and attributes, and creat bounding boxes
        //FIXME: This should go through an aspect?
        foreach(auto entity, entities()) {
            QComponentVector components = entity->components();
            foreach(QComponent* component, components) {
                QGeometryRenderer* geometryRenderer = dynamic_cast<QGeometryRenderer*>(component);
                if(geometryRenderer != nullptr) {
                    QAttribute* pointsAttribute = nullptr;
                    QAttribute* indexAttribute = nullptr;
                    foreach(auto attribute, geometryRenderer->geometry()->attributes()) {
                        switch(attribute->attributeType()) {
                        case QAttribute::IndexAttribute:
                            indexAttribute = attribute;
                            break;
                        case QAttribute::VertexAttribute:
                            if(attribute->vertexSize() == 3 && attribute->name() == QAttribute::defaultPositionAttributeName()) {
                                pointsAttribute = attribute;
                            }
                            break;
                        default:
                            break;
                        }
                    }

                    if(pointsAttribute != nullptr && indexAttribute != nullptr) {
                        switch(geometryRenderer->primitiveType()) {
                        case Qt3DRender::QGeometryRenderer::Lines:
                            addLines(pointsAttribute, indexAttribute);
                            break;
                        case Qt3DRender::QGeometryRenderer::Triangles:
                            addTriangles(pointsAttribute, indexAttribute);
                            break;
                        default:
                            break;
                        }
                    }

                    break;
                }
            }
        }
        return boxes;
    };

    /**
     * @brief cwGeometryItersecter::nearestNeighbor
     * @param ray
     * @return Finds the point on the ray that's the nearest neigbor of the ray
     */
    auto nearestNeighbor = [](const QRay3D &ray, const QVector<QBox3D>& boxes)
    {

        QVector<QVector3D> points;
        points.resize(8);

        double bestT = 0.0;
        double bestDistance = std::numeric_limits<double>::max();

        foreach(auto box, boxes) {

            QVector3D min = box.minimum();
            QVector3D max = box.maximum();

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
    };

    auto boxes = createBoundingBoxes();

    QList<double> intersections;

//    qDebug() << "--------------------------------";
    foreach(auto box, boxes) {
//        qDebug() << "Boxes:" << box.minimum() << box.maximum();
        //FIXME: Boxes need to transformed with thier ModelMatrix
        double t = box.intersection(ray);
        if(!qIsNaN(t)) {
            intersections.append(t);
        }
    }

    //See if we've intersected anything
    if(intersections.size() == 0) {
        return nearestNeighbor(ray, boxes); //Do a nearest neighbor search
    }

    //Find the max value int intersections
    double maxValue = -std::numeric_limits<double>::max();
    foreach(double t, intersections) {
        maxValue = qMax(t, maxValue);
    }

    return maxValue;
}
