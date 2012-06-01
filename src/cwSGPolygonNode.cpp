//Our includes
#include "cwSGPolygonNode.h"
#include "cwTriangulate.h"

//Qt includes
#include <QSGGeometry>
#include <QSGFlatColorMaterial>
#include <qgl.h>

cwSGPolygonNode::cwSGPolygonNode() {
    QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
    material->setColor(QColor(255, 0, 0, 100));
    setMaterial(material);
    setFlags(QSGNode::OwnsMaterial);
    setFlags(QSGNode::OwnsGeometry);
}

void cwSGPolygonNode::setPolygon(const QPolygonF &polygon) {

    //Trianglate the polygon
    QVector<QPointF> results;
    cwTriangulate::Process(polygon, results);

    QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), results.size());
    geometry->setDrawingMode(GL_TRIANGLES);

    for(int i = 0; i < results.size(); i++) {
        const QPointF& point = results.at(i);
        geometry->vertexDataAsPoint2D()[i].set(point.x(), point.y());
    }

    setGeometry(geometry);
    markDirty(DirtyGeometry);
}
