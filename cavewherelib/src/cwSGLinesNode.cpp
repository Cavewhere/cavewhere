/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our inculdes
#include "cwSGLinesNode.h"

//Qt includes
#include <QSGGeometry>
#include <QSGFlatColorMaterial>
#include <QLineF>


cwSGLinesNode::cwSGLinesNode()
{
    m_lineWidth = 1.0;
    QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
    material->setColor(QColor::fromRgbF(0.0, 0.0, 0.0, 1.0));
    setMaterial(material);
    setFlags(QSGNode::OwnsMaterial);
    setFlags(QSGNode::OwnsGeometry);
    setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0));
}


/**
 * @brief cwSGLinesNode::setLines
 * @param lines - Creates the geometry of lines as thick quads
 *
 * Note: Since individual lines are not connected, using triangle strips is less effective.
 * We will still use triangles for individual lines.
 */
void cwSGLinesNode::setLines(const QVector<QLineF> &lines)
{
    m_lines = lines; // Store the lines for later use

    int numberOfLines = lines.size();
    int numberOfVertices = numberOfLines * 6; // 6 vertices per line (2 triangles per quad)

    geometry()->setDrawingMode(QSGGeometry::DrawTriangles);
    geometry()->allocate(numberOfVertices);

    QSGGeometry::Point2D *vertices = geometry()->vertexDataAsPoint2D();

    int vertexIndex = 0;
    for(int i = 0; i < lines.size(); i++) {
        const QLineF& line = lines.at(i);

        QPointF p1 = line.p1();
        QPointF p2 = line.p2();

        // Compute the normal vector to the line
        QPointF direction = p2 - p1;
        double length = std::sqrt(direction.x() * direction.x() + direction.y() * direction.y());
        if (length == 0)
            continue;
        QPointF normal(-direction.y() / length, direction.x() / length);

        // Half width
        float halfWidth = m_lineWidth / 2.0f;

        // Compute the four corners of the quad
        QPointF offset = normal * halfWidth;

        QPointF v0 = p1 + offset;
        QPointF v1 = p2 + offset;
        QPointF v2 = p2 - offset;
        QPointF v3 = p1 - offset;

        // Add two triangles
        vertices[vertexIndex++].set(v0.x(), v0.y());
        vertices[vertexIndex++].set(v1.x(), v1.y());
        vertices[vertexIndex++].set(v2.x(), v2.y());

        vertices[vertexIndex++].set(v2.x(), v2.y());
        vertices[vertexIndex++].set(v3.x(), v3.y());
        vertices[vertexIndex++].set(v0.x(), v0.y());
    }

    markDirty(DirtyGeometry);
}

void cwSGLinesNode::setLineStrip(const QVector<QPointF> &points)
{
    if (points.size() < 2) {
        return; // Not enough points to create a line strip
    }

    m_lineStripPoints = points; // Store points for later use

    const int isClosed = points.first() == points.last() ? 1 : 0;
    const int numberOfSegments = points.size();

    //We need to skip the last point because it's the same as the first
    const int numberOfPoints = isClosed ? points.size() - 1 : points.size();
    const int numberOfVertices = numberOfSegments * 2; //2 vertices per segment for triangle strip

    geometry()->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    geometry()->allocate(numberOfVertices);

    QSGGeometry::Point2D *vertices = geometry()->vertexDataAsPoint2D();

    float halfWidth = m_lineWidth / 2.0f;

    int vertexIndex = 0;

    struct LinePoint {
        QVector2D p1;
        QVector2D normal1;
        QVector2D normal2;
    };

    struct Indexes {
        int p0Index;
        int p1Index;
        int p2Index;
    };

    auto wrapIndex = [numberOfPoints](int i) {
        return Indexes({
            (i - 1 + numberOfPoints) % numberOfPoints, //p0
            i % numberOfPoints, //p1
            (i + 1) % numberOfPoints //p2
        });
    };

    auto linearIndex = [](int i) {
        return Indexes({
            i - 1,
            i,
            i + 1
        });
    };

    auto addPoint = [&vertices, &vertexIndex, halfWidth](LinePoint d)
    {
        // Miter vector
        QVector2D miter = QVector2D(d.normal1 + d.normal2).normalized();
        double miterLength = halfWidth / QVector2D::dotProduct(miter, d.normal1);

        QVector2D miterOffset = miter * miterLength;

        // Quad vertices
        QVector2D v0 = d.p1 - miterOffset;
        QVector2D v1 = d.p1 + miterOffset;

        // Add vertices to the triangle strip
        vertices[vertexIndex++].set(v0.x(), v0.y());
        vertices[vertexIndex++].set(v1.x(), v1.y());
    };


    auto lineData = [numberOfPoints, &points](int i, auto wrapFunction) -> LinePoint {
        Indexes ii = wrapFunction(i);

        QVector2D p0 = QVector2D(points[ii.p0Index]);
        QVector2D p1 = QVector2D(points[ii.p1Index]);
        QVector2D p2 = QVector2D(points[ii.p2Index]);

        // Calculate direction vectors
        QVector2D dir1 = QVector2D(p1 - p0).normalized();
        QVector2D dir2 = QVector2D(p2 - p1).normalized();

        // Compute normal vectors
        QVector2D normal1(-dir1.y(), dir1.x());
        QVector2D normal2(-dir2.y(), dir2.x());

        return {p1, normal1, normal2};
    };

    //First point
    if(isClosed) {
        addPoint(lineData(0, wrapIndex));
    } else {
        //Not connected
        QVector2D p1 = QVector2D(points[0]);
        QVector2D p2 = QVector2D(points[1]);
        QVector2D dir2 = QVector2D(p2 - p1).normalized();
        QVector2D normal2(-dir2.y(), dir2.x());
        addPoint({p1, normal2, normal2});
    }

    //Middle
    for (int i = 1; i < numberOfSegments - 1; ++i) {
        addPoint(lineData(i, linearIndex));
    }

    //Last point
    if(isClosed) {
        addPoint(lineData(numberOfSegments - 1, wrapIndex));
    } else {
        //Not connected
        int lastIndex = numberOfPoints - 1;
        QVector2D p0 = QVector2D(points[lastIndex - 1]);
        QVector2D p1 = QVector2D(points[lastIndex]);
        QVector2D dir1 = QVector2D(p1 - p0).normalized();
        QVector2D normal1(-dir1.y(), dir1.x());
        addPoint({p1, normal1, normal1});
    }

    //If this fails there's a bug in this function
    Q_ASSERT(vertexIndex == numberOfVertices);

    markDirty(DirtyGeometry);
}

/**
 * @brief cwSGLinesNode::setLineWidth
 * @param lineWidth - Sets the line width for the lines node
 *
 * Regenerates the geometry with the new line width
 */
void cwSGLinesNode::setLineWidth(float lineWidth) {
    if(m_lineWidth != lineWidth) {
        m_lineWidth = lineWidth;
        if(geometry() != nullptr) {
            // Regenerate the geometry with the new line width
            if (!m_lines.isEmpty()) {
                setLines(m_lines);
            }
            else if (!m_lineStripPoints.isEmpty()) {
                setLineStrip(m_lineStripPoints);
            }
        }
    }
}
