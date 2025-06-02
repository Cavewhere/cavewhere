#include "AbstractPainterPathModel.h"

using namespace cwSketch;

QHash<int, QByteArray> AbstractPainterPathModel::roleNames() const {
    return { {PainterPathRole, "painterPath"},
        {StrokeWidthRole, "strokeWidthRole"},
        {StrokeColorRole, "strokeColorRole"},
        {ZRole, "zRole"},
    };
}

QVariant AbstractPainterPathModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }

    switch(role) {
    case PainterPathRole:
        return QVariant::fromValue(path(index).painterPath);
    case StrokeWidthRole:
        return path(index).strokeWidth;
    case StrokeColorRole:
        return path(index).strokeColor;
    case ZRole:
        return path(index).z;
    }

    return QVariant();
}
