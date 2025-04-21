//Our includes
#include "MovingAverageProxyModel.h"
#include "PenLineModel.h"

//Qt includes
#include <QPointF>


MovingAverageProxyModel::MovingAverageProxyModel(QObject* parent)
    : QIdentityProxyModel(parent) {}

QVariant MovingAverageProxyModel::data(const QModelIndex &idx, int role) const {
    if (!idx.isValid()) {
        return QIdentityProxyModel::data(idx, role);
    }

    auto smoothPenPoint = [this](int row, const QModelIndex& parent) {
        // int half = m_windowSize.value() / 2;
        double count = 0.0;
        QPointF sumPos;
        double sumPr = 0;
        int parentRowCount = sourceModel()->rowCount(parent);

        for (int r = row - m_windowSize.value(); r <= row; ++r) {
            if (r < 0 || r >= parentRowCount) {
                continue;
            }

            QModelIndex src = sourceModel()->index(r, 0, parent);
            PenPoint p = sourceModel()->data(src, PenLineModel::PenPointRole).value<PenPoint>();

            sumPos += p.position;
            sumPr += p.pressure;
            ++count;
        }

        if(count == 0.0) {
            return PenPoint();
        }

        return PenPoint{ sumPos / count, sumPr / count };
    };

    switch(role) {
    case PenLineModel::PenPointRole:
        return QVariant::fromValue(smoothPenPoint(idx.row(), idx.parent()));
    case PenLineModel::LineRole: {
        auto line = sourceModel()->data(idx, PenLineModel::LineRole).value<PenLine>();
        if(line.points.size() > 2) {
            PenLine smoothLine;
            smoothLine.width = line.width;
            smoothLine.points.reserve(line.points.size());
            // qDebug() << "=======================";
            for(int i = 0; i < line.points.size(); ++i) {
                smoothLine.points.append(smoothPenPoint(i, idx));
                // qDebug() << "Smooth:" << smoothLine.points.last().position;
            }

            Q_ASSERT(smoothLine.points.size() == line.points.size());
            return QVariant::fromValue(smoothLine);
        } else {
            return QVariant::fromValue(line);
        }

    }
    }

    return QIdentityProxyModel::data(idx, role);
}
