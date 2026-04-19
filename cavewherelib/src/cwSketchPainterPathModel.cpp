/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchPainterPathModel.h"

//Qt includes
#include <QPolygonF>
#include <cmath>

namespace {
constexpr char kPointsRoleName[] = "points";
constexpr char kWidthRoleName[]  = "strokeWidth";
constexpr char kColorRoleName[]  = "color";
} // namespace

cwSketchPainterPathModel::cwSketchPainterPathModel(QObject *parent)
    : cwAbstractSketchPainterPathModel(parent)
{
    connect(this, &cwSketchPainterPathModel::activeStrokeIndexChanged,
            this, &cwSketchPainterPathModel::onActiveStrokeIndexChanged);
}

int cwSketchPainterPathModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_finishedPaths.size() + m_finishLineIndexOffset;
}

QAbstractItemModel *cwSketchPainterPathModel::strokeModel() const
{
    return m_strokeModel;
}

void cwSketchPainterPathModel::setStrokeModel(QAbstractItemModel *model)
{
    if (m_strokeModel == model) {
        return;
    }

    beginResetModel();
    if (m_strokeModel) {
        disconnect(m_strokeModel, nullptr, this, nullptr);
    }

    m_strokeModel = model;
    m_activePath = Path{};
    m_finishedPaths.clear();
    m_previousActiveStroke = -1;

    if (m_strokeModel) {
        resolveRoles();

        connect(m_strokeModel, &QAbstractItemModel::rowsInserted, this,
                &cwSketchPainterPathModel::onSourceRowsInserted);
        connect(m_strokeModel, &QAbstractItemModel::rowsAboutToBeRemoved, this,
                &cwSketchPainterPathModel::onSourceRowsAboutToBeRemoved);
        connect(m_strokeModel, &QAbstractItemModel::dataChanged, this,
                &cwSketchPainterPathModel::onSourceDataChanged);
        connect(m_strokeModel, &QAbstractItemModel::modelReset, this,
                &cwSketchPainterPathModel::onSourceModelReset);

        for (int row = 0; row < m_strokeModel->rowCount(); ++row) {
            if (row == m_activeStrokeIndex) {
                continue;
            }
            addOrUpdateFinishPath(row);
        }
        updateActivePath();
    }

    endResetModel();
    emit strokeModelChanged();
}

void cwSketchPainterPathModel::resolveRoles()
{
    const auto names = m_strokeModel->roleNames();
    m_pointsRole = names.key(kPointsRoleName, -1);
    m_widthRole  = names.key(kWidthRoleName,  -1);
    m_colorRole  = names.key(kColorRoleName,  -1);
    if (m_pointsRole < 0 || m_widthRole < 0 || m_colorRole < 0) {
        qWarning("cwSketchPainterPathModel: source model is missing required "
                 "role names (points/strokeWidth/color)");
    }
}

QVector<cwPenPoint> cwSketchPainterPathModel::strokePoints(int row) const
{
    if (!m_strokeModel || m_pointsRole < 0) {
        return {};
    }
    const QModelIndex idx = m_strokeModel->index(row, 0);
    return idx.data(m_pointsRole).value<QVector<cwPenPoint>>();
}

double cwSketchPainterPathModel::strokeWidth(int row) const
{
    if (!m_strokeModel || m_widthRole < 0) {
        return 0.0;
    }
    return m_strokeModel->index(row, 0).data(m_widthRole).toDouble();
}

QColor cwSketchPainterPathModel::strokeColor(int row) const
{
    if (!m_strokeModel || m_colorRole < 0) {
        return Qt::black;
    }
    QColor c = m_strokeModel->index(row, 0).data(m_colorRole).value<QColor>();
    return c.isValid() ? c : QColor(Qt::black);
}

cwAbstractSketchPainterPathModel::Path
cwSketchPainterPathModel::path(const QModelIndex &index) const
{
    if (index.row() == 0) {
        return m_activePath;
    }
    return m_finishedPaths.at(index.row() - m_finishLineIndexOffset);
}

void cwSketchPainterPathModel::onSourceRowsInserted(const QModelIndex &parent,
                                                    int first, int last)
{
    if (parent.isValid()) {
        return;
    }
    for (int row = first; row <= last; ++row) {
        if (row == m_activeStrokeIndex) {
            updateActivePath();
        } else {
            addOrUpdateFinishPath(row);
        }
    }
}

