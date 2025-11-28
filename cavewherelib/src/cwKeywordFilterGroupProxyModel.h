#ifndef CWKEYWORDFILTERGROUPPROXYMODEL_H
#define CWKEYWORDFILTERGROUPPROXYMODEL_H

// Qt includes
#include <QSortFilterProxyModel>
#include <QQmlEngine>

// Our includes
#include "CaveWhereLibExport.h"
#include "cwKeywordFilterPipelineModel.h"

class CAVEWHERE_LIB_EXPORT cwKeywordFilterGroupProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KeywordFilterGroupProxyModel)
    Q_PROPERTY(int groupIndex READ groupIndex WRITE setGroupIndex NOTIFY groupIndexChanged)

public:
    explicit cwKeywordFilterGroupProxyModel(QObject* parent = nullptr);

    enum ExtraRoles {
        SourceRowRole = Qt::UserRole + 1100
    };
    Q_ENUM(ExtraRoles)

    int groupIndex() const;
    void setGroupIndex(int groupIndex);

    // QAbstractItemModel interface
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setSourceModel(QAbstractItemModel* sourceModel) override;
signals:
    void groupIndexChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    int operatorAt(int row) const;
    int groupIndexForRow(int sourceRow) const;

    int m_groupIndex = 0;
    bool shouldInvalidate = false;
};

#endif // CWKEYWORDFILTERGROUPPROXYMODEL_H
