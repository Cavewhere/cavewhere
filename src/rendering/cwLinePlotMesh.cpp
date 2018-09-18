#include "cwLinePlotMesh.h"

#include <QAttribute>
#include <QBuffer>
#include <Qt3DRender/QBuffer>
#include <QVector3D>

using namespace Qt3DCore;
using namespace Qt3DRender;


cwLinePlotMesh::cwLinePlotMesh(Qt3DCore::QNode *parent)
    : QGeometryRenderer(parent)
{
    setGeometry(new QGeometry());

    QAttribute* pointAttribute = new QAttribute();
    pointAttribute->setAttributeType(QAttribute::VertexAttribute);
    pointAttribute->setDataSize(3);
    pointAttribute->setDataType(QAttribute::Float);
    pointAttribute->setByteOffset(0);
    pointAttribute->setBuffer(new Qt3DRender::QBuffer());
    pointAttribute->buffer()->setType(Qt3DRender::QBuffer::VertexBuffer);
    pointAttribute->setName("vertexPosition");

    QAttribute* indexAttribute = new QAttribute();
    indexAttribute->setAttributeType(QAttribute::IndexAttribute);
    indexAttribute->setDataType(QAttribute::UnsignedInt);
    indexAttribute->setBuffer(new Qt3DRender::QBuffer());
    indexAttribute->buffer()->setType(Qt3DRender::QBuffer::IndexBuffer);

    geometry()->addAttribute(pointAttribute);
    geometry()->addAttribute(indexAttribute);

//    setVertexCount(2);
    setPrimitiveType(QGeometryRenderer::Lines);
}

/**
 * @brief cwLinePlotEntity::setPoints
 * @param pointData - Sets the point data for the entity
 */
void cwLinePlotMesh::setPoints(QVector<QVector3D> pointData)
{
    if(!pointData.isEmpty()) {
        PointData = pointData;
        auto pointAttribute = geometry()->attributes().at(PointAttribute);
        const char* points = reinterpret_cast<const char*>(&PointData[0]);
        int size = PointData.size() * sizeof(QVector3D);
        pointAttribute->buffer()->setData(QByteArray(points, size)); //Deep copy
    }
}

/**
 * @brief cwLinePlotEntity::setIndexes
 * @param indexData - Sets the index daat for the entity
 */
void cwLinePlotMesh::setIndexes(QVector<unsigned int> indexData)
{
    if(!indexData.isEmpty()) {
        IndexData = indexData;
        qDebug() << "IndexData:" << IndexData;
        auto indexAttribute = geometry()->attributes().at(IndexAttribute);
        const char* indexes = reinterpret_cast<const char*>(&IndexData[0]);
        int size = IndexData.size() * sizeof(unsigned int);
        indexAttribute->buffer()->setData(QByteArray(indexes, size)); //deep copy
        qDebug() << "data when set:" << indexAttribute->buffer()->data();
        indexAttribute->setCount(indexData.size());
    }
}
