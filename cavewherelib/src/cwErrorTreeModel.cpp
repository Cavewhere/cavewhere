/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwErrorTreeModel.h"
#include "cwErrorModel.h"
#include "cwErrorTreeListModel.h"

//Std includes
#include <array>
#include <utility>

namespace {

constexpr int kSuppressedRole = static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole);
constexpr int kErrorTypeIdRole = static_cast<int>(cwErrorListModel::ErrorRoles::ErrorTypeIdRole);
constexpr int kErrorTypeRole = static_cast<int>(cwErrorListModel::ErrorRoles::ErrorTypeRole);

enum class Kind {
    Disagreement,
    MissingBacksight,
    MissingReading,
    MissingLrud,
    Other
};

//Rare, serious kinds first: a big backsight disagreement is the ignored warning a
//reviewer most needs to see, missing LRUDs are the bulk
constexpr std::array<Kind, 5> kKindOrder = {
    Kind::Disagreement,
    Kind::MissingBacksight,
    Kind::MissingReading,
    Kind::MissingLrud,
    Kind::Other
};

struct KindMember {
    cwErrorTypeId typeId;
    Kind kind;
};

//Every warning id that belongs to a kind; any other id is Kind::Other
constexpr std::array<KindMember, 11> kKindMembers = {{
    {cwErrorTypeId::SurveyCompassDisagreement, Kind::Disagreement},
    {cwErrorTypeId::SurveyClinoDisagreement, Kind::Disagreement},
    {cwErrorTypeId::SurveyMissingBackCompass, Kind::MissingBacksight},
    {cwErrorTypeId::SurveyMissingBackClino, Kind::MissingBacksight},
    {cwErrorTypeId::SurveyMissingCompass, Kind::MissingReading},
    {cwErrorTypeId::SurveyMissingClino, Kind::MissingReading},
    {cwErrorTypeId::SurveyEmptyShot, Kind::MissingReading},
    {cwErrorTypeId::SurveyMissingLeft, Kind::MissingLrud},
    {cwErrorTypeId::SurveyMissingRight, Kind::MissingLrud},
    {cwErrorTypeId::SurveyMissingUp, Kind::MissingLrud},
    {cwErrorTypeId::SurveyMissingDown, Kind::MissingLrud}
}};

Kind kindOf(int errorTypeId)
{
    for(const KindMember& member : kKindMembers) {
        if(static_cast<int>(member.typeId) == errorTypeId) {
            return member.kind;
        }
    }
    return Kind::Other;
}

QList<int> idsOf(Kind kind)
{
    QList<int> ids;
    for(const KindMember& member : kKindMembers) {
        if(member.kind == kind) {
            ids.append(static_cast<int>(member.typeId));
        }
    }
    return ids;
}

QString labelOf(Kind kind)
{
    switch(kind) {
    case Kind::Disagreement:
        return cwErrorTreeModel::tr("Backsight disagreement");
    case Kind::MissingBacksight:
        return cwErrorTreeModel::tr("Missing backsight");
    case Kind::MissingReading:
        return cwErrorTreeModel::tr("Missing reading");
    case Kind::MissingLrud:
        return cwErrorTreeModel::tr("Missing LRUD");
    case Kind::Other:
        break;
    }
    return cwErrorTreeModel::tr("Other warnings");
}

}

cwErrorTreeModel::cwErrorTreeModel(QObject* parent) :
    QSortFilterProxyModel(parent),
    m_flat(new cwErrorTreeListModel(this))
{
    //Suppression is the only role that changes in place; ids and messages
    //arrive as new rows. Filtering on it re-filters on every toggle.
    setFilterRole(kSuppressedRole);
    setSourceModel(m_flat);

    connect(m_flat, &QAbstractItemModel::rowsInserted, this, &cwErrorTreeModel::markSummaryDirty);
    connect(m_flat, &QAbstractItemModel::rowsRemoved, this, &cwErrorTreeModel::markSummaryDirty);
    connect(m_flat, &QAbstractItemModel::modelReset, this, &cwErrorTreeModel::markSummaryDirty);
    connect(m_flat, &QAbstractItemModel::layoutChanged, this, &cwErrorTreeModel::markSummaryDirty);
    connect(m_flat, &QAbstractItemModel::dataChanged, this, &cwErrorTreeModel::onFlatDataChanged);
}

