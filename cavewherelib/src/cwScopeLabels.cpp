//Our includes
#include "cwScopeLabels.h"
#include "cwCavernNaming.h"
#include "cwCaveData.h"
#include "cwCavingRegionData.h"
#include "cwTripData.h"

namespace {

//! Answered for a node the pool never saw, so tripLabels() can hand back a
//! reference without the caller checking first.
const QHash<QUuid, QString> kNoTripLabels;

template <typename Data>
QList<cwCavernNaming::ScopeEntry> scopeEntries(const QList<Data>& siblings)
{
    QList<cwCavernNaming::ScopeEntry> entries;
    entries.reserve(siblings.size());
    for (const Data& sibling : siblings) {
        entries.append({sibling.id, sibling.name});
    }
    return entries;
}

//! The labels every native trip of \a root's subtree takes, one pool assigned
//! pre-order (a node's trips, then each child's subtree), apart from the scopes
//! the root's included file opens: its derived nodes' and derived trips'
//! cavernLabels among them.
QHash<QUuid, QString> sourcedTripLabels(const cwCaveData& root)
{
    QList<cwCavernNaming::ScopeEntry> entries;
    QStringList cavernLabels;
    const auto collect = [&](const auto& self, const cwCaveData& node) -> void {
        cavernLabels.append(node.cavernLabel);
        for (const cwTripData& trip : node.trips) {
            if (isNativeTrip(trip)) {
                entries.append({trip.id, trip.name});
            } else {
                cavernLabels.append(trip.cavernLabel);
            }
        }
        for (const cwCaveData& child : node.nodes) {
            self(self, child);
        }
    };
    collect(collect, root);

    return cwCavernNaming::scopeLabels(
        entries, cwScopeLabels::includedScopeHeads(root.externalStations, cavernLabels));
}

}

cwScopeLabels::cwScopeLabels(const cwCavingRegionData& region)
{
    addSiblings(QUuid(), QString(), region.caves, {}, QUuid());
}

cwScopeLabels cwScopeLabels::forNode(const cwCaveData& node)
{
    cwScopeLabels labels;
    labels.addSiblings(QUuid(), QString(), {node}, {}, QUuid());
    return labels;
}

QSet<QString> cwScopeLabels::includedScopeHeads(const QStringList& rootStations,
                                                const QStringList& cavernLabels)
{
    QSet<QString> heads;
    for (const QString& station : rootStations) {
        const QString head = cwCavernNaming::scopeHeadOf(station);
        if (!head.isEmpty()) {
            heads.insert(head.toLower());
        }
    }
    for (const QString& label : cavernLabels) {
        if (!label.isEmpty()) {
            heads.insert(label.toLower());
        }
    }
    return heads;
}

void cwScopeLabels::addSiblings(const QUuid& parentId, const QString& parentPrefix,
                                const QList<cwCaveData>& siblings,
                                const QList<cwTripData>& siblingTrips,
                                const QUuid& sourceRootId)
{
    //A node's trips and child nodes open their blocks in one scope, so they draw
    //from one pool. Trips claim first, which keeps a trip's label a function of
    //its sibling trips alone.
    const QHash<QUuid, QString> labels =
        cwCavernNaming::scopeLabels(scopeEntries(siblingTrips) + scopeEntries(siblings));
    for (const cwCaveData& sibling : siblings) {
        addNode(parentId, parentPrefix, sibling, labels.value(sibling.id), sourceRootId);
    }
}

void cwScopeLabels::addNode(const QUuid& parentId, const QString& parentPrefix,
                            const cwCaveData& node, const QString& label,
                            const QUuid& sourceRootId)
{
    const QString prefix = parentPrefix + cwCavernNaming::scopePrefix(label);
    m_nodeLabels.insert(node.id, label);
    m_prefixes.insert(node.id, prefix);
    m_childIdsByLabel[parentId].insert(label.toLower(), node.id);

    QUuid rootId = sourceRootId;
    if (rootId.isNull() && isSourceRoot(node)) {
        rootId = node.id;
        m_tripLabelsByNode.insert(node.id, sourcedTripLabels(node));
        m_cavernPaths.insert(node.id, QString());
    } else if (!rootId.isNull()) {
        m_tripLabelsByNode.insert(node.id, m_tripLabelsByNode.value(rootId));
        m_cavernPaths.insert(node.id, m_cavernPaths.value(parentId)
                                          + cwCavernNaming::scopePrefix(node.cavernLabel));
    } else {
        m_tripLabelsByNode.insert(node.id, cwCavernNaming::scopeLabels(scopeEntries(node.trips)));
    }
    if (!rootId.isNull()) {
        m_sourceRoots.insert(node.id, rootId);
    }

    addSiblings(node.id, prefix, node.nodes, node.trips, rootId);
}

QString cwScopeLabels::label(const QUuid& nodeId) const
{
    return m_nodeLabels.value(nodeId);
}

QString cwScopeLabels::prefix(const QUuid& nodeId) const
{
    return m_prefixes.value(nodeId);
}

QUuid cwScopeLabels::childId(const QUuid& parentId, const QString& label) const
{
    return m_childIdsByLabel.value(parentId).value(label.toLower());
}

QUuid cwScopeLabels::nodeId(const QStringList& labelPath) const
{
    QUuid current;
    for (const QString& segment : labelPath) {
        current = childId(current, segment);
        if (current.isNull()) {
            return QUuid();
        }
    }
    return current;
}

cwScopeLabels::Resolution cwScopeLabels::resolve(const QString& scopedName) const
{
    Resolution resolution{QUuid(), scopedName};

    for (;;) {
        const QString head = cwCavernNaming::scopeHeadOf(resolution.remainder);
        if (head.isEmpty()) {
            return resolution;
        }

        const QUuid child = childId(resolution.nodeId, head);
        if (child.isNull()) {
            return resolution;
        }

        resolution.nodeId = child;
        resolution.remainder = cwCavernNaming::removeScopeHead(resolution.remainder);
    }
}

const QHash<QUuid, QString>& cwScopeLabels::tripLabels(const QUuid& nodeId) const
{
    const auto iter = m_tripLabelsByNode.constFind(nodeId);
    if (iter == m_tripLabelsByNode.constEnd()) {
        return kNoTripLabels;
    }
    return iter.value();
}

QUuid cwScopeLabels::sourceRoot(const QUuid& nodeId) const
{
    return m_sourceRoots.value(nodeId);
}

QString cwScopeLabels::cavernPath(const QUuid& nodeId) const
{
    return m_cavernPaths.value(nodeId);
}
