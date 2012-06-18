//Our inculdes
#include "cwSGLinesNode.h"

//Qt includes
#include <QSGGeometry>
#include <QSGFlatColorMaterial>
#include <qgl.h>
#include <QLineF>


cwSGLinesNode::cwSGLinesNode()
{
    LineWidth = 1.0;
    QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
    material->setColor(QColor::fromRgbF(0.0, 0.0, 0.0, 1.0));
    setMaterial(material);
    setFlags(QSGNode::OwnsMaterial);
    setFlags(QSGNode::OwnsGeometry);
    setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0));
}

/**
 * @brief cwSGLinesNode::setLines
 * @param lines - Creates the geometry of lines
 */
void cwSGLinesNode::setLines(const QVector<QLineF> &lines)
{
    int numberOfPoints = lines.size() * 2;

//    QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), numberOfPoints);
    geometry()->setDrawingMode(GL_LINES);
    geometry()->setLineWidth(LineWidth);
    geometry()->allocate(numberOfPoints);

    for(int i = 0; i < lines.size(); i++) {
        const QLineF& line = lines.at(i);
        geometry()->vertexDataAsPoint2D()[i * 2].set(line.p1().x(), line.p1().y());
        geometry()->vertexDataAsPoint2D()[i * 2 + 1].set(line.p2().x(), line.p2().y());
    }

//    setGeometry(geometry);
    markDirty(DirtyGeometry);
}

/**
 * @brief cwSGLinesNode::setLines
 * @param points - In a line strip
 *
 * This creates the geometry in a line strip
 */
void cwSGLinesNode::setLineStrip(const QVector<QPointF> &points)
{
//    QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), points.size());
    geometry()->setDrawingMode(GL_LINE_STRIP);
    geometry()->setLineWidth(lineWidth());
    geometry()->allocate(points.size());

    for(int i = 0; i < points.size(); i++) {
        const QPointF& point = points.at(i);
        geometry()->vertexDataAsPoint2D()[i].set(point.x(), point.y());
    }

//    setGeometry(geometry);
    markDirty(DirtyGeometry);
}


/**
 * @brief cwSGLinesNode::setLineWidth
 * @param lineWidth - Sets the line width for the lines node
 */
void cwSGLinesNode::setLineWidth(float lineWidth) {
    if(LineWidth != lineWidth) {
        LineWidth = lineWidth;
        if(geometry() != NULL) {
            geometry()->setLineWidth(LineWidth);
            markDirty(DirtyGeometry);
        }
    }
}

