#include "AbstractPainterPathModel.h"

using namespace cwSketch;

QHash<int, QByteArray> AbstractPainterPathModel::roleNames() const {
    return { {PainterPathRole, "painterPath"},
        {StrokeWidthRole, "strokeWidthRole"}
    };
}

QVariant AbstractPainterPathModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }

    // auto path = [this](const QModelIndex& index) -> const Path& {
    //     if (index.row() == 0) {
    //         return m_activePath;
    //     } else  {
    //         return m_finishedPaths.at(index.row() - m_finishLineIndexOffset);
    //     }
    // };

    switch(role) {
    case PainterPathRole:
        return QVariant::fromValue(path(index).painterPath);
    case StrokeWidthRole:
        return path(index).strokeWidth;
    }

    return QVariant();
}
