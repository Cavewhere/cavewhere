#pragma once

#include <QAbstractListModel>
#include <QPainterPath>

namespace cwSketch {

class AbstractPainterPathModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit AbstractPainterPathModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {}

    enum Roles {
        PainterPathRole = Qt::UserRole + 1,
        StrokeWidthRole,
    };

    // Make these pure virtual so derived classes must implement them
    QVariant data(const QModelIndex &index, int role) const override;

    // Shared roleNames implementation
    QHash<int, QByteArray> roleNames() const override;

protected:
    struct Path {
        QPainterPath painterPath;
        double strokeWidth;
    };

    //Subclass should implement this to access the current path
    virtual const Path& path(const QModelIndex& index) const = 0;
};

};
