/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSGPolygonNode.h"
#include "cwTriangulate.h"

//Qt includes
#include <QSGGeometry>
#include <QSGFlatColorMaterial>

cwSGPolygonNode::cwSGPolygonNode() {
    QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
    material->setFlag(QSGMaterial::Blending, true);
    setMaterial(material);
    setFlags(QSGNode::OwnsMaterial);
    setFlags(QSGNode::OwnsGeometry);
    setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0));
}

void cwSGPolygonNode::setPolygon(const QPolygonF &polygon) {

    //Trianglate the polygon
    QVector<QPointF> results;
    cwTriangulate::Process(polygon, results);

    QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), results.size());
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);

    for(int i = 0; i < results.size(); i++) {
        const QPointF& point = results.at(i);
        geometry->vertexDataAsPoint2D()[i].set(point.x(), point.y());
    }

    setGeometry(geometry);
    markDirty(DirtyGeometry);
}

/**
 * @brief setColor
 * @param color - Set the color for the polygon
 */
void cwSGPolygonNode::setColor(const QColor& color) {
    static_cast<QSGFlatColorMaterial*>(material())->setColor(color);
    markDirty(DirtyMaterial);
}
