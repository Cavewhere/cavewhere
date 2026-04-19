/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwMovingAveragePenStrokeProxy.h"
#include "cwPenStrokeModel.h"
#include "cwPenPoint.h"
#include "cwPenStroke.h"

namespace {

QVector<cwPenPoint> smoothed(const QVector<cwPenPoint> &raw, int window)
{
    if (raw.size() < 3 || window <= 0) {
        return raw;
    }
    QVector<cwPenPoint> out;
    out.reserve(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        const int start = std::max(0, i - window);
        double count = 0.0;
        QPointF sumPos;
        double sumPressure = 0.0;
        for (int r = start; r <= i; ++r) {
            sumPos      += raw[r].position;
            sumPressure += raw[r].pressure;
            count       += 1.0;
        }
        out.append(cwPenPoint(sumPos / count, sumPressure / count, raw[i].timestampMs));
    }
    return out;
}

} // namespace

cwMovingAveragePenStrokeProxy::cwMovingAveragePenStrokeProxy(QObject *parent)
    : QIdentityProxyModel(parent)
{
}

void cwMovingAveragePenStrokeProxy::setWindowSize(int size)
{
    const int clamped = std::max(0, size);
    if (m_windowSize == clamped) {
        return;
    }
    m_windowSize = clamped;
    emit windowSizeChanged();

    if (sourceModel() && sourceModel()->rowCount() > 0) {
        const QModelIndex first = index(0, 0);
        const QModelIndex last  = index(sourceModel()->rowCount() - 1, 0);
        emit dataChanged(first, last,
                         { cwPenStrokeModel::PointsRole, cwPenStrokeModel::StrokeRole });
    }
}

QVariant cwMovingAveragePenStrokeProxy::data(const QModelIndex &proxyIndex, int role) const
{
    if (!proxyIndex.isValid() || !sourceModel()) {
        return QIdentityProxyModel::data(proxyIndex, role);
    }

    switch (role) {
    case cwPenStrokeModel::PointsRole: {
        const auto raw = QIdentityProxyModel::data(proxyIndex, cwPenStrokeModel::PointsRole)
                             .value<QVector<cwPenPoint>>();
        return QVariant::fromValue(smoothed(raw, m_windowSize));
    }
    case cwPenStrokeModel::StrokeRole: {
        cwPenStroke stroke = QIdentityProxyModel::data(proxyIndex, cwPenStrokeModel::StrokeRole)
                                 .value<cwPenStroke>();
        stroke.points = smoothed(stroke.points, m_windowSize);
        return QVariant::fromValue(stroke);
    }
    default:
        return QIdentityProxyModel::data(proxyIndex, role);
    }
}
