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
    QVector<QVector<QPointF>> pointLines;
    pointLines.reserve(lines.size());
    for(const auto line : lines) {
        pointLines.append(QVector<QPointF>({line.p1(), line.p2()}));
    }

    setLineStrips(pointLines);
}

void cwSGLinesNode::setLineStrip(const QVector<QPointF> &points)
{
    setLineStrips({points});
}

/**
 * @brief cwSGLinesNode::setLineWidth
 * @param lineWidth - Sets the line width for the lines node
 *
 * Regenerates the geometry with the new line width
 */
void cwSGLinesNode::setLineWidth(double lineWidth) {
    if(m_lineWidth != lineWidth) {
        m_lineWidth = lineWidth;
        if(geometry() != nullptr) {
            // Regenerate the geometry with the new line width
            if (!m_lines.isEmpty()) {
                setLineStrips(m_lines);
            }
        }
    }
}

void cwSGLinesNode::setLineStrips(const QVector<QVector<QPointF> > &potentialLines)
{
    QVector<QVector<QPointF> > lines = potentialLines;
    // Remove empty QVector<QPointF>
    lines.erase(
        std::remove_if(
            lines.begin(),
            lines.end(),
            [](const QVector<QPointF>& line) {
                return line.size() <= 1; // Predicate: true if isn't a line 0 or 1 points
            }
            ),
        lines.end()
        );

    if(lines.isEmpty()) {
        return;
    }

    m_lines = lines;

    struct LineMetadata {
        qsizetype numberOfSegments;
        qsizetype numberOfPoints;
        qsizetype numberOfVertices;
        bool isClosed;
    };

    const qsizetype numberOfPointsOfDegerateTriangle = 2;

    auto [numberOfVertices, linesMetadata] = [](const QVector<QVector<QPointF> > &lines) {
        QVector<LineMetadata> linesMetadata;
        linesMetadata.resize(lines.size());

        //Allocate memory
        auto numberOfVerticesForLine = [](const QVector<QPointF>& points) {
            const int isClosed = points.first() == points.last() ? 1 : 0;
            const int numberOfSegments = points.size();

            //We need to skip the last point because it's the same as the first
            const int numberOfPoints = isClosed ? points.size() - 1 : points.size();
            const int numberOfVertices = numberOfSegments * 2; //2 vertices per segment for triangle strip

            return numberOfVertices;
        };

        int verticesCount = 0;

        Q_ASSERT(lines.size() == linesMetadata.size());
        for(int i = 0; i < lines.size(); i++) {
            const auto& points = lines.at(i);

            bool isClosed = points.first() == points.last() ? 1 : 0;
            auto numberOfSegments = points.size();

            LineMetadata metadata {
                numberOfSegments,
                isClosed ? points.size() - 1 : points.size(),
                numberOfSegments * 2,
                isClosed
            };

            verticesCount += metadata.numberOfVertices;
            linesMetadata[i] = metadata; //Cache the line size
        }

        //Degenerate triangles
        verticesCount += (lines.size() - 1) * numberOfPointsOfDegerateTriangle;

        return std::make_tuple(verticesCount, linesMetadata);
    }(lines);

    const qsizetype totalNumberOfVertices = numberOfVertices;

    geometry()->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    geometry()->allocate(totalNumberOfVertices);

    QSGGeometry::Point2D *vertices = geometry()->vertexDataAsPoint2D();

    auto addLine = [this, vertices, totalNumberOfVertices](const QVector<QPointF>& points, const LineMetadata& metadata, qsizetype offset) {
        float halfWidth = m_lineWidth / 2.0f;

        qsizetype vertexIndex = offset;

        struct LinePoint {
            QVector2D p1;
            QVector2D normal1;
            QVector2D normal2;
        };

        struct Indexes {
            qsizetype p0Index;
            qsizetype p1Index;
            qsizetype p2Index;
        };

        auto wrapIndex = [metadata](qsizetype i) {
            return Indexes({
                (i - 1 + metadata.numberOfPoints) % metadata.numberOfPoints, //p0
                i % metadata.numberOfPoints, //p1
                (i + 1) % metadata.numberOfPoints //p2
            });
        };

        auto linearIndex = [](qsizetype i) {
            return Indexes({
                i - 1,
                i,
                i + 1
            });
        };

        auto addPoint = [&vertices, &vertexIndex, halfWidth, totalNumberOfVertices](LinePoint d)
        {
            // Miter vector
            QVector2D miter = QVector2D(d.normal1 + d.normal2).normalized();
            double miterLength = halfWidth / QVector2D::dotProduct(miter, d.normal1);

            QVector2D miterOffset = miter * miterLength;

            // Quad vertices
            QVector2D v0 = d.p1 - miterOffset;
            QVector2D v1 = d.p1 + miterOffset;

            // Add vertices to the triangle strip
            Q_ASSERT(vertexIndex < totalNumberOfVertices);
            vertices[vertexIndex++].set(v0.x(), v0.y());

            Q_ASSERT(vertexIndex < totalNumberOfVertices);
            vertices[vertexIndex++].set(v1.x(), v1.y());
        };


        auto lineData = [&points](int i, auto wrapFunction) -> LinePoint {
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
        if(metadata.isClosed) {
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
        for (int i = 1; i < metadata.numberOfSegments - 1; ++i) {
            addPoint(lineData(i, linearIndex));
        }

        //Last point
        if(metadata.isClosed) {
            addPoint(lineData(metadata.numberOfSegments - 1, wrapIndex));
        } else {
            //Not connected
            int lastIndex = metadata.numberOfPoints - 1;
            QVector2D p0 = QVector2D(points[lastIndex - 1]);
            QVector2D p1 = QVector2D(points[lastIndex]);
            QVector2D dir1 = QVector2D(p1 - p0).normalized();
            QVector2D normal1(-dir1.y(), dir1.x());
            addPoint({p1, normal1, normal1});
        }

        return vertexIndex;
    };

    Q_ASSERT(lines.size() == linesMetadata.size());
    qsizetype offset = 0;
    for(int i = 0; i < lines.size(); i++) {
        //This could probably be multi-threaded
        offset = addLine(lines.at(i), linesMetadata.at(i), offset);
        if(i < lines.size() - 1) {
            //Not at the end
            offset += numberOfPointsOfDegerateTriangle; //This adds space do degenerate triangles
        }
    }

    //Add degenerated triangles, this allows the line to be split
    offset = 0;
    for(int i = 0; i < lines.size() - 1; i++) {
        offset += linesMetadata.at(i).numberOfVertices;
        // auto p1 = vertices[offset - 1];
        auto p2 = vertices[offset - 1];
        auto offsetNext = offset + numberOfPointsOfDegerateTriangle;
        auto n1 = vertices[offsetNext];

        Q_ASSERT(offset < totalNumberOfVertices);
        vertices[offset++] = p2;
        Q_ASSERT(offset < totalNumberOfVertices);
        vertices[offset++] = n1;
    }

    // qDebug() << "Vertices:";

    // auto degerateTriangle = [](const QSGGeometry::Point2D v1,
    //                          const QSGGeometry::Point2D v2,
    //                          const QSGGeometry::Point2D v3)->double {
    //     return (v1.x == v2.x && v1.y == v2.y)
    //            || (v1.x == v3.x && v1.y == v3.y)
    //            || (v2.x == v3.x && v2.y == v3.y);
    // };

    // for(int i = 2; i < totalNumberOfVertices; i++) {
    //     QSGGeometry::Point2D v1 = vertices[i - 2];
    //     QSGGeometry::Point2D v2 = vertices[i - 1];
    //     QSGGeometry::Point2D v3 = vertices[i];

    //     qDebug() << "[" << v1.x << "," << v1.y
    //              << ";" << v2.x << "," << v2.y
    //              << ";" << v3.x << "," << v3.y
    //              << "] degerate:" << degerateTriangle(v1, v2, v3);
    // }


    markDirty(DirtyGeometry);
}
