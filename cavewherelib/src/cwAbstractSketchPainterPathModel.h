/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWABSTRACTSKETCHPAINTERPATHMODEL_H
#define CWABSTRACTSKETCHPAINTERPATHMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QColor>
#include <QPainterPath>

//Our includes
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwAbstractSketchPainterPathModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit cwAbstractSketchPainterPathModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {}

    enum Roles {
        PainterPathRole = Qt::UserRole + 1,
        StrokeWidthRole,
        StrokeColorRole,
        ZRole,
    };

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

protected:
    struct Path {
        Path() = default;
        Path(const QPainterPath &path,
             QColor strokeColor,
             double strokeWidth,
             double z = 0.0)
            : painterPath(path),
              strokeColor(strokeColor),
              strokeWidth(strokeWidth),
              z(z)
        {}

        QPainterPath painterPath;
        QColor strokeColor;
        double strokeWidth = 1.0;
        double z = 0.0;
    };

    virtual Path path(const QModelIndex &index) const = 0;
};

#endif // CWABSTRACTSKETCHPAINTERPATHMODEL_H
