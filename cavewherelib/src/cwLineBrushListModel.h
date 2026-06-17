/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLINEBRUSHLISTMODEL_H
#define CWLINEBRUSHLISTMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwLineBrush.h"

// moc needs the complete cwSymbologyPalette to register the palette property
// metatype; Q_MOC_INCLUDE keeps it out of this header.
Q_MOC_INCLUDE("cwSymbologyPalette.h")

class cwSymbologyPalette;

// Flat list of the brushes in `palette`, sorted for the brush picker (§1881):
// categories ordered by the lowest zOrder among their members (so wall-ish
// groups float to the top), and within a category alphabetically by displayName
// (locale-aware). The isFirstInCategory role marks each group's first row so a
// flat picker can draw a category header without a nested model.
//
// Bind `palette` to cwSketch::resolvedPalette; brushes are copied by value on
// set, so the model never dereferences the palette pointer again (safe across a
// manager reload that deletes the palette object).
class CAVEWHERE_LIB_EXPORT cwLineBrushListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LineBrushListModel)

    Q_PROPERTY(cwSymbologyPalette* palette READ palette WRITE setPalette NOTIFY paletteChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        DisplayNameRole,
        CategoryRole,
        IsFirstInCategoryRole
    };
    Q_ENUM(Role)

    explicit cwLineBrushListModel(QObject *parent = nullptr);

    cwSymbologyPalette *palette() const { return m_palette; }
    void setPalette(cwSymbologyPalette *palette);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // The §1881 ordering, exposed as a free function so it can be unit-tested
    // directly on a brush vector without standing up a model + palette.
    static QVector<cwLineBrush> sortedForPicker(QVector<cwLineBrush> brushes);

signals:
    void paletteChanged();

private:
    void refresh();

    cwSymbologyPalette *m_palette = nullptr;
    QMetaObject::Connection m_paletteConnection;
    QVector<cwLineBrush> m_brushes; // sorted per sortedForPicker()
};

#endif // CWLINEBRUSHLISTMODEL_H
