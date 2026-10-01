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

QList<cwCavernNaming::ScopeEntry> tripEntries(const QList<cwTripData>& trips)
{
    QList<cwCavernNaming::ScopeEntry> entries;
    entries.reserve(trips.size());
    for (const cwTripData& trip : trips) {
        entries.append({trip.id, trip.name});
    }
    return entries;
}

}

cwScopeLabels::cwScopeLabels(const cwCavingRegionData& region)
{
    addSiblings(QUuid(), QString(), region.caves);
}

cwScopeLabels cwScopeLabels::forNode(const cwCaveData& node)
{
    cwScopeLabels labels;
    labels.addSiblings(QUuid(), QString(), {node});
    return labels;
}

void cwScopeLabels::addSiblings(const QUuid& parentId, const QString& parentPrefix,
                                const QList<cwCaveData>& siblings)
{
    QList<cwCavernNaming::ScopeEntry> entries;
    entries.reserve(siblings.size());
    for (const cwCaveData& sibling : siblings) {
        entries.append({sibling.id, sibling.name});
    }

    const QHash<QUuid, QString> labels = cwCavernNaming::scopeLabels(entries);
    for (const cwCaveData& sibling : siblings) {
        addNode(parentId, parentPrefix, sibling, labels.value(sibling.id));
    }
}

void cwScopeLabels::addNode(const QUuid& parentId, const QString& parentPrefix,
                            const cwCaveData& node, const QString& label)
{
    const QString prefix = parentPrefix + cwCavernNaming::scopePrefix(label);
    m_nodeLabels.insert(node.id, label);
    m_prefixes.insert(node.id, prefix);
    m_childIdsByLabel[parentId].insert(label.toLower(), node.id);
    m_tripLabelsByNode.insert(node.id, cwCavernNaming::scopeLabels(tripEntries(node.trips)));
    addSiblings(node.id, prefix, node.nodes);
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