void cwErrorTreeModel::setRoot(cwErrorModel* root)
{
    if (m_root == root) {
        return;
    }

    untrackAll();

    m_root = root;

    QList<cwErrorListModel*> lists;
    if (m_root) {
        connect(m_root, &QObject::destroyed, this, &cwErrorTreeModel::clearForDestroyedRoot);

        QList<cwErrorModel*> nodes;
        track(m_root, &nodes);
        lists.reserve(nodes.size());
        for(cwErrorModel* node : std::as_const(nodes)) {
            lists.append(node->errors());
        }
    }
    m_flat->setSources(lists);

    markSummaryDirty();
    emit rootChanged();
}

void cwErrorTreeModel::setIgnoredOnly(bool ignoredOnly)
{
    if (m_ignoredOnly == ignoredOnly) {
        return;
    }
    beginFilterChange();
    m_ignoredOnly = ignoredOnly;
    endFilterChange(Direction::Rows);
    emit ignoredOnlyChanged();
    markSummaryDirty();
}

void cwErrorTreeModel::setKindFilter(const QString& kindFilter)
{
    if (m_kindFilter == kindFilter) {
        return;
    }
    beginFilterChange();
    m_kindFilter = kindFilter;
    endFilterChange(Direction::Rows);
    emit kindFilterChanged();
    markSummaryDirty();
}

QStringList cwErrorTreeModel::allKinds()
{
    QStringList labels;
    labels.reserve(static_cast<qsizetype>(kKindOrder.size()));
    for(Kind kind : kKindOrder) {
        labels.append(labelOf(kind));
    }
    return labels;
}

QList<int> cwErrorTreeModel::typeIdsForKind(const QString& kind)
{
    for(Kind candidate : kKindOrder) {
        if(labelOf(candidate) == kind) {
            return idsOf(candidate);
        }
    }
    return {};
}

QString cwErrorTreeModel::kindForTypeId(int errorTypeId)
{
    return labelOf(kindOf(errorTypeId));
}

QVariant cwErrorTreeModel::data(const QModelIndex& index, int role) const
{
    switch(role) {
    case static_cast<int>(Roles::KindRole):
        return kindForTypeId(QSortFilterProxyModel::data(index, kErrorTypeIdRole).toInt());
    case static_cast<int>(Roles::ErrorModelRole): {
        const cwErrorListModel* list = m_flat->sourceForRow(mapToSource(index).row());
        if(list == nullptr) {
            return {};
        }
        //cwErrorModel parents its own cwErrorListModel
        auto* owner = qobject_cast<cwErrorModel*>(list->parent());
        return QVariant::fromValue(owner);
    }
    default:
        return QSortFilterProxyModel::data(index, role);
    }
}

QHash<int, QByteArray> cwErrorTreeModel::roleNames() const
{
    auto names = QSortFilterProxyModel::roleNames();
    names.insert(static_cast<int>(Roles::KindRole), QByteArrayLiteral("kind"));
    names.insert(static_cast<int>(Roles::ErrorModelRole), QByteArrayLiteral("errorModel"));
    return names;
}

bool cwErrorTreeModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceParent);
    const cwError error = m_flat->errorAt(sourceRow);
    if(!passesSuppressedFilter(error)) {
        return false;
    }
    if(m_kindFilter.isEmpty()) {
        return true;
    }
    return kindForTypeId(error.errorTypeId()) == m_kindFilter;
}

bool cwErrorTreeModel::passesSuppressedFilter(const cwError& error) const
{
    if(m_ignoredOnly) {
        return error.suppressed() && error.type() == cwError::Warning;
    }
    return true;
}

void cwErrorTreeModel::track(cwErrorModel* node, QList<cwErrorModel*>* trackedNodes)
{
    if(node == nullptr || m_tracked.contains(node)) {
        return;
    }

    m_tracked.insert(node, node->errors());
    trackedNodes->append(node);

    connect(node, &cwErrorModel::childModelAdded, this, &cwErrorTreeModel::addChild);
    connect(node, &cwErrorModel::childModelRemoved, this, &cwErrorTreeModel::untrack);
    connect(node, &QObject::destroyed, this, &cwErrorTreeModel::forgetDestroyedNode);

    const QList<cwErrorModel*> children = node->childModels();
    for(cwErrorModel* child : children) {
        track(child, trackedNodes);
    }
}

