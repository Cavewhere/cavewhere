/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPenStrokeModel.h"
#include "cwSketch.h"

cwPenStrokeModel::cwPenStrokeModel(cwSketch *sketch)
    : QAbstractListModel(sketch),
      m_sketch(sketch)
{
    Q_ASSERT(sketch != nullptr);
}

int cwPenStrokeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_sketch->strokes().size();
}

QVariant cwPenStrokeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const auto &strokes = m_sketch->strokes();
    if (index.row() < 0 || index.row() >= strokes.size()) {
        return {};
    }

    const cwPenStroke &stroke = strokes.at(index.row());

    switch (role) {
    case StrokeRole:    return QVariant::fromValue(stroke);
    case PointsRole:    return QVariant::fromValue(stroke.points);
    case BrushNameRole: return stroke.brushName;
    case IdRole:        return stroke.id;
    default:            return {};
    }
}

QHash<int, QByteArray> cwPenStrokeModel::roleNames() const
{
    return {
        { StrokeRole,    "stroke" },
        { PointsRole,    "points" },
        { BrushNameRole, "brushName" },
        { IdRole,        "strokeId" }
    };
}
