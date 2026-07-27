#ifndef CWSIBLINGLABELCACHE_H
#define CWSIBLINGLABELCACHE_H

//Our includes
#include "cwCavernNaming.h"

//Qt includes
#include <QHash>
#include <QString>
#include <QUuid>

/**
 * A parent's memoized scope labels for one sibling set — its caves, or its trips.
 *
 * cwCavernNaming assigns a label across a whole sibling set at once, so the
 * object that owns the set — a region over its caves, a cave over its trips — is
 * the only one that can answer what any single child's label is. Answering walks
 * every sibling, and cwTrip::scopePrefix() is a Q_PROPERTY read, so the answer is
 * kept until the owner says the set moved.
 *
 * Recomputing eagerly on every change would be O(n^2) on load, where trips arrive
 * one undo command at a time; this rebuilds once, on the first read after an
 * invalidate().
 *
 * The live counterpart to cwScopeLabels, which answers the same question for a
 * whole region at once, from an ordered cwCavingRegionData snapshot, on a worker
 * thread. Both must reach the same assignment from the same order — that
 * equivalence is what lets the exporter and the decoder agree without carrying a
 * map between them, and test_cwScopeLabels.cpp pins it.
 */
class cwSiblingLabelCache
{
public:
    //! The label each of \a children takes, keyed by child id. \a children is
    //! any list of pointers answering id() and name(); it must be the owner's
    //! whole sibling set, since that is what makes a label unique.
    template <typename ChildList>
    const QHash<QUuid, QString>& labels(const ChildList& children) const
    {
        if(m_stale) {
            QList<cwCavernNaming::ScopeEntry> entries;
            entries.reserve(children.size());
            for(const auto* child : children) {
                entries.append({child->id(), child->name()});
            }

            m_labels = cwCavernNaming::scopeLabels(entries);
            m_stale = false;
        }
        return m_labels;
    }

    //! The sibling set moved — a child was added, removed, or renamed — so the
    //! next labels() rebuilds.
    void invalidate() { m_stale = true; }

private:
    mutable QHash<QUuid, QString> m_labels;
    mutable bool m_stale = true;
};

#endif // CWSIBLINGLABELCACHE_H
