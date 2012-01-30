#ifndef CWSURVEXIMPORTERMODEL_H
#define CWSURVEXIMPORTERMODEL_H

//Our includes
class cwSurvexGlobalData;
class cwSurvexBlockData;
class cwShot;

//Qt includes
#include <QAbstractItemModel>
#include <QHash>

class cwSurvexImporterModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit cwSurvexImporterModel(QObject *parent = 0);

    virtual int columnCount ( const QModelIndex & parent = QModelIndex() ) const;
    virtual QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
    virtual QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
    virtual QModelIndex parent ( const QModelIndex & index ) const;
    virtual int rowCount ( const QModelIndex & parent = QModelIndex() ) const;

    void setSurvexData(cwSurvexGlobalData* data);

    cwSurvexBlockData* toBlockData(const QModelIndex& index) const;
    cwShot* toShot(const QModelIndex& index) const;

    QModelIndex toIndex(cwSurvexBlockData* block);
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

    cwSurvexGlobalData* GlobalData;
    QHash<void*, Type> PointerTypeLookup;

    QVariant NameColumnData(const QModelIndex & index, int role) const;
    QVariant NameColumnDisplayData(const QModelIndex& index) const;
    QVariant NameColumnIconData(const QModelIndex& index) const;

    QModelIndex createAndRegisterIndex(int row, void* object, Type type) const;

    void connectBlock(cwSurvexBlockData* block);

private slots:
    void blockDataChanged();
    void shotDataChanged();

};

#endif // CWSURVEXIMPORTERMODEL_H
