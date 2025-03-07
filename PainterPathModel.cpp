#include "PainterPathModel.h"

//Qt includes
#include <QLineF>
#include <QPolygonF>

PainterPathModel::PainterPathModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_penLineModel(nullptr)
{

    connect(this, &PainterPathModel::activeLineIndexChanged, this, [this]() {
        updateActivePath();

        //Update the stroke width
        updateActivePathStrokeWidth();

        // qDebug() << "m_previousActionPath:" << m_activeLineIndex.value() << m_previousActivePath;
        if(m_previousActivePath >= 0) {
            addOrUpdateFinishPath(m_previousActivePath);
            // addLinePolygon(m_finishedPath, m_previousActivePath);
            // auto finishedPathIndex = index(FinishedPath);
            // emit dataChanged(finishedPathIndex, finishedPathIndex, {PainterPathRole});
        }

        m_previousActivePath = m_activeLineIndex;
    });

}

int PainterPathModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_finishedPaths.size() + m_finishLineIndexOffset;
}

QVariant PainterPathModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }

    auto path = [this](const QModelIndex& index) -> const Path& {
        if (index.row() == 0) {
            return m_activePath;
        } else  {
            return m_finishedPaths.at(index.row() - m_finishLineIndexOffset);
        }
    };

    switch(role) {
    case PainterPathRole:
        return QVariant::fromValue(path(index).painterPath);
    case StrokeWidthRole:
        return path(index).strokeWidth;
    }

    return QVariant();
}

QHash<int, QByteArray> PainterPathModel::roleNames() const {
    return { {PainterPathRole, "painterPath"},
        {StrokeWidthRole, "strokeWidthRole"}
    };
}

PenLineModel* PainterPathModel::penLineModel() const {
    return m_penLineModel;
}

void PainterPathModel::setPenLineModel(PenLineModel* penLineModel) {
    if (m_penLineModel != penLineModel) {
        if(m_penLineModel) {
            disconnect(this, nullptr, m_penLineModel, nullptr);
        }

        m_penLineModel = penLineModel;

        if(m_penLineModel) {
            connect(m_penLineModel, &PenLineModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last) {
                Q_UNUSED(parent);
                Q_ASSERT(first >= 0);
                Q_ASSERT(first < m_penLineModel->rowCount());
                Q_ASSERT(last >= 0);
                Q_ASSERT(last < m_penLineModel->rowCount());
                Q_ASSERT(first <= last);

                for(int i = first; i <= last; ++i) {
                    if(i == m_activeLineIndex) {
                        updateActivePath();
                        updateActivePathStrokeWidth();
                    } else {
                        addOrUpdateFinishPath(i);
                    }
                }
            });

            connect(m_penLineModel, &PenLineModel::dataChanged,
                    this, [this](const QModelIndex &topLeft,
                           const QModelIndex &bottomRight,
                           const QList<int> &roles = QList<int>())
                    {
                        if(roles.contains(PenLineModel::LineRole)) {
                    // qDebug() << "Data changed:" << topLeft.row() << bottomRight.row();
                            if(m_activeLineIndex >= topLeft.row() && m_activeLineIndex <= bottomRight.row()) {
                                updateActivePath();
                            }

                            //Anything other than the active path
                            if(!(m_activeLineIndex == topLeft.row() && m_activeLineIndex == bottomRight.row())) {
                                //Rebuild final path
                                rebuildFinalPath();
                            }
                        }
                    });
        }

        emit penLineModelChanged();
    }
}

double PainterPathModel::pressureToLineHalfWidth(const PenPoint &point) const
{
    double scaledPressure = point.pressure * m_widthScale;
    return std::clamp(scaledPressure, m_minHalfWidth, m_maxHalfWidth);
}

