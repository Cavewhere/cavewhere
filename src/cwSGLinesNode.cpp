//Our inculdes
#include "cwSGLinesNode.h"

//Qt includes
#include <QSGGeometry>
#include <QSGFlatColorMaterial>
#include <qgl.h>
#include <QLineF>


cwSGLinesNode::cwSGLinesNode()
{
    QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
    material->setColor(QColor::fromRgbF(0.0, 0.0, 0.0, 1.0));
    setMaterial(material);
    setFlags(QSGNode::OwnsMaterial);
    setFlags(QSGNode::OwnsGeometry);
}

void cwSGLinesNode::setLines(const QVector<QLineF> &lines)
{
    int numberOfPoints = lines.size() * 2;

    QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), numberOfPoints);
    geometry->setDrawingMode(GL_LINES);
    geometry->setLineWidth(1.0);

    for(int i = 0; i < lines.size(); i++) {
        const QLineF& line = lines.at(i);
        geometry->vertexDataAsPoint2D()[i * 2].set(line.p1().x(), line.p1().y());
        geometry->vertexDataAsPoint2D()[i * 2 + 1].set(line.p2().x(), line.p2().y());
    }

    setGeometry(geometry);
    markDirty(DirtyGeometry);
}

