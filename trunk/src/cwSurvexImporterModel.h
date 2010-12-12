#ifndef CWSURVEXIMPORTERMODEL_H
#define CWSURVEXIMPORTERMODEL_H

//Our includes
class cwSurvexGlobalData;

//Qt includes
#include <QAbstractItemModel>

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

signals:

public slots:

private:
    cwSurvexGlobalData* GlobalData;

};

#endif // CWSURVEXIMPORTERMODEL_H