void cwSketchPainterPathModel::onSourceRowsAboutToBeRemoved(const QModelIndex &parent,
                                                            int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    // Defer the rebuild: batches don't track which source rows contributed to
    // them, so the rebuild has to re-read the source model *after* the remove
    // lands. Iteration 3 will introduce per-batch row tracking for live edits.
    QMetaObject::invokeMethod(this, [this]{ rebuildAllFinished(); }, Qt::QueuedConnection);
}

void cwSketchPainterPathModel::onSourceDataChanged(const QModelIndex &tl,
                                                    const QModelIndex &br,
                                                    const QList<int> &roles)
{
    // Iteration 1 contract: only the active stroke mutates via dataChanged;
    // finished strokes are immutable. Updating a finished batch in-place would
    // re-append geometry and double-count.
    const bool touchesPoints = roles.isEmpty() || roles.contains(m_pointsRole);
    if (!touchesPoints) {
        return;
    }
    if (m_activeStrokeIndex >= tl.row() && m_activeStrokeIndex <= br.row()) {
        updateActivePath();
    }
}

void cwSketchPainterPathModel::onSourceModelReset()
{
    rebuildAllFinished();
    updateActivePath();
}

void cwSketchPainterPathModel::onActiveStrokeIndexChanged()
{
    updateActivePath();
    if (m_previousActiveStroke >= 0 && m_strokeModel
        && m_previousActiveStroke < m_strokeModel->rowCount()) {
        addOrUpdateFinishPath(m_previousActiveStroke);
    }
    m_previousActiveStroke = m_activeStrokeIndex;
}

void cwSketchPainterPathModel::updateActivePath()
{
    m_activePath.painterPath.clear();

    if (m_activeStrokeIndex < 0 || !m_strokeModel
        || m_activeStrokeIndex >= m_strokeModel->rowCount()) {
        m_activePath.strokeWidth = 0.0;
        m_activePath.strokeColor = QColor(Qt::black);
        const QModelIndex rowIdx = index(0);
        emit dataChanged(rowIdx, rowIdx,
                         { PainterPathRole, StrokeWidthRole, StrokeColorRole });
        return;
    }

    const double w = strokeWidth(m_activeStrokeIndex);
    const QColor c = strokeColor(m_activeStrokeIndex);
    m_activePath.strokeWidth = w;
    m_activePath.strokeColor = c;
    buildStrokeGeometry(m_activePath.painterPath, m_activeStrokeIndex);

    const QModelIndex rowIdx = index(0);
    emit dataChanged(rowIdx, rowIdx,
                     { PainterPathRole, StrokeWidthRole, StrokeColorRole });
}

void cwSketchPainterPathModel::rebuildAllFinished()
{
    clearFinished();
    if (!m_strokeModel) {
        return;
    }
    for (int row = 0; row < m_strokeModel->rowCount(); ++row) {
        if (row == m_activeStrokeIndex) {
            continue;
        }
        addOrUpdateFinishPath(row);
    }
}

void cwSketchPainterPathModel::clearFinished()
{
    if (m_finishedPaths.isEmpty()) {
        return;
    }
    const int first = m_finishLineIndexOffset;
    const int last  = m_finishedPaths.size() - 1 + m_finishLineIndexOffset;
    beginRemoveRows(QModelIndex(), first, last);
    m_finishedPaths.clear();
    endRemoveRows();
}

void cwSketchPainterPathModel::addOrUpdateFinishPath(int sourceRow)
{
    if (!m_strokeModel || sourceRow < 0 || sourceRow >= m_strokeModel->rowCount()) {
        return;
    }

    const double width = strokeWidth(sourceRow);
    const QColor color = strokeColor(sourceRow);
    const QRgb   rgba  = color.rgba();

    auto it = std::find_if(m_finishedPaths.begin(), m_finishedPaths.end(),
                           [width, rgba](const Path &p) {
                               return p.strokeWidth == width
                                      && p.strokeColor.rgba() == rgba;
                           });
    if (it != m_finishedPaths.end()) {
        buildStrokeGeometry(it->painterPath, sourceRow);
        const int rowIdx = std::distance(m_finishedPaths.begin(), it)
                           + m_finishLineIndexOffset;
        const QModelIndex modelIdx = index(rowIdx);
        emit dataChanged(modelIdx, modelIdx,
                         { PainterPathRole, StrokeWidthRole, StrokeColorRole });
        return;
    }

    const int newRow = m_finishedPaths.size() + m_finishLineIndexOffset;
    beginInsertRows(QModelIndex(), newRow, newRow);
    Path fresh;
    fresh.strokeWidth = width;
    fresh.strokeColor = color;
    buildStrokeGeometry(fresh.painterPath, sourceRow);
    m_finishedPaths.append(fresh);
    endInsertRows();
}

