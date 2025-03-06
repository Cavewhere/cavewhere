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

        // qDebug() << "m_previousActionPath:" << m_activeLineIndex.value() << m_previousActivePath;
        if(m_previousActivePath >= 0) {
            addLinePolygon(m_finishedPath, m_previousActivePath);
            auto finishedPathIndex = index(FinishedPath);
            emit dataChanged(finishedPathIndex, finishedPathIndex, {PainterPathRole});
        }

        m_previousActivePath = m_activeLineIndex;
    });

}

int PainterPathModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return 2;
}

QVariant PainterPathModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }
    if (role == PainterPathRole) {
        if (index.row() == FinishedPath) {
            return QVariant::fromValue(m_finishedPath);
        } else if (index.row() == ActivePath) {
            return QVariant::fromValue(m_activePath);
        }
    }
    return QVariant();
}

QHash<int, QByteArray> PainterPathModel::roleNames() const {
    return { {PainterPathRole, "painterPath"} };
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
                    } else {
                        addLinePolygon(m_finishedPath, i);
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

void PainterPathModel::addLinePolygon(QPainterPath &path, int modelRow)
{
    //end cap will drawn on p2 with the direction from p1 to p2
    auto cap = [](const QPointF& normal, const QLineF& perpendicularLine, double radius) {
        QVector<QPointF> points;
        const int tessellation = 9;
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

    auto beginCap = [cap](const QVector<PenPoint>& points, const QLineF& perpendicularLine) {
        Q_ASSERT(points.size() >= 2);
        auto firstPoint = points.at(0);
        auto normal = firstPoint.position - points.at(1).position;
        return cap(normal, perpendicularLine, firstPoint.width * 0.5);
    };

    auto endCap = [cap](const QVector<PenPoint>& points, const QLineF& perpendicularLine) {
        Q_ASSERT(points.size() >= 2);
        auto lastIndex = points.size() - 1;
        auto lastPoint = points.at(lastIndex);
        auto normal = lastPoint.position - points.at(lastIndex - 1).position;
        return cap(normal, perpendicularLine, lastPoint.width * 0.5);
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
        points.reserve(perpendicularLines.size() * 2 + 6);

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
    auto linePoints = penLine.points;

    if(linePoints.size() >= 2) {
        // path.addPolygon(polygon(linePoints));

        path.addPath(centerline(linePoints));

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

    double width = std::min(m_maxWidth, mid.width);
    double halfWidth = width * 0.5;

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
    m_activePath.clear();
    addLinePolygon(m_activePath, m_activeLineIndex);
    auto activePathIndex = index(ActivePath);
    emit dataChanged(activePathIndex, activePathIndex, {PainterPathRole});
}

void PainterPathModel::rebuildFinalPath()
{

    // qDebug() << "Rebuilding Final Path!";
    m_finishedPath.clear();
    for(int i = 0; i < m_penLineModel->rowCount(); i++) {
        if(i != m_activeLineIndex) {
            // qDebug() << "Adding finished path:" << i;
            addLinePolygon(m_finishedPath, i);
        }
    }
    auto finishedPathIndex = index(FinishedPath);
    emit dataChanged(finishedPathIndex, finishedPathIndex, {PainterPathRole});
}
