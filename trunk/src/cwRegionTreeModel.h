#ifndef CWREGIONTREEMODEL_H
#define CWREGIONTREEMODEL_H

//Our includes
class cwCavingRegion;
class cwTrip;
class cwCave;

//Qt includes
#include <QAbstractItemModel>
#include <QtGlobal>

class cwRegionTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum RoleItem {
        TypeRole = Qt::UserRole + 1,
        NameRole, //For everything
        DateRole,  //Only valid for trips
        ObjectRole //For exctracting the object
    };

    enum ItemType {
        Cave,
        Trip
    };

    explicit cwRegionTreeModel(QObject *parent = 0);

    void setCavingRegion(cwCavingRegion* region);

    QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
    QModelIndex parent ( const QModelIndex & index ) const;
    int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
    int columnCount ( const QModelIndex & parent = QModelIndex() ) const;
    QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;

    Q_INVOKABLE cwTrip* trip(const QModelIndex& index) const;
    Q_INVOKABLE cwCave* cave(const QModelIndex& index) const;

signals:

public slots:

private slots:
    void insertCaves(int beginIndex, int endIndex);

private:

    cwCavingRegion* Region;

};

#endif // CWREGIONTREEMODEL_H
