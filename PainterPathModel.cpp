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
    auto polygon = [this](int modelRow) {
        auto penLine = m_penLineModel->data(m_penLineModel->index(modelRow), PenLineModel::LineRole).value<PenLine>();

        //Generate the perpendicularLines that give width to the pen line
        QVector<QLineF> perpendicularLines;
        perpendicularLines.reserve(penLine.points.size());
        for(int i = 1; i < penLine.points.size() - 1; i++) {
            auto line = perpendicularLineAt(penLine.points, i);
            perpendicularLines.append(line);
            // path.moveTo(line.p1());
            // path.lineTo(line.p2());
        }

        // path.move

        //Generate the polygon
        QList<QPointF> points;
        points.reserve(perpendicularLines.size() * 2);

        //Go forward around the top edge of the polygon
        for(const auto& line : perpendicularLines) {
            points.append(line.p1());
        }

        //Go backward around the bottom edge of the polygon
        for(auto it = perpendicularLines.crbegin(); it != perpendicularLines.crend(); ++it) {
            points.append(it->p2());
        }

        return QPolygonF(points);
    };

    path.addPolygon(polygon(modelRow));
}

QLineF PainterPathModel::perpendicularLineAt(const QVector<PenPoint>& points, int index) const
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < points.size());

    const auto left = index - 1 < 0 ? PenPoint() : points.at(index - 1);
    const auto mid = points.at(index);
    const auto right = index + 1 >= points.size() ? PenPoint() : points.at(index + 1);

    QLineF topLine;
    QLineF bottomLine;

    double halfWidth = mid.width * 0.5;

    //    qDebug() << "Null?:" << !left.isNull() << !right.isNull();
    if(!left.isNull() && !right.isNull()) {
        //A middle segment
        QLineF leftLine(mid.position, left.position);
        QLineF rightLine(mid.position, right.position);
        double angleBetween = leftLine.angleTo(rightLine);
        double halfAngle = angleBetween * 0.5;

        //        qDebug() << "Both good!";

        topLine.setP1(mid.position);
        topLine.setAngle(leftLine.angle() + halfAngle);
        topLine.setLength(halfWidth);

        bottomLine.setP1(mid.position);
        bottomLine.setAngle(leftLine.angle() + halfAngle);
        bottomLine.setLength(-halfWidth);

        Q_ASSERT(topLine.p1() == bottomLine.p1());
        Q_ASSERT(topLine.p2() != bottomLine.p2());
    } else {
        QLineF line;
        if(!left.isNull()) {
            //The start section
            line = QLineF(mid.position, right.position);
            //            qDebug() << "left good:" << line;
        } else if(!right.isNull()) {
            line = QLineF(mid.position, left.position);
            //            qDebug() << "Right good:" << line;
        }

        QLineF normalLine = line.normalVector();
        //        qDebug() << "NormalLine:" << normalLine;

        topLine = normalLine;
        topLine.setLength(halfWidth);

        bottomLine = normalLine;
        bottomLine.setLength(-halfWidth);

        Q_ASSERT(topLine.p1() == bottomLine.p1());
        //        Q_ASSERT(topLine.p2() != bottomLine.p2());
    }


    return QLineF(topLine.p2(), bottomLine.p2());
}

void PainterPathModel::updateActivePath()
{
    m_activePath.clear();
    // qDebug() << "Updating active path:" << m_activeLineIndex;
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
