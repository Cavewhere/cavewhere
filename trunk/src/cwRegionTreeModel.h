#ifndef CWREGIONTREEMODEL_H
#define CWREGIONTREEMODEL_H

//Our includes
class cwCavingRegion;

//Qt includes
#include <QAbstractItemModel>

class cwRegionTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit cwRegionTreeModel(QObject *parent = 0);

    void setCavingRegion(cwCavingRegion* region);

    QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
    QModelIndex parent ( const QModelIndex & index ) const;
    int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
    int columnCount ( const QModelIndex & parent = QModelIndex() ) const;
    QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;



signals:

public slots:

private slots:
    void insertCaves(int beginIndex, int endIndex);

private:

    cwCavingRegion* Region;

};

#endif // CWREGIONTREEMODEL_H
