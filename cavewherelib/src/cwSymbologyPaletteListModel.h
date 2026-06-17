/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYPALETTELISTMODEL_H
#define CWSYMBOLOGYPALETTELISTMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

// moc needs the complete cwSymbologyPaletteManager to register the manager
// property metatype; Q_MOC_INCLUDE keeps it out of this header.
Q_MOC_INCLUDE("cwSymbologyPaletteManager.h")

class cwSymbologyPaletteManager;
class cwSymbologyPalette;

// Flat list of the installed palettes for the palette picker. Bind `manager`
// to cwRootData.paletteManager; the model snapshots manager->palettes() and
// resets whenever the manager rescans (palettesChanged). Read-only — selecting
// a palette is the picker's job (it writes cwCavingRegion::defaultPaletteId),
// not the model's.
class CAVEWHERE_LIB_EXPORT cwSymbologyPaletteListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SymbologyPaletteListModel)

    Q_PROPERTY(cwSymbologyPaletteManager* manager READ manager WRITE setManager NOTIFY managerChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        IdRole
    };
    Q_ENUM(Role)

    explicit cwSymbologyPaletteListModel(QObject *parent = nullptr);

    cwSymbologyPaletteManager *manager() const { return m_manager; }
    void setManager(cwSymbologyPaletteManager *manager);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void managerChanged();

private:
    void refresh();

    cwSymbologyPaletteManager *m_manager = nullptr;
    QMetaObject::Connection m_managerConnection;
    QList<cwSymbologyPalette *> m_palettes;
};

#endif // CWSYMBOLOGYPALETTELISTMODEL_H
