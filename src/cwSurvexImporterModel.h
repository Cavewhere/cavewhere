#ifndef CWSURVEXIMPORTERMODEL_H
#define CWSURVEXIMPORTERMODEL_H

//Our includes
class cwSurvexGlobalData;
class cwSurvexBlockData;
class cwShot;
class cwSurveyChunk;

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
    cwSurveyChunk* surveyChunk(cwSurvexBlockData* parentBlock, int shotIndex);

    QModelIndex toIndex(cwSurvexBlockData* block);

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

    QVariant NameColumnData(const QModelIndex & index, int role) const;
    QVariant NameColumnDisplayData(const QModelIndex& index) const;
    QVariant NameColumnIconData(const QModelIndex& index) const;

    void connectBlock(cwSurvexBlockData* block);

    bool isBlock(const QModelIndex& index) const;
    bool isShot(const QModelIndex& index) const;

private slots:
    void blockDataChanged();

};

#endif // CWSURVEXIMPORTERMODEL_H
