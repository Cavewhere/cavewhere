#ifndef CWSCOPELABELS_H
#define CWSCOPELABELS_H

//Our includes
#include "cwGlobals.h"
struct cwCaveData;
struct cwCavingRegionData;
struct cwTripData;

//Qt includes
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUuid>

/**
 * The survey labels one region snapshot's nodes and trips take, assigned once.
 *
 * Every native node is its own "*begin <label>" block, nested the way the tree
 * nests, so a station's cavern name is its node's label path, then the trip
 * scope, then the tail: <cave>.<section>.<tripLabel>.<tail>. cwCavernNaming
 * assigns a label per sibling set — a trip among its node's trips, a node among
 * its parent's trips and child nodes — which makes every label a pure function
 * of an ordered snapshot. That is what lets the exporter, the line-plot worker and the
 * geometry pass agree on a name without carrying a map across the thread
 * boundary between them; this is that derivation done once and passed along.
 *
 * The top-level nodes (the region's caves) are children of the root, which
 * carries no label and answers to the null QUuid.
 *
 * A sourced subtree is one "*include" at its source root, so its derived nodes
 * open no block. The native trips anywhere inside it are written in the root's
 * block, each as "*begin <tripLabel>", so they share one label set across the
 * whole subtree (§13 Q7), kept apart from the scopes the included file opens
 * itself. tripLabels() answers that one set for every node of the subtree.
 *
 * Holds no live model object and copies cheaply (implicitly-shared hashes), so
 * it travels with the cwCavingRegionData it was built from.
 */
class CAVEWHERE_LIB_EXPORT cwScopeLabels
{
public:
    //! Where a cavern name lands: the deepest node whose label path the name
    //! starts with, and what is left of the name below that node.
    struct Resolution {
        QUuid nodeId;
        QString remainder;
    };

    cwScopeLabels() = default;
    explicit cwScopeLabels(const cwCavingRegionData& region);

    //! Labels for one node standing alone, as the single-cave exporter sees it:
    //! the node takes its own sanitized name, since it has no siblings to
    //! collide with, and its subtree is labeled the way a region would label it.
    static cwScopeLabels forNode(const cwCaveData& node);

    //! The label \a nodeId's "*begin" block carries, unique among its sibling
    //! nodes and its parent's trips, or empty when this pool assigned none.
    QString label(const QUuid& nodeId) const;

    //! Every label from the top-level node down to \a nodeId, each followed by
    //! "." — the prefix every station inside that node carries. Empty when this
    //! pool assigned the node no label.
    QString prefix(const QUuid& nodeId) const;

    //! The node whose label path is \a labelPath, top-level label first, or a
    //! null QUuid when no node wears it. Matches case-insensitively, as cavern
    //! may echo a nested scope in the case its source file used.
    QUuid nodeId(const QStringList& labelPath) const;

    //! Consumes \a scopedName's leading scopes one segment at a time for as long
    //! as a child of the current node wears the segment. The inverse of
    //! prefix(): resolve(prefix(id) + tail) is {id, tail}. A name whose first
    //! segment names no top-level node resolves to a null nodeId with the whole
    //! name as the remainder.
    Resolution resolve(const QString& scopedName) const;

    //! The labels \a nodeId's own trips take, keyed by trip id. Empty for a node
    //! this pool never saw. For a node of a sourced subtree, the labels of
    //! every native trip in the subtree.
    const QHash<QUuid, QString>& tripLabels(const QUuid& nodeId) const;

    //! The source root whose subtree holds \a nodeId (the node itself for a
    //! root), or a null QUuid for a native node.
    QUuid sourceRoot(const QUuid& nodeId) const;

    //! The cavernLabels from \a nodeId's source root down to it, each followed
    //! by "." — the scope a derived node's stations carry below the root's
    //! label path ("alpha." for a Compass file). Empty for a root, a Walls
    //! book, and a native node.
    QString cavernPath(const QUuid& nodeId) const;

    //! The scope heads an included file opens itself, lowercased: the first
    //! segment of each dotted station it holds and each derived node's
    //! cavernLabel. A native trip written beside the *include never takes one.
    static QSet<QString> includedScopeHeads(const QStringList& rootStations,
                                            const QStringList& cavernLabels);

private:
    QHash<QUuid, QString> m_nodeLabels;
    QHash<QUuid, QString> m_prefixes;
    //! Keyed by parent id (null for the root), then by lowercased label.
    QHash<QUuid, QHash<QString, QUuid>> m_childIdsByLabel;
    QHash<QUuid, QHash<QUuid, QString>> m_tripLabelsByNode;
    QHash<QUuid, QUuid> m_sourceRoots;
    QHash<QUuid, QString> m_cavernPaths;

    void addSiblings(const QUuid& parentId, const QString& parentPrefix,
                     const QList<cwCaveData>& siblings,
                     const QList<cwTripData>& siblingTrips,
                     const QUuid& sourceRootId);
    void addNode(const QUuid& parentId, const QString& parentPrefix,
                 const cwCaveData& node, const QString& label,
                 const QUuid& sourceRootId);
    QUuid childId(const QUuid& parentId, const QString& label) const;
};

#endif // CWSCOPELABELS_H
