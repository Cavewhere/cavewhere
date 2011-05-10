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
    Q_ENUMS(ItemType)
    Q_ENUMS(RoleItem)

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
    Q_INVOKABLE bool setData(const QModelIndex &index, const QVariant &value, int role);
//     Q_INVOKABLE bool setData(const QModelIndex &index, const QVariant &value, QString role);
    Qt::ItemFlags flags ( const QModelIndex & index);

    Q_INVOKABLE cwTrip* trip(const QModelIndex& index) const;
    Q_INVOKABLE cwCave* cave(const QModelIndex& index) const;

signals:

public slots:

private slots:
    void beginInsertCaves(int beginIndex, int endIndex);
    void endInsertCaves(int beginIndex, int endIndex);

    void beginRemoveCaves(int beginIndex, int endIndex);
    void endRemoveCaves(int beginIndex, int endIndex);

    void caveDataChanged();

private:

    cwCavingRegion* Region;

    void addCaveConnections(int beginIndex, int endIndex);
    void removeCaveConnections(int beginIndex, int endIndex);

};


/**
  \brief Called when a cave is beginning to be added
  */
inline void cwRegionTreeModel::beginInsertCaves(int beginIndex, int endIndex) {
    beginInsertRows(QModelIndex(), beginIndex, endIndex);
}

/**
  \brief Cave removed
  */
inline void cwRegionTreeModel::endRemoveCaves(int /*beginIndex*/, int /*endIndex*/) {
    endRemoveRows();
}

#endif // CWREGIONTREEMODEL_H