void PainterPathModel::addLinePolygon(QPainterPath &path, int modelRow)
{
    //end cap will drawn on p2 with the direction from p1 to p2
    auto cap = [this](const QPointF& normal, const QLineF& perpendicularLine, double radius) {
        QVector<QPointF> points;
        Q_ASSERT(m_endPointTessellation >= 3);
        const int tessellation = m_endPointTessellation;
        points.reserve(tessellation - 2); //We don't include first and last points

        // The endpoints of the perpendicular line are the start and end of the arc.
        // Their midpoint is the center of the circle (which is at p2).
        const QPointF pStart = perpendicularLine.p1();
        const QPointF pEnd = perpendicularLine.p2();
        const QPointF center((pStart.x() + pEnd.x()) / 2.0, (pStart.y() + pEnd.y()) / 2.0);

        // Compute the starting angle (from center to pStart)
        const double angleStart = std::atan2(pStart.y() - center.y(), pStart.x() - center.x());

        // The half circle has two possible arcs.
        // We pick the one whose midpoint (at angleStart + π/2) lies in the direction of the given normal.
        const double candidateAngle = angleStart + M_PI / 2.0;
        const QPointF candidatePoint(center.x() + radius * std::cos(candidateAngle),
                                     center.y() + radius * std::sin(candidateAngle));

        // Compute the dot product of the candidate offset with the normal vector.
        const QPointF candidateOffset(candidatePoint.x() - center.x(), candidatePoint.y() - center.y());
        const double dot = candidateOffset.x() * normal.x() + candidateOffset.y() * normal.y();
        const bool usePositiveDirection = (dot > 0);

        // Tessellate the half circle (arc of π radians)
        // We create tessellation number of points starting from angleStart to angleStart ± π.
        for (int i = 1; i < tessellation - 1; ++i) {
            const double t = static_cast<double>(i) / (tessellation - 1);
            double angle = 0.0;
            if (usePositiveDirection) {
                angle = angleStart + t * M_PI;
                const QPointF pt(center.x() + radius * std::cos(angle),
                                 center.y() + radius * std::sin(angle));
                points.push_front(pt);
            } else {
                angle = angleStart - t * M_PI;
                const QPointF pt(center.x() + radius * std::cos(angle),
                                 center.y() + radius * std::sin(angle));
                points.push_back(pt);
            }
        }
        return points;
    };

    auto beginCap = [cap, this](const QVector<PenPoint>& points, const QLineF& perpendicularLine) {
        Q_ASSERT(points.size() >= 2);
        auto firstPoint = points.at(0);
        auto normal = firstPoint.position - points.at(1).position;
        return cap(normal, perpendicularLine, pressureToLineHalfWidth(firstPoint));
    };

    auto endCap = [cap, this](const QVector<PenPoint>& points, const QLineF& perpendicularLine) {
        Q_ASSERT(points.size() >= 2);
        auto lastIndex = points.size() - 1;
        auto lastPoint = points.at(lastIndex);
        auto normal = lastPoint.position - points.at(lastIndex - 1).position;
        return cap(normal, perpendicularLine, pressureToLineHalfWidth(lastPoint));
    };

    auto polygon = [this, beginCap, endCap](const QVector<PenPoint>& linePoints) {
        Q_ASSERT(linePoints.size() >= 2);

        //Generate the perpendicularLines that give width to the pen line
        QVector<QLineF> perpendicularLines;
        perpendicularLines.reserve(linePoints.size());
        for(int i = 0; i < linePoints.size(); i++) {
            auto line = perpendicularLineAt(linePoints, i);
            perpendicularLines.append(line);
            // path.moveTo(line.p1());
            // path.lineTo(line.p2());
        }

        // path.move

        //Generate the polygon
        QPolygonF points;
        points.reserve((perpendicularLines.size() + m_endPointTessellation) * 2 );

        //Begin cap
        points.append(beginCap(linePoints, perpendicularLines.at(0)));

        //Go forward around the top edge of the polygon
        // qDebug() << "PerpendicularLines:" << perpendicularLines.size();
        for(const auto& line : perpendicularLines) {
            points.append(line.p1());
        }

        //End cap
        auto lastPerpendicularIndex = perpendicularLines.size() - 1;
        points.append(endCap(linePoints, perpendicularLines.at(lastPerpendicularIndex)));

        //Go backward around the bottom edge of the polygon
        for(auto it = perpendicularLines.crbegin(); it != perpendicularLines.crend(); ++it) {
            points.append(it->p2());
        }

        // qDebug() << "points:" << points;

        return points;
    };

    auto centerline = [](const QVector<PenPoint>& linePoints) {
        QPainterPath path;
        path.reserve(linePoints.size());
        if(linePoints.size() > 0) {
            path.moveTo(linePoints.first().position);
            for(int i = 1; i < linePoints.size(); i++) {
                path.lineTo(linePoints.at(i).position);
            }
        }
        return path;
    };

    //Debugging:
    auto penLine = m_penLineModel->data(m_penLineModel->index(modelRow), PenLineModel::LineRole).value<PenLine>();
    double penWidth = m_penLineModel->data(m_penLineModel->index(modelRow), PenLineModel::LineWidthRole).toDouble();
    auto linePoints = penLine.points;

    if(linePoints.size() >= 2) {
        if(penWidth > 0.0) {
            path.addPath(centerline(linePoints));
        } else {
            path.addPolygon(polygon(linePoints));
        }

        //Generate the perpendicularLines that give width to the pen line
        // QVector<QLineF> perpendicularLines;
        // perpendicularLines.reserve(linePoints.size());
        // for(int i = 0; i < linePoints.size(); i++) {
        //     auto line = perpendicularLineAt(linePoints, i);
        //     // perpendicularLines.append(line);
        //     path.moveTo(line.p1());
        //     path.lineTo(line.p2());
        // }

        // path = path.simplified();
    }

}

