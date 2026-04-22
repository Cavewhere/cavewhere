/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSketchStrokeGeometry.h"

#include <QLineF>
#include <QPolygonF>

#include <cmath>

namespace cwSketchStrokeGeometry {

namespace {

double pressureToLineHalfWidth(const cwPenPoint &p, const Params &params)
{
    Q_ASSERT(p.pressure >= 0.0);
    Q_ASSERT(p.pressure <= 1.0);
    const double interpolated = p.pressure * (params.maxHalfWidth - params.minHalfWidth)
                                + params.minHalfWidth;
    return params.widthScale * interpolated;
}

QLineF perpendicularLineAt(const QVector<cwPenPoint> &points, int index,
                           const Params &params)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < points.size());
    Q_ASSERT(points.size() >= 2);

    const auto left  = index - 1 < 0             ? cwPenPoint() : points.at(index - 1);
    const auto mid   = points.at(index);
    const auto right = index + 1 >= points.size() ? cwPenPoint() : points.at(index + 1);

    QLineF topLine;
    QLineF bottomLine;
    const double halfWidth = pressureToLineHalfWidth(mid, params);

    if (!left.isNull() && !right.isNull()) {
        QLineF leftLine(mid.position, left.position);
        QLineF rightLine(mid.position, right.position);
        const double angleBetween = leftLine.angleTo(rightLine);
        const double halfAngle = angleBetween * 0.5;

        topLine.setP1(mid.position);
        topLine.setAngle(leftLine.angle() + halfAngle);
        topLine.setLength(halfWidth);

        bottomLine.setP1(mid.position);
        bottomLine.setAngle(leftLine.angle() + halfAngle);
        bottomLine.setLength(-halfWidth);
    } else if (!left.isNull() || !right.isNull()) {
        QLineF line;
        double direction = 0.0;
        if (!left.isNull()) {
            line = QLineF(mid.position, left.position);
            direction = 1.0;
        } else {
            line = QLineF(mid.position, right.position);
            direction = -1.0;
        }
        Q_ASSERT(line.length() > 0.0);

        const QLineF normalLine = line.normalVector();
        topLine = normalLine;
        topLine.setLength(direction * halfWidth);
        bottomLine = normalLine;
        bottomLine.setLength(-1.0 * direction * halfWidth);
    } else {
        Q_ASSERT(false);
    }

    return QLineF(topLine.p2(), bottomLine.p2());
}

QVector<QPointF> capFromNormal(const QPointF &normal,
                               const QLineF &perpendicularLine,
                               double radius,
                               int tessellation)
{
    QVector<QPointF> points;
    Q_ASSERT(tessellation >= 3);
    points.reserve(tessellation - 2);

    const QPointF pStart = perpendicularLine.p1();
    const QPointF pEnd   = perpendicularLine.p2();
    const QPointF center((pStart.x() + pEnd.x()) * 0.5,
                         (pStart.y() + pEnd.y()) * 0.5);

    const double angleStart = std::atan2(pStart.y() - center.y(),
                                         pStart.x() - center.x());
    const double candidateAngle = angleStart + M_PI / 2.0;
    const QPointF candidatePoint(center.x() + radius * std::cos(candidateAngle),
                                 center.y() + radius * std::sin(candidateAngle));
    const QPointF candidateOffset(candidatePoint.x() - center.x(),
                                  candidatePoint.y() - center.y());
    const double dot = candidateOffset.x() * normal.x()
                       + candidateOffset.y() * normal.y();
    const bool usePositiveDirection = (dot > 0);

    for (int i = 1; i < tessellation - 1; ++i) {
        const double t = static_cast<double>(i) / (tessellation - 1);
        const double angle = usePositiveDirection
                                 ? angleStart + t * M_PI
                                 : angleStart - t * M_PI;
        const QPointF pt(center.x() + radius * std::cos(angle),
                         center.y() + radius * std::sin(angle));
        if (usePositiveDirection) {
            points.push_front(pt);
        } else {
            points.push_back(pt);
        }
    }
    return points;
}

QVector<QPointF> endCap(const QVector<cwPenPoint> &points,
                        const QLineF &perpendicularLine,
                        const Params &params)
{
    Q_ASSERT(points.size() >= 2);
    const auto lastIndex = points.size() - 1;
    const auto lastPoint = points.at(lastIndex);
    const QPointF normal = lastPoint.position - points.at(lastIndex - 1).position;
    return capFromNormal(normal, perpendicularLine,
                         pressureToLineHalfWidth(lastPoint, params),
                         params.endPointTessellation);
}

} // namespace

void buildPath(QPainterPath &out,
               const QVector<cwPenPoint> &points,
               double width,
               const Params &params)
{
    if (points.size() < 2) {
        return;
    }

    if (width > 0.0) {
        out.moveTo(points.first().position);
        for (int i = 1; i < points.size(); ++i) {
            out.lineTo(points.at(i).position);
        }
        return;
    }

    // Variable-width stroke: tessellate into a filled polygon (begin cap → top
    // edge → end cap → bottom edge).
    QVector<QLineF> perpendicularLines;
    perpendicularLines.reserve(points.size());
    for (int i = 0; i < points.size(); ++i) {
        perpendicularLines.append(perpendicularLineAt(points, i, params));
    }

    QPolygonF polygon;
    polygon.reserve((perpendicularLines.size() + params.endPointTessellation) * 2);

    {
        const auto &first   = points.first();
        const QPointF normal = first.position - points.at(1).position;
        polygon.append(capFromNormal(normal, perpendicularLines.first(),
                                     pressureToLineHalfWidth(first, params),
                                     params.endPointTessellation));
    }

    QVector<QPointF> topLine;
    topLine.reserve(perpendicularLines.size());
    for (const auto &line : perpendicularLines) {
        topLine.append(line.p1());
    }
    polygon.append(topLine);

    polygon.append(endCap(points, perpendicularLines.last(), params));

    QVector<QPointF> bottomLine;
    bottomLine.reserve(perpendicularLines.size());
    for (auto it = perpendicularLines.crbegin(); it != perpendicularLines.crend(); ++it) {
        bottomLine.append(it->p2());
    }
    polygon.append(bottomLine);

    out.addPolygon(polygon);
}

} // namespace cwSketchStrokeGeometry
