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
#include "cwSketchPathSource.h"

class CAVEWHERE_LIB_EXPORT cwAbstractSketchPainterPathModel
    : public QAbstractListModel, public cwSketchPathSource
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

    // cwSketchPathSource: adapts the row/role surface to a flat path list so
    // cwSketchPainter has a single code path. Dropped in commit 2.2 once grid
    // and line-plot implement cwSketchPathSource directly.
    QList<Path> paths() const override;

protected:
    virtual Path path(const QModelIndex &index) const = 0;
};

#endif // CWABSTRACTSKETCHPAINTERPATHMODEL_H
