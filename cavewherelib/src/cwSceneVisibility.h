/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCENEVISIBILITY_H
#define CWSCENEVISIBILITY_H

//Qt includes
#include <QObject>
#include <QHash>
#include <QSet>
#include <QVector>

//Std includes
#include <memory>
#include <utility>

//Our includes
#include "cwRenderObjectId.h"
#include "CaveWhereLibExport.h"

// The scene's single published visibility truth (issue #579, see
// plans/SCENE_VISIBILITY_STORE_PLAN.html). Render objects keep their authoring
// copies and *publish* here through facades; consumers read versioned immutable
// snapshots instead of keeping hand-synced twins.
//
// Addressing: (cwRenderObjectId, subId) covers all three granularities — the
// whole-object flag is keyed by id alone, a per-sub-item flag by (id, subId),
// and a per-vertex mask rides the (id, subId) entry. Effective visibility is
// the AND across levels: objectVisible && subVisible && maskByte. The store is
// sparse — absent means visible — so it only holds what is hidden or masked.

// Immutable view of the store, cheap to copy (a shared_ptr), safe to hand to
// any thread: keys are stable ids, never pointers. mask() returns a pointer
// into this snapshot's data — valid for the snapshot's lifetime.
class CAVEWHERE_LIB_EXPORT cwVisibilitySnapshot
{
public:
    cwVisibilitySnapshot() = default;

    quint64 version() const;
    bool objectVisible(cwRenderObjectId id) const;                          // absent = true
    bool subVisible(cwRenderObjectId id, uint64_t subId) const;             // ANDs the object level
    const QVector<quint8>* mask(cwRenderObjectId id, uint64_t subId) const; // null = all-visible
    quint64 entryVersion(cwRenderObjectId id, uint64_t subId) const;        // 0 = no entry

private:
    friend class cwSceneVisibility;

    using SubKey = std::pair<cwRenderObjectId, uint64_t>;

    struct Entry {
        bool visible = true;
        QVector<quint8> mask; // empty = all-visible; shares the owner's buffer
        // Store-wide version at this entry's last mutation, so a consumer
        // (cwRHILinePlot's mask upload) can skip work when only unrelated
        // entries changed. Values come from the monotonic store version and
        // are never reused, so "entry removed" (reads as 0) can't alias a
        // real version.
        quint64 entryVersion = 0;
    };

    struct Data {
        quint64 version = 0;
        QSet<cwRenderObjectId> hiddenObjects;
        QHash<SubKey, Entry> subEntries;
    };

    explicit cwVisibilitySnapshot(std::shared_ptr<const Data> data);

    std::shared_ptr<const Data> m_data;
};

// Owned by cwScene; all mutation is scene-thread-only, and snapshot()/
// version() count as mutation for that rule (snapshot() writes the m_cached
// cache) — call them on the scene thread, or from the render thread only
// while the scene thread is blocked at the sync barrier. Off-thread
// consumers receive a cwVisibilitySnapshot by value instead of the store
// pointer. Snapshots materialize lazily on read (copy-on-read): mutations
// write the live state and drop the cached snapshot, so a keyword burst
// flipping N items costs one materialization per consumer read, not one per
// toggle. The copy itself is cheap — the containers are implicitly shared,
// and the first mutation after a materialization pays the detach.
class CAVEWHERE_LIB_EXPORT cwSceneVisibility : public QObject
{
    Q_OBJECT

public:
    explicit cwSceneVisibility(QObject* parent = nullptr);

    // Owner-facing publish API (called by the render-object facades).
    // Writing the default state (visible, empty mask) removes the entry, so
    // the store stays sparse; redundant writes are no-ops — no version bump,
    // no signal.
    void setObjectVisible(cwRenderObjectId id, bool visible);
    void setSubVisible(cwRenderObjectId id, uint64_t subId, bool visible);
    void setMask(cwRenderObjectId id, uint64_t subId, QVector<quint8> mask);
    void removeObject(cwRenderObjectId id); // scrubs the object entry and all its subs
    void removeSub(cwRenderObjectId id, uint64_t subId);

    // Consumer-facing read API
    cwVisibilitySnapshot snapshot() const;
    quint64 version() const;

signals:
    // One emission per effective mutation, so bursts fire it in a burst too;
    // consumers coalesce on their side (re-read at frame cadence) rather than
    // re-materializing a snapshot per toggle.
    void changed();

private:
    using SubKey = cwVisibilitySnapshot::SubKey;
    using Entry = cwVisibilitySnapshot::Entry;

    void touch();

    cwVisibilitySnapshot::Data m_live;
    mutable std::shared_ptr<const cwVisibilitySnapshot::Data> m_cached; // null = dirty
};

#endif // CWSCENEVISIBILITY_H