void cwErrorTreeModel::addChild(cwErrorModel* child)
{
    //A burst of new cells, such as an import into an open trip, joins m_flat
    //in one insert
    track(child, &m_pendingAdds);
    schedulePass();
}

void cwErrorTreeModel::untrack(cwErrorModel* node)
{
    //Runs from childModelRemoved, which a dying cwErrorModel emits from its
    //destructor body, so node and its children are still whole here
    QList<cwErrorListModel*> lists;
    untrackSubtree(node, &lists);
    m_flat->removeSources(lists);
}

void cwErrorTreeModel::untrackSubtree(cwErrorModel* node, QList<cwErrorListModel*>* lists)
{
    //A node still waiting in m_pendingAdds is skipped once it leaves m_tracked
    const auto found = m_tracked.constFind(node);
    if(found == m_tracked.cend()) {
        return;
    }
    if(found.value()) {
        lists->append(found.value());
    }
    m_tracked.erase(found);
    disconnect(node, nullptr, this, nullptr);

    const QList<cwErrorModel*> children = node->childModels();
    for(cwErrorModel* child : children) {
        untrackSubtree(child, lists);
    }
}

void cwErrorTreeModel::untrackAll()
{
    for(auto it = m_tracked.cbegin(); it != m_tracked.cend(); ++it) {
        disconnect(it.key(), nullptr, this, nullptr);
    }
    m_tracked.clear();
    m_pendingAdds.clear();
}

void cwErrorTreeModel::forgetDestroyedNode(QObject* node)
{
    //The cwErrorModel part is gone; the pointer is only a key now. Its list
    //model is a QObject child and outlives this signal.
    const QPointer<cwErrorListModel> list = m_tracked.take(static_cast<cwErrorModel*>(node));
    if(list) {
        m_flat->removeSources({list.data()});
    }
}

void cwErrorTreeModel::clearForDestroyedRoot()
{
    untrackAll();
    m_flat->setSources({});
    markSummaryDirty();
    emit rootChanged();
}

void cwErrorTreeModel::addPendingSources()
{
    const QList<cwErrorModel*> pending = std::exchange(m_pendingAdds, {});
    QList<cwErrorListModel*> lists;
    lists.reserve(pending.size());
    for(cwErrorModel* node : pending) {
        const QPointer<cwErrorListModel> list = m_tracked.value(node);
        if(list) {
            lists.append(list.data());
        }
    }
    m_flat->appendSources(lists);
}

void cwErrorTreeModel::onFlatDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    //Only these roles move count or kinds; message edits are frequent
    const bool affectsFilter = roles.isEmpty()
                               || roles.contains(kSuppressedRole)
                               || roles.contains(kErrorTypeRole)
                               || roles.contains(kErrorTypeIdRole);
    if(affectsFilter) {
        markSummaryDirty();
    }
}

void cwErrorTreeModel::markSummaryDirty()
{
    m_summaryDirty = true;
    schedulePass();
}

void cwErrorTreeModel::schedulePass()
{
    if(m_passScheduled) {
        return;
    }
    m_passScheduled = true;
    QMetaObject::invokeMethod(this, &cwErrorTreeModel::runPass, Qt::QueuedConnection);
}

void cwErrorTreeModel::runPass()
{
    //The flag stays set while the adds land, so their rowsInserted joins this pass
    addPendingSources();
    m_passScheduled = false;
    if(m_summaryDirty) {
        refresh();
    }
}

void cwErrorTreeModel::refresh()
{
    m_summaryDirty = false;

    const int newCount = rowCount();
    if(m_count != newCount) {
        m_count = newCount;
        emit countChanged();
    }

    //Walks the leaf lists directly; a row lookup through m_flat costs a search
    std::array<bool, kKindOrder.size()> present = {};
    const QList<cwErrorListModel*> lists = m_flat->sources();
    for(const cwErrorListModel* list : lists) {
        const QList<cwError> errors = list->toList();
        for(const cwError& error : errors) {
            if(passesSuppressedFilter(error)) {
                present.at(static_cast<size_t>(kindOf(error.errorTypeId()))) = true;
            }
        }
    }

    QStringList newKinds;
    for(Kind kind : kKindOrder) {
        if(present.at(static_cast<size_t>(kind))) {
            newKinds.append(labelOf(kind));
        }
    }

    if(m_kinds != newKinds) {
        m_kinds = newKinds;
        emit kindsChanged();
    }
}
