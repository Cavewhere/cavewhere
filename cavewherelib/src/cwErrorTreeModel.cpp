/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwErrorTreeModel.h"
#include "cwErrorModel.h"

//Qt includes
#include <QConcatenateTablesProxyModel>

//Std includes
#include <array>

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
constexpr std::array<KindMember, 10> kKindMembers = {{
    {cwErrorTypeId::SurveyCompassDisagreement, Kind::Disagreement},
    {cwErrorTypeId::SurveyClinoDisagreement, Kind::Disagreement},
    {cwErrorTypeId::SurveyMissingBackCompass, Kind::MissingBacksight},
    {cwErrorTypeId::SurveyMissingBackClino, Kind::MissingBacksight},
    {cwErrorTypeId::SurveyMissingCompass, Kind::MissingReading},
    {cwErrorTypeId::SurveyMissingClino, Kind::MissingReading},
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
    m_concat(new QConcatenateTablesProxyModel(this))
{
    //Suppression is the only role that changes in place; ids and messages
    //arrive as new rows. Filtering on it re-filters on every toggle.
    setFilterRole(kSuppressedRole);
    setSourceModel(m_concat);

    //Connected after setSourceModel so the proxy has already caught up
    connect(m_concat, &QAbstractItemModel::rowsInserted, this, &cwErrorTreeModel::refresh);
    connect(m_concat, &QAbstractItemModel::rowsRemoved, this, &cwErrorTreeModel::refresh);
    connect(m_concat, &QAbstractItemModel::modelReset, this, &cwErrorTreeModel::refresh);
    connect(m_concat, &QAbstractItemModel::layoutChanged, this, &cwErrorTreeModel::refresh);
    connect(m_concat, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles)
    {
        //Only these roles move count or kinds; message edits are frequent
        const bool affectsFilter = roles.isEmpty()
                                   || roles.contains(kSuppressedRole)
                                   || roles.contains(kErrorTypeRole)
                                   || roles.contains(kErrorTypeIdRole);
        if(affectsFilter) {
            refresh();
        }
    });
}

void cwErrorTreeModel::setRoot(cwErrorModel* root)
{
    if (m_root == root) {
        return;
    }

    if (m_root) {
        disconnect(m_root, &QObject::destroyed, this, nullptr);
    }
    untrackAll();

    m_root = root;

    if (m_root) {
        connect(m_root, &QObject::destroyed, this, [this]() {
            untrackAll();
            refresh();
            emit rootChanged();
        });
        track(m_root);
    }

    refresh();
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
    refresh();
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
    refresh();
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
        const QModelIndex listIndex = m_concat->mapToSource(mapToSource(index));
        if(!listIndex.isValid()) {
            return {};
        }
        //cwErrorModel parents its own cwErrorListModel
        auto* owner = qobject_cast<cwErrorModel*>(listIndex.model()->parent());
        return QVariant::fromValue(owner);
    }
    default:
        return QSortFilterProxyModel::data(index, role);
    }
}

QHash<int, QByteArray> cwErrorTreeModel::roleNames() const
{
    //The concatenation has no roles until its first source arrives, and a
    //QML view reads roles once, so take them from the list model type
    static const QHash<int, QByteArray> kListRoles = cwErrorListModel().roleNames();
    auto names = kListRoles;
    names.insert(static_cast<int>(Roles::KindRole), QByteArrayLiteral("kind"));
    names.insert(static_cast<int>(Roles::ErrorModelRole), QByteArrayLiteral("errorModel"));
    return names;
}

bool cwErrorTreeModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = m_concat->index(sourceRow, 0, sourceParent);
    if(!passesSuppressedFilter(index)) {
        return false;
    }
    if(m_kindFilter.isEmpty()) {
        return true;
    }
    return kindForTypeId(index.data(kErrorTypeIdRole).toInt()) == m_kindFilter;
}

bool cwErrorTreeModel::passesSuppressedFilter(const QModelIndex& concatIndex) const
{
    if(m_ignoredOnly) {
        return concatIndex.data(kSuppressedRole).toBool()
               && concatIndex.data(kErrorTypeRole).toInt() == cwError::Warning;
    }
    return true;
}

void cwErrorTreeModel::track(cwErrorModel* node)
{
    if(node == nullptr || m_tracked.contains(node)) {
        return;
    }

    m_tracked.insert(node, node->errors());
    m_concat->addSourceModel(node->errors());

    connect(node, &cwErrorModel::childModelAdded, this, &cwErrorTreeModel::track);
    connect(node, &cwErrorModel::childModelRemoved, this, &cwErrorTreeModel::untrack);
    connect(node, &QObject::destroyed, this, &cwErrorTreeModel::forgetDestroyedNode);

    const QList<cwErrorModel*> children = node->childModels();
    for(cwErrorModel* child : children) {
        track(child);
    }
}

void cwErrorTreeModel::untrack(cwErrorModel* node)
{
    //Runs from childModelRemoved, which a dying cwErrorModel emits from its
    //destructor body, so node and its children are still whole here
    if(!m_tracked.contains(node)) {
        return;
    }

    const QList<cwErrorModel*> children = node->childModels();
    for(cwErrorModel* child : children) {
        untrack(child);
    }

    disconnect(node, nullptr, this, nullptr);
    removeSourceOf(node);
}

void cwErrorTreeModel::untrackAll()
{
    const QList<cwErrorModel*> nodes = m_tracked.keys();
    for(cwErrorModel* node : nodes) {
        disconnect(node, nullptr, this, nullptr);
        removeSourceOf(node);
    }
}

void cwErrorTreeModel::forgetDestroyedNode(QObject* node)
{
    //The cwErrorModel part is gone; the pointer is only a key now. Its list
    //model is a QObject child and outlives this signal.
    removeSourceOf(static_cast<cwErrorModel*>(node));
}

void cwErrorTreeModel::removeSourceOf(cwErrorModel* node)
{
    const QPointer<cwErrorListModel> list = m_tracked.take(node);
    if(list) {
        m_concat->removeSourceModel(list);
    }
}

void cwErrorTreeModel::refresh()
{
    const int newCount = rowCount();
    if(m_count != newCount) {
        m_count = newCount;
        emit countChanged();
    }

    std::array<bool, kKindOrder.size()> present = {};
    for(int row = 0; row < m_concat->rowCount(); ++row) {
        const QModelIndex index = m_concat->index(row, 0);
        if(passesSuppressedFilter(index)) {
            present.at(static_cast<size_t>(kindOf(index.data(kErrorTypeIdRole).toInt()))) = true;
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
