// Std includes
#include <algorithm>

// Our includes
#include "cwKeywordFilterOrGroupProxyModel.h"

cwKeywordFilterOrGroupProxyModel::cwKeywordFilterOrGroupProxyModel(QObject* parent)
    : QAbstractProxyModel(parent)
{}

QModelIndex cwKeywordFilterOrGroupProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!sourceModel()) {
        return QModelIndex();
    }

    if(!parent.isValid()) {
        if(row < 0 || row >= mGroups.size()) {
            return QModelIndex();
        }
        return createIndex(row, column, GroupInternalId);
    }

    if(isGroupIndex(parent)) {
        const auto group = mGroups.at(parent.row());
        if(row < 0 || row >= group.count()) {
            return QModelIndex();
        }

        const auto sourceRow = group.start + row;
        return createIndex(row, column, static_cast<quintptr>(sourceRow + 1));
    }

    return QModelIndex();
}

QModelIndex cwKeywordFilterOrGroupProxyModel::parent(const QModelIndex& child) const
{
    if(!sourceModel() || !child.isValid() || isGroupIndex(child)) {
        return QModelIndex();
    }

    const auto sourceRow = sourceRowFromInternalId(child);
    if(sourceRow < 0) {
        return QModelIndex();
    }

    const auto group = groupForSourceRow(sourceRow);
    if(group < 0) {
        return QModelIndex();
    }

    return createGroupIndex(group);
}

int cwKeywordFilterOrGroupProxyModel::rowCount(const QModelIndex& parent) const
{
    if(!sourceModel()) {
        return 0;
    }

    if(!parent.isValid()) {
        return mGroups.size();
    }

    if(isGroupIndex(parent)) {
        const auto group = mGroups.value(parent.row());
        return group.count();
    }

    return 0;
}

int cwKeywordFilterOrGroupProxyModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    if(auto* src = sourceModel()) {
        return src->columnCount();
    }
    return 0;
}

QVariant cwKeywordFilterOrGroupProxyModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    if(isGroupIndex(index)) {
        switch(role) {
        case IsGroupRole:
            return true;
        case GroupIndexRole:
            return index.row();
        case FirstSourceRowRole:
            return mGroups.value(index.row()).start;
        case LastSourceRowRole:
            return mGroups.value(index.row()).end;
        default:
            return QVariant();
        }
    }

    if(!sourceModel()) {
        return QVariant();
    }

    const auto sourceIndex = mapToSource(index);
    if(!sourceIndex.isValid()) {
        return QVariant();
    }

    if(role == IsGroupRole) {
        return false;
    }

    switch(role) {
    case IsGroupRole:
        return false;
    case GroupIndexRole:
        return parent(index).row();
    case SourceRowRole:
        return sourceIndex.row();
    default:
        return sourceIndex.data(role);
    }
}

QHash<int, QByteArray> cwKeywordFilterOrGroupProxyModel::roleNames() const
{
    QHash<int, QByteArray> names;
    if(auto* src = sourceModel()) {
        names = src->roleNames();
    }

    names.insert(GroupIndexRole, "groupIndex");
    names.insert(IsGroupRole, "isGroup");
    names.insert(FirstSourceRowRole, "firstSourceRow");
    names.insert(LastSourceRowRole, "lastSourceRow");
    names.insert(SourceRowRole, "sourceRow");
    return names;
}

// Qt::ItemFlags cwKeywordFilterOrGroupProxyModel::flags(const QModelIndex& index) const
// {
//     if(isGroupIndex(index)) {
//         return Qt::ItemIsEnabled;
//     }

//     if(auto* src = sourceModel()) {
//         return src->flags(mapToSource(index));
//     }

//     return Qt::NoItemFlags;
// }