QLineF PainterPathModel::perpendicularLineAt(const QVector<PenPoint>& points, int index) const
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < points.size());
    Q_ASSERT(points.size() >= 2);

    const auto left = index - 1 < 0 ? PenPoint() : points.at(index - 1);
    const auto mid = points.at(index);
    const auto right = index + 1 >= points.size() ? PenPoint() : points.at(index + 1);

    QLineF topLine;
    QLineF bottomLine;

    double halfWidth = pressureToLineHalfWidth(mid);

    //    qDebug() << "Null?:" << !left.isNull() << !right.isNull();
    if(!left.isNull() && !right.isNull()) {
        //A middle segment
        QLineF leftLine(mid.position, left.position);
        QLineF rightLine(mid.position, right.position);
        double angleBetween = leftLine.angleTo(rightLine);
        double halfAngle = angleBetween * 0.5;

        // qDebug() << "Both good!";

        topLine.setP1(mid.position);
        topLine.setAngle(leftLine.angle() + halfAngle);
        topLine.setLength(halfWidth);

        bottomLine.setP1(mid.position);
        bottomLine.setAngle(leftLine.angle() + halfAngle);
        bottomLine.setLength(-halfWidth);

        Q_ASSERT(topLine.p1() == bottomLine.p1());
        Q_ASSERT(topLine.p2() != bottomLine.p2());
    } else if(!left.isNull() || !right.isNull()){
        QLineF line;
        double direction;
        if(!left.isNull()) {
            //The start section
            line = QLineF(mid.position, left.position);
            direction = 1.0;
            // qDebug() << "left good:" << line;
        } else if(!right.isNull()) {
            line = QLineF(mid.position, right.position);
            direction = -1.0;
            // qDebug() << "Right good:" << line;
        }

        // qDebug() << "Mid:" << mid.position << right.position << line.length();
        Q_ASSERT(line.length() > 0.0);

        QLineF normalLine = line.normalVector();

        //        qDebug() << "NormalLine:" << normalLine;

        topLine = normalLine;
        topLine.setLength(direction * halfWidth);

        bottomLine = normalLine;
        bottomLine.setLength(-1.0 * direction * halfWidth);

        Q_ASSERT(topLine.p1() == bottomLine.p1());
        //        Q_ASSERT(topLine.p2() != bottomLine.p2());
    } else {
        Q_ASSERT(false); //Not enough data need to have at least 2 points
    }

    // qDebug() << "Line:" << topLine.p2() << bottomLine.p2();
    return QLineF(topLine.p2(), bottomLine.p2());
}

