/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPENSTROKEMODEL_H
#define CWPENSTROKEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

class cwSketch;

class CAVEWHERE_LIB_EXPORT cwPenStrokeModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PenStrokeModel)
    QML_UNCREATABLE("cwPenStrokeModel is owned by cwSketch")

public:
    enum Role {
        StrokeRole = Qt::UserRole + 1,
        PointsRole,
        BrushNameRole,
        IdRole
    };
    Q_ENUM(Role)

    explicit cwPenStrokeModel(cwSketch *sketch);

    cwSketch *sketch() const { return m_sketch; }

    int      rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    friend class cwSketch;

    cwSketch *m_sketch; // not owned; sketch owns the model
};

#endif // CWPENSTROKEMODEL_H
