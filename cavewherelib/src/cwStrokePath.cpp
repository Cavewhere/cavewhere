/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStrokePath.h"

//Qt includes
#include <QLineF>

//Std includes
#include <algorithm>
#include <cmath>
#include <limits>

cwStrokePath::cwStrokePath(const QVector<QPointF> &points) :
    m_points(points)
{
    m_cumulative.resize(points.size());
    double length = 0.0;
    for (int i = 0; i < points.size(); ++i) {
        if (i > 0) {
            length += QLineF(points.at(i - 1), points.at(i)).length();
        }
        m_cumulative[i] = length;
    }
    m_totalLength = length;
}

void cwStrokePath::locate(double s, int &segment, double &t) const
{
    const double clamped = std::clamp(s, 0.0, m_totalLength);

    // First point whose cumulative length is >= clamped; the segment ends there.
    const auto it = std::lower_bound(m_cumulative.constBegin(), m_cumulative.constEnd(), clamped);
    int end = static_cast<int>(it - m_cumulative.constBegin());
    if (end <= 0) {
        end = 1;
    }
    if (end >= m_points.size()) {
        end = m_points.size() - 1;
    }

    segment = end - 1;
    const double segStart = m_cumulative.at(segment);
    const double segLength = m_cumulative.at(end) - segStart;
    t = segLength > 0.0 ? (clamped - segStart) / segLength : 0.0;
}

QPointF cwStrokePath::pointAtArcLength(double s) const
{
    if (m_points.isEmpty()) {
        return QPointF();
    }
    if (isEmpty()) {
        return m_points.first();
    }

    int segment = 0;
    double t = 0.0;
    locate(s, segment, t);
    const QPointF &a = m_points.at(segment);
    const QPointF &b = m_points.at(segment + 1);
    return a + (b - a) * t;
}

QPointF cwStrokePath::tangentAtArcLength(double s) const
{
    if (isEmpty()) {
        return QPointF(1.0, 0.0);
    }

    int segment = 0;
    double t = 0.0;
    locate(s, segment, t);
    const QPointF delta = m_points.at(segment + 1) - m_points.at(segment);
    const double length = std::hypot(delta.x(), delta.y());
    if (length <= 0.0) {
        return QPointF(1.0, 0.0);
    }
    return delta / length;
}

QPointF cwStrokePath::normalAtArcLength(double s) const
{
    const QPointF tangent = tangentAtArcLength(s);
    return QPointF(-tangent.y(), tangent.x());
}

int cwStrokePath::closestSegment(const QPointF &worldPoint, double &t) const
{
    int bestSegment = 0;
    double bestT = 0.0;
    double bestDistanceSquared = std::numeric_limits<double>::max();
    for (int i = 0; i + 1 < m_points.size(); ++i) {
        const QPointF a = m_points.at(i);
        const QPointF b = m_points.at(i + 1);
        const QPointF ab = b - a;
        const double abLengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
        double segmentT = 0.0;
        if (abLengthSquared > 0.0) {
            const QPointF ap = worldPoint - a;
            segmentT = std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / abLengthSquared, 0.0, 1.0);
        }
        const QPointF closest = a + ab * segmentT;
        const QPointF delta = worldPoint - closest;
        const double distanceSquared = delta.x() * delta.x() + delta.y() * delta.y();
        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestSegment = i;
            bestT = segmentT;
        }
    }
    t = bestT;
    return bestSegment;
}

QPointF cwStrokePath::tangentNearPoint(const QPointF &worldPoint) const
{
    if (isEmpty()) {
        return QPointF(1.0, 0.0);
    }

    double t = 0.0;
    const int segment = closestSegment(worldPoint, t);
    const QPointF delta = m_points.at(segment + 1) - m_points.at(segment);
    const double length = std::hypot(delta.x(), delta.y());
    if (length <= 0.0) {
        return QPointF(1.0, 0.0);
    }
    return delta / length;
}

double cwStrokePath::arcLengthNearPoint(const QPointF &worldPoint) const
{
    if (isEmpty()) {
        return 0.0;
    }

    double t = 0.0;
    const int segment = closestSegment(worldPoint, t);
    const double segLength = m_cumulative.at(segment + 1) - m_cumulative.at(segment);
    return m_cumulative.at(segment) + t * segLength;
}
