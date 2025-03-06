//Qt includes
#include <QDebug>
#include <QLineF>

//Our includes
#include "PenLineModel.h"

PenLineModel::PenLineModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int PenLineModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return m_lines.size();
}

QVariant PenLineModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return QVariant();

    const PenLine& line = m_lines.at(index.row());
    if (role == LineRole) {
        return QVariant::fromValue(line);
    }
    return QVariant();
}

QHash<int, QByteArray> PenLineModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[LineRole] = "line";
    return roles;
}

int PenLineModel::addNewLine()
{
    int lastIndex = m_lines.size();
    PenLine line;
    addLine(line);
    Q_ASSERT(lastIndex == m_lines.size() - 1);
    return lastIndex;
}

void PenLineModel::addLine(const PenLine &line) {
    // qDebug() << "-------------- Add line -------------";
    beginInsertRows(QModelIndex(), m_lines.size(), m_lines.size());
    m_lines.append(line);
    endInsertRows();
}

void PenLineModel::addPoint(int lineIndex, PenPoint point)
{
    if (lineIndex < 0 || lineIndex >= m_lines.size()) {
        qWarning() << "addPoint: lineIndex out of bounds:" << lineIndex;
        return;
    }

    if(point.width < 0.1) {
        qDebug() << "Rejected for too small width:" << point.width;
        return;
    }


    // Append the point to the specified PenLine.
    auto& points =  m_lines[lineIndex].points;
    if(!points.isEmpty()) {
        QLineF distanceLine(points.last().position, point.position);
        if(distanceLine.length() <= 0.2) {
            return;
        } else {
            // qDebug() << "Point:" << point.position << point.width;
            points.append(point);
        }
    } else {
        // qDebug() << "Point:" << point.position << point.width;
        points.append(point);
    }

    if(points.size() == 2) {
        points[0].width = points.at(1).width;
    }

    // Notify that the data for this row has changed so that any views update.
    QModelIndex index = this->index(lineIndex);
    emit dataChanged(index, index, { LineRole });
}

void PenLineModel::clear() {
    beginResetModel();
    m_lines.clear();
    endResetModel();
}