void cwKeywordFilterOrGroupProxyModel::setSourceModel(QAbstractItemModel* source)
{
    if(source == QAbstractProxyModel::sourceModel()) {
        return;
    }

    if(auto* current = sourceModel()) {
        disconnect(current, nullptr, this, nullptr);
    }

    beginResetModel();
    QAbstractProxyModel::setSourceModel(source);
    rebuildGroups();
    endResetModel();

    if(source) {
        connect(source, &QAbstractItemModel::modelReset, this, &cwKeywordFilterOrGroupProxyModel::resetAndRebuild);
        connect(source, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& parent, int first, int last) {
            if(parent.isValid()) {
                resetAndRebuild();
                return;
            }
            const auto oldGroups = mGroups;
            const auto alignedOld = adjustForInsert(oldGroups, first, last - first + 1);
            const auto newGroups = computeGroups();
            applyGroupDiff(alignedOld, newGroups);
        });
        connect(source, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex& parent, int first, int last) {
            if(parent.isValid()) {
                resetAndRebuild();
                return;
            }
            const auto oldGroups = mGroups;
            const auto alignedOld = adjustForRemove(oldGroups, first, last - first + 1);
            const auto newGroups = computeGroups();
            applyGroupDiff(alignedOld, newGroups);
        });
        connect(source, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& topLeft,
                                                                       const QModelIndex& bottomRight,
                                                                       const QVector<int>& roles)
        {
            if(operatorChanged(roles)) {
                const auto oldGroups = mGroups;
                const auto newGroups = computeGroups();
                applyGroupDiff(oldGroups, newGroups);
                return;
            }

            emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
        });
    }
}

QModelIndex cwKeywordFilterOrGroupProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if(!sourceModel() || !proxyIndex.isValid() || isGroupIndex(proxyIndex)) {
        return QModelIndex();
    }

    const auto sourceRow = sourceRowFromInternalId(proxyIndex);
    if(sourceRow < 0) {
        return QModelIndex();
    }

    return sourceModel()->index(sourceRow, proxyIndex.column());
}

QModelIndex cwKeywordFilterOrGroupProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if(!sourceModel() || !sourceIndex.isValid()) {
        return QModelIndex();
    }

    const auto group = groupForSourceRow(sourceIndex.row());
    if(group < 0) {
        return QModelIndex();
    }

    const auto& range = mGroups.at(group);
    const auto childRow = sourceIndex.row() - range.start;
    return createIndex(childRow, sourceIndex.column(), static_cast<quintptr>(sourceIndex.row() + 1));
}

QModelIndex cwKeywordFilterOrGroupProxyModel::groupModelIndex(int row) const
{
    if(row < 0 || row >= mGroups.size()) {
        return QModelIndex();
    }
    return createGroupIndex(row);
}

bool cwKeywordFilterOrGroupProxyModel::isGroupIndex(const QModelIndex& index) const
{
    return index.isValid() && index.internalId() == GroupInternalId;
}

int cwKeywordFilterOrGroupProxyModel::sourceRowFromInternalId(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return -1;
    }
    return static_cast<int>(index.internalId()) - 1;
}

int cwKeywordFilterOrGroupProxyModel::groupForSourceRow(int sourceRow) const
{
    for(int i = 0; i < mGroups.size(); ++i) {
        if(mGroups.at(i).contains(sourceRow)) {
            return i;
        }
    }
    return -1;
}

QModelIndex cwKeywordFilterOrGroupProxyModel::createGroupIndex(int row) const
{
    return createIndex(row, 0, GroupInternalId);
}

void cwKeywordFilterOrGroupProxyModel::rebuildGroups()
{
    mGroups = computeGroups();
}

void cwKeywordFilterOrGroupProxyModel::resetAndRebuild()
{
    beginResetModel();
    rebuildGroups();
    endResetModel();
}

bool cwKeywordFilterOrGroupProxyModel::operatorChanged(const QVector<int>& roles) const
{
    return roles.isEmpty() || roles.contains(cwKeywordFilterPipelineModel::OperatorRole);
}

