#pragma once

#include <QAbstractListModel>
#include <QPainterPath>
#include <QColor>

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
        StrokeColorRole,
    };

    // Make these pure virtual so derived classes must implement them
    QVariant data(const QModelIndex &index, int role) const override;

    // Shared roleNames implementation
    QHash<int, QByteArray> roleNames() const override;

protected:
    struct Path {
        QPainterPath painterPath;
        double strokeWidth;
        QColor strokeColor;
    };

    //Subclass should implement this to access the current path
    virtual Path path(const QModelIndex& index) const = 0;
};

};