void cwSketchPainterPathModel::buildStrokeGeometry(QPainterPath &out, int sourceRow) const
{
    const QVector<cwPenPoint> points = strokePoints(sourceRow);
    if (points.size() < 2) {
        return;
    }

    const double width = strokeWidth(sourceRow);
    if (width > 0.0) {
        out.moveTo(points.first().position);
        for (int i = 1; i < points.size(); ++i) {
            out.lineTo(points.at(i).position);
        }
        return;
    }

    // Variable-width stroke: tessellate into a filled polygon (begin cap → top
    // edge → end cap → bottom edge). Each pen point's perpendicular line gives
    // the two polygon vertices at that sample, offset by a half-width derived
    // from pressure. The fill-vs-stroke branch is the same convention the
    // prototype used: width > 0 → stroked centerline; width <= 0 → filled poly.
    QVector<QLineF> perpendicularLines;
    perpendicularLines.reserve(points.size());
    for (int i = 0; i < points.size(); ++i) {
        perpendicularLines.append(perpendicularLineAt(points, i));
    }

    QPolygonF polygon;
    polygon.reserve((perpendicularLines.size() + m_endPointTessellation) * 2);

    {
        const auto &first   = points.first();
        const QPointF normal = first.position - points.at(1).position;
        polygon.append(capFromNormal(normal, perpendicularLines.first(),
                                     pressureToLineHalfWidth(first)));
    }

    QVector<QPointF> topLine;
    topLine.reserve(perpendicularLines.size());
    for (const auto &line : perpendicularLines) {
        topLine.append(line.p1());
    }
    polygon.append(topLine);

    polygon.append(endCap(points, perpendicularLines.last()));

    QVector<QPointF> bottomLine;
    bottomLine.reserve(perpendicularLines.size());
    for (auto it = perpendicularLines.crbegin(); it != perpendicularLines.crend(); ++it) {
        bottomLine.append(it->p2());
    }
    polygon.append(bottomLine);

    out.addPolygon(polygon);
}

double cwSketchPainterPathModel::pressureToLineHalfWidth(const cwPenPoint &p) const
{
    Q_ASSERT(p.pressure >= 0.0);
    Q_ASSERT(p.pressure <= 1.0);
    const double interpolated = p.pressure * (m_maxHalfWidth - m_minHalfWidth)
                                + m_minHalfWidth;
    return m_widthScale * interpolated;
}

QLineF cwSketchPainterPathModel::perpendicularLineAt(const QVector<cwPenPoint> &points,
                                                     int index) const
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < points.size());
    Q_ASSERT(points.size() >= 2);

    const auto left  = index - 1 < 0             ? cwPenPoint() : points.at(index - 1);
    const auto mid   = points.at(index);
    const auto right = index + 1 >= points.size() ? cwPenPoint() : points.at(index + 1);

    QLineF topLine;
    QLineF bottomLine;
    const double halfWidth = pressureToLineHalfWidth(mid);

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

        Q_ASSERT(topLine.p1() == bottomLine.p1());
        Q_ASSERT(topLine.p2() != bottomLine.p2());
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
        Q_ASSERT(false); // need at least 2 points
    }

    return QLineF(topLine.p2(), bottomLine.p2());
}

QVector<QPointF>
cwSketchPainterPathModel::capFromNormal(const QPointF &normal,
                                        const QLineF &perpendicularLine,
                                        double radius) const
{
    QVector<QPointF> points;
    Q_ASSERT(m_endPointTessellation >= 3);
    const int tessellation = m_endPointTessellation;
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

QVector<QPointF>
cwSketchPainterPathModel::endCap(const QVector<cwPenPoint> &points,
                                 const QLineF &perpendicularLine) const
{
    Q_ASSERT(points.size() >= 2);
    const auto lastIndex = points.size() - 1;
    const auto lastPoint = points.at(lastIndex);
    const QPointF normal = lastPoint.position - points.at(lastIndex - 1).position;
    return capFromNormal(normal, perpendicularLine, pressureToLineHalfWidth(lastPoint));
}
