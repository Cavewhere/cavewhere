#ifndef CWKEYWORDFILTERORGROUPPROXYMODEL_H
#define CWKEYWORDFILTERORGROUPPROXYMODEL_H

// Qt includes
#include <QSortFilterProxyModel>
#include <QQmlEngine>

// Our includes
#include "CaveWhereLibExport.h"
#include "cwKeywordFilterPipelineModel.h"

class CAVEWHERE_LIB_EXPORT cwKeywordFilterOrGroupProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KeywordFilterOrGroupProxyModel)

public:
    explicit cwKeywordFilterOrGroupProxyModel(QObject* parent = nullptr);

    // Q_INVOKABLE QModelIndex groupModelIndex(int row) const;

    // QAbstractItemModel interface
    // QVariant dat
    //(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    // QHash<int, QByteArray> roleNames() const override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    // void setSourceModel(QAbstractItemModel* sourceModel) override;

// private:
//     int operatorAt(int row) const;
//     void emitDerivedDataChanged();
};

#endif // CWKEYWORDFILTERORGROUPPROXYMODEL_H
