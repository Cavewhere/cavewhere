#ifndef CWKEYWORDFILTERPIPELINEMODEL_H
#define CWKEYWORDFILTERPIPELINEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QPointer>

//Our includes
class cwKeywordItemModel;
class cwKeywordGroupByKeyModel;
class cwEntityKeywordsModel;
#include "cwEntityAndKeywords.h"

class cwKeywordFilterPipelineModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(cwKeywordItemModel* keywordModel READ keywordModel WRITE setKeywordModel NOTIFY keywordModelChanged)
    Q_PROPERTY(QStringList operators READ operators CONSTANT)
    Q_PROPERTY(QVector<cwEntityAndKeywords> acceptedEntities READ acceptedEntities NOTIFY acceptedEntitiesChanged)


public:
    enum Role {
        FilterModelObjectRole,
        OperatorRole //OR or AND operator
    };

    enum Operator {
        OR,
        AND,
        None
    };

    explicit cwKeywordFilterPipelineModel(QObject *parent = nullptr);

    cwKeywordItemModel *keywordModel() const;
    void setKeywordModel(cwKeywordItemModel* keywordModel);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QHash<int, QByteArray> roleNames() const;

    QStringList operators() const;

    QVector<cwEntityAndKeywords> acceptedEntities() const;

signals:
    void keywordModelChanged();
    void acceptedEntitiesChanged();

private:
    struct Row {
        cwKeywordGroupByKeyModel* filter;
        Operator modelOperator; //Operators are append before the model. The first filter should be None
    };

    QPointer<cwKeywordItemModel> mKeywordModel; //!<
    QStringList mOperators; //!<
    QVector<Row> mRows;
    QVector<cwEntityAndKeywords> mAcceptedEntities; //!<
    cwEntityKeywordsModel* mEntityKeywords;



};


#endif // CWKEYWORDFILTERPIPELINEMODEL_H
