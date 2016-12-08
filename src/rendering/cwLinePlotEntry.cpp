
//Our includes
#include "cwLinePlotEntry.h"

//Qt includes
#include <QBuffer>
#include <QAttribute>

using namespace Qt3DCore;
using namespace Qt3DRender;

//cwLinePlotEntity::cwLinePlotEntity(QNode* parent) :
//    QEntity(parent),
//    GeometryRenderer(new QGeometryRenderer())
//{
//    GeometryRenderer->setGeometry(new QGeometry());

//    QAttribute* pointAttribute = new QAttribute();
//    pointAttribute->setAttributeType(QAttribute::VertexAttribute);
//    pointAttribute->setDataType(QAttribute::Float);
//    pointAttribute->setBuffer(new QBuffer());

//    QAttribute* indexAttribute = new QAttribute();
//    indexAttribute->setAttributeType(QAttribute::IndexAttribute);
//    pointAttribute->setDataType(QAttribute::UnsignedInt);
//    pointAttribute->setBuffer(new QBuffer());

//    GeometryRenderer->geometry()->addAttribute(pointAttribute);
//    GeometryRenderer->geometry()->addAttribute(indexAttribute);
//    GeometryRenderer->setPrimitiveType(QGeometryRenderer::Lines);

//    addComponent(GeometryRenderer);
//}

///**
// * @brief cwLinePlotEntity::setPoints
// * @param pointData - Sets the point data for the entity
// */
//void cwLinePlotEntity::setPoints(QVector<QVector3D> pointData)
//{
//    auto pointAttribute = GeometryRenderer->geometry()->attributes().at(PointAttribute);
//    char* points = reinterpret_cast<char*>(&pointData[0]);
//    int size = pointData.size() * sizeof(QVector3D);
//    pointAttribute->buffer()->setData(points, size);
//}

///**
// * @brief cwLinePlotEntity::setIndexes
// * @param indexData - Sets the index daat for the entity
// */
//void cwLinePlotEntity::setIndexes(QVector<unsigned int> indexData)
//{
//    auto indexAttribute = GeometryRenderer->geometry()->attributes().at(PointAttribute);
//    char* indexes = reinterpret_cast<char*>(&indexData[0]);
//    int size = indexData.size() * sizeof(unsigned int);
//    indexAttribute->buffer()->setData(indexes, size);
//}
