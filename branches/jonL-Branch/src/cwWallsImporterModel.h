#ifndef CWWALLSIMPORTERMODEL_H
#define CWWALLSIMPORTERMODEL_H

//Our includes
class cwWallsGlobalData;
class cwWallsBlockData;
class cwShot;

//Qt includes
#include <QAbstractItemModel>
#include <QHash>

class cwWallsImporterModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit cwWallsImporterModel(QObject *parent = 0);

    virtual int columnCount ( const QModelIndex & parent = QModelIndex() ) const;
    virtual QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
    virtual QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
    virtual QModelIndex parent ( const QModelIndex & index ) const;
    virtual int rowCount ( const QModelIndex & parent = QModelIndex() ) const;

    void setWallsData(cwWallsGlobalData* data);

    cwWallsBlockData* toBlockData(const QModelIndex& index) const;
    cwShot* toShot(const QModelIndex& index) const;

    QModelIndex toIndex(cwWallsBlockData* block);
    QModelIndex toIndex(cwShot* shot);

signals:

public slots:

private:

    enum Columns {
        Name,
        NumberOfColumns
    };

    enum Type {
        Block,
        Shot,
        Invalid
    };

    cwWallsGlobalData* GlobalData;
    QHash<void*, Type> PointerTypeLookup;

    QVariant NameColumnData(const QModelIndex & index, int role) const;
    QVariant NameColumnDisplayData(const QModelIndex& index) const;
    QVariant NameColumnIconData(const QModelIndex& index) const;

    QModelIndex createAndRegisterIndex(int row, void* object, Type type) const;

    void connectBlock(cwWallsBlockData* block);

private slots:
    void blockDataChanged();
    void shotDataChanged();

};

#endif // CWWALLSIMPORTERMODEL_H