QVector<cwKeywordFilterOrGroupProxyModel::GroupRange> cwKeywordFilterOrGroupProxyModel::computeGroups() const
{
    QVector<GroupRange> groups;

    if(!sourceModel()) {
        return groups;
    }

    const auto* src = sourceModel();
    const auto rowCount = src->rowCount();
    if(rowCount <= 0) {
        return groups;
    }

    auto flushGroup = [&groups](int start, int end) {
        if(start <= end) {
            groups.append({start, end});
        }
    };

    int start = 0;
    for(int row = 0; row < rowCount; ++row) {
        auto idx = src->index(row, 0);
        const auto op = idx.data(cwKeywordFilterPipelineModel::OperatorRole).toInt();
        if(row > 0 && op == cwKeywordFilterPipelineModel::Or) {
            flushGroup(start, row - 1);
            start = row;
        }
    }

    flushGroup(start, rowCount - 1);
    return groups;
}

QVector<cwKeywordFilterOrGroupProxyModel::GroupRange> cwKeywordFilterOrGroupProxyModel::adjustForInsert(const QVector<GroupRange>& groups, int start, int count) const
{
    QVector<GroupRange> adjusted = groups;
    for(auto& g : adjusted) {
        if(g.end < start) {
            continue;
        } else if(g.start >= start) {
            g.start += count;
            g.end += count;
        } else {
            g.end += count;
        }
    }
    return adjusted;
}

QVector<cwKeywordFilterOrGroupProxyModel::GroupRange> cwKeywordFilterOrGroupProxyModel::adjustForRemove(const QVector<GroupRange>& groups, int start, int count) const
{
    QVector<GroupRange> adjusted;
    const int endRemove = start + count - 1;

    for(auto g : groups) {
        if(g.end < start) {
            adjusted.append(g);
            continue;
        }

        if(g.start > endRemove) {
            g.start -= count;
            g.end -= count;
            adjusted.append(g);
            continue;
        }

        // Overlap cases
        if(g.start >= start && g.end <= endRemove) {
            // Entire group removed; keep entry so diff emits removals
            adjusted.append(g);
            continue;
        }

        if(g.start < start && g.end <= endRemove) {
            // Remove tail of group
            g.end = start - 1;
            adjusted.append(g);
            continue;
        }

        if(g.start >= start && g.start <= endRemove && g.end > endRemove) {
            // Remove head of group
            g.start = start;
            g.end -= count;
            adjusted.append(g);
            continue;
        }

        // Removal occurs in the middle of the group; shrink and shift tail left
        g.end -= count;
        adjusted.append(g);
    }

    return adjusted;
}

void cwKeywordFilterOrGroupProxyModel::applyGroupDiff(const QVector<GroupRange>& oldGroups, const QVector<GroupRange>& newGroups)
{
    auto equal = [](const GroupRange& a, const GroupRange& b) {
        return a.start == b.start && a.end == b.end;
    };

    int prefix = 0;
    const int oldSize = oldGroups.size();
    const int newSize = newGroups.size();
    while(prefix < oldSize && prefix < newSize && equal(oldGroups[prefix], newGroups[prefix])) {
        ++prefix;
    }

    int suffix = 0;
    while(suffix + prefix < oldSize && suffix + prefix < newSize &&
          equal(oldGroups[oldSize - 1 - suffix], newGroups[newSize - 1 - suffix])) {
        ++suffix;
    }

    const int removeCount = oldSize - prefix - suffix;
    const int insertCount = newSize - prefix - suffix;

    if(removeCount > 0) {
        beginRemoveRows(QModelIndex(), prefix, prefix + removeCount - 1);
        endRemoveRows();
    }

    if(insertCount > 0) {
        beginInsertRows(QModelIndex(), prefix, prefix + insertCount - 1);
        endInsertRows();
    }

    mGroups = newGroups;
}