void PainterPathModel::updateActivePath()
{
    // qDebug() << "Update active path!";
    m_activePath.painterPath.clear();
    addLinePolygon(m_activePath.painterPath, m_activeLineIndex);
    auto activePathIndex = index(0);
    // qDebug() << "ActivePath:" << activePathIndex.data(PainterPathRole).value<QPainterPath>().elementCount();
    emit dataChanged(activePathIndex, activePathIndex, {PainterPathRole});
}

void PainterPathModel::updateActivePathStrokeWidth()
{
    auto newStrokeWidth =  m_penLineModel->index(m_activeLineIndex).data(PenLineModel::LineWidthRole).toDouble();
    // qDebug() << "NewStrokWidth:" << newStrokeWidth << m_activePath.strokeWidth;
    if(m_activePath.strokeWidth != newStrokeWidth) {
        m_activePath.strokeWidth = newStrokeWidth;
        auto activePathIndex = index(0);
        emit dataChanged(activePathIndex, activePathIndex, {StrokeWidthRole});
    }
}

void PainterPathModel::rebuildFinalPath()
{
    // m_finishedPaths.clear();
    //Clear the QPainterPaths
    for(auto& path : m_finishedPaths) {
        path.painterPath.clear();
    }

    for(int i = 0; i < m_penLineModel->rowCount(); i++) {
        if(i != m_activeLineIndex) {
            addOrUpdateFinishPath(i);
        }
    }
    // auto finishedPathIndex = index(m_finishedPaths.size() + m_finishLineIndexOffset);

    //Remove unused finished paths
    for(auto it = m_finishedPaths.begin(); it != m_finishedPaths.end(); ++it) {
        if(it->painterPath.isEmpty()) {
            int index = static_cast<int>(it - m_finishedPaths.begin());
            beginRemoveRows(QModelIndex(), index, index);
            it = m_finishedPaths.erase(it);
            endRemoveRows();
        }
    }

}

QList<PainterPathModel::Path>::iterator PainterPathModel::indexOfFinalPaths(double strokeWidth)
{
    auto it = std::find_if(m_finishedPaths.begin(), m_finishedPaths.end(), [strokeWidth](const Path& path) {
        return path.strokeWidth == strokeWidth;
    });
    return it;
}

void PainterPathModel::addOrUpdateFinishPath(int penLineIndex)
{
    // if(penLineIndex != m_activeLineIndex) {
    // qDebug() << "Adding finished path:" << i;
    double lineWidth = m_penLineModel->index(penLineIndex).data(PenLineModel::LineWidthRole).toDouble();
    auto it = indexOfFinalPaths(lineWidth);
    int modelIndexInt = it - m_finishedPaths.begin() + m_finishLineIndexOffset;
    if(it != m_finishedPaths.end()) {
        //Add to existing line
        addLinePolygon(it->painterPath, penLineIndex);
        const auto modelIndex = index(modelIndexInt);
        emit dataChanged(modelIndex, modelIndex, {PainterPathRole});
    } else {
        //Create a new line
        int lastIndex = m_finishedPaths.size() + m_finishLineIndexOffset;
        beginInsertRows(QModelIndex(), lastIndex, lastIndex);
        Path newPath {QPainterPath(), lineWidth};
        m_finishedPaths.append(newPath);
        auto& lastPath = m_finishedPaths.last();
        addLinePolygon(lastPath.painterPath, penLineIndex);
        endInsertRows();
    }
    // }
}


