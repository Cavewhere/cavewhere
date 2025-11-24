#ifndef CWKEYWORDFILTERORGROUPPROXYMODEL_H
#define CWKEYWORDFILTERORGROUPPROXYMODEL_H

// Qt includes
#include <QAbstractProxyModel>
#include <QQmlEngine>

// Our includes
#include "CaveWhereLibExport.h"
#include "cwKeywordFilterPipelineModel.h"

class CAVEWHERE_LIB_EXPORT cwKeywordFilterOrGroupProxyModel : public QAbstractProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KeywordFilterOrGroupProxyModel)

public:
    explicit cwKeywordFilterOrGroupProxyModel(QObject* parent = nullptr);

    enum ExtraRoles {
        GroupIndexRole = Qt::UserRole + 1000,
        IsGroupRole
    };
    Q_ENUM(ExtraRoles)

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    // Qt::ItemFlags flags(const QModelIndex& index) const override;

    // QAbstractProxyModel interface
    void setSourceModel(QAbstractItemModel* sourceModel) override;
    QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

private:
    struct GroupRange {
        int start = -1;
        int end = -1; // inclusive

        int count() const { return end >= start ? end - start + 1 : 0; }
        bool contains(int row) const { return row >= start && row <= end; }
    };

    enum Internal {
        GroupInternalId = 0
    };

    QVector<GroupRange> mGroups;

    bool isGroupIndex(const QModelIndex& index) const;
    int sourceRowFromInternalId(const QModelIndex& index) const;
    int groupForSourceRow(int sourceRow) const;
    QModelIndex createGroupIndex(int row) const;
    void rebuildGroups();
    void resetAndRebuild();
    bool operatorChanged(const QVector<int>& roles) const;
    QVector<GroupRange> computeGroups() const;
    QVector<GroupRange> adjustForInsert(const QVector<GroupRange>& groups, int start, int count) const;
    QVector<GroupRange> adjustForRemove(const QVector<GroupRange>& groups, int start, int count) const;
    void applyGroupDiff(const QVector<GroupRange>& oldGroups, const QVector<GroupRange>& newGroups);
};

#endif // CWKEYWORDFILTERORGROUPPROXYMODEL_H
