/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwAbstractSketchPainterPathModel.h"

QHash<int, QByteArray> cwAbstractSketchPainterPathModel::roleNames() const {
    return {
        { PainterPathRole,  "painterPath" },
        { StrokeWidthRole,  "strokeWidthRole" },
        { StrokeColorRole,  "strokeColorRole" },
        { ZRole,            "zRole" },
    };
}

QVariant cwAbstractSketchPainterPathModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    switch (role) {
    case PainterPathRole: return QVariant::fromValue(path(index).painterPath);
    case StrokeWidthRole: return path(index).strokeWidth;
    case StrokeColorRole: return path(index).strokeColor;
    case ZRole:           return path(index).z;
    }

    return {};
}
