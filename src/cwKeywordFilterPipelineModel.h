#ifndef CWKEYWORDFILTERPIPELINEMODEL_H
#define CWKEYWORDFILTERPIPELINEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QPointer>
#include <QConcatenateTablesProxyModel>
#include <QSortFilterProxyModel>
#include <QSet>

//Our includes
class cwKeywordItemModel;
class cwKeywordGroupByKeyModel;
class cwKeywordFilterModel;
class cwUniqueValueFilterModel;
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwKeywordFilterPipelineModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(cwKeywordItemModel* keywordModel READ keywordModel WRITE setKeywordModel NOTIFY keywordModelChanged)
    Q_PROPERTY(QStringList operators READ operators CONSTANT)
    Q_PROPERTY(QAbstractItemModel* acceptedModel READ acceptedModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel* rejectedModel READ rejectedModel CONSTANT)

public:
    enum Role {
        FilterModelObjectRole,
        OperatorRole //OR or AND operator
    };

    enum Operator {
        Or,
        And
    };

    explicit cwKeywordFilterPipelineModel(QObject *parent = nullptr);

    cwKeywordItemModel *keywordModel() const;
    void setKeywordModel(cwKeywordItemModel* keywordModel);

    QAbstractItemModel* acceptedModel() const;
    QAbstractItemModel* rejectedModel() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QHash<int, QByteArray> roleNames() const;

    QStringList operators() const;

    void insertRow(int i);
    void addRow();

    void removeRow(int i );

signals:
    void keywordModelChanged();
    void acceptedEntitiesChanged();

private:
    struct Row {
        cwKeywordGroupByKeyModel* filter;
        Operator modelOperator; //Operators are append before the model.
    };

    QPointer<cwKeywordItemModel> mKeywordModel; //!<
    QStringList mOperators; //!<
    QVector<Row> mRows;

    QConcatenateTablesProxyModel* mAcceptedModel; //!<
    cwUniqueValueFilterModel* mUniqueAcceptedModel;
//    QSet<QObject*> mAccepted;

    cwKeywordFilterModel* mRejectedModel;


    void link(int i);
    void linkPipelineAt(int i);
    void linkLast(int i);
    void linkFirst(int i);

    template<typename F>
    void runAtRow(int i, F func) {
        if(i >= 0 && i < mRows.size()) {
            func(mRows[i]);
        }
    }
};






#endif // CWKEYWORDFILTERPIPELINEMODEL_H
