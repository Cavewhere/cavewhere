/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXEXPORTERCAVETASK_H
#define CWSURVEXEXPORTERCAVETASK_H

//Our includes
#include "cwCaveExporterTask.h"
#include "cwEquate.h"
#include "cwScopeLabels.h"
#include "cwSurvexExporterRegion.h"
#include "cwSurvexExporterUtils.h"
class cwSurvexExporterTripTask;
class cwStationHandle;

// Qt includes
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QTextStream>
#include <QUuid>

// Std includes
#include <optional>

class cwSurvexExporterCaveTask : public cwCaveExporterTask
{
    Q_OBJECT
public:
    explicit cwSurvexExporterCaveTask(QObject *parent = 0);

    //! One driver's snapshot, indexed once per export: the labels every block
    //! and operand share, which nodes emit a block, and the node or trip each
    //! container id names. writeNode and operand read the index, so neither
    //! walks the tree per node or per operand.
    class DriverTree
    {
    public:
        //! \a nodes are the snapshot's top-level nodes and \a labels the pool
        //! built from that same snapshot.
        DriverTree(const QList<cwCaveData>& nodes,
                   const cwScopeLabels& labels,
                   const QSet<QUuid>& excludedExternalOwners = {});
        Q_DISABLE_COPY_MOVE(DriverTree)

        const cwScopeLabels& labels() const { return m_labels; }
        const QSet<QUuid>& excludedExternalOwners() const { return m_excludedExternalOwners; }

        //! True when \a nodeId's subtree gives cavern something to read: a
        //! station, or an *include the driver does not exclude. A fix counts
        //! through its station, since a fix is valid only on one of the node's
        //! own stations. Cavern fatals with "No survey data" on a driver that
        //! declares only empty scopes, so a node without any is left out.
        bool emits(const QUuid& nodeId) const { return m_emittingNodeIds.contains(nodeId); }

        //! The station name cavern knows \a handle by from region scope: every
        //! label from the top-level node down to the handle's node, then a Trip
        //! handle's trip scope, then the tail. Inside a sourced subtree a
        //! derived trip's station is the root's label path, the trip's node's
        //! cavern path, then the tail, and a native trip's is the root's label
        //! path, the trip's label in the root's block, then the tail.
        //!
        //! Empty when the driver opens no scope for the handle: its container is
        //! in no node of the tree, sits inside an excluded owner or below a
        //! sourced root's own level, or is a node that emits nothing; and for a
        //! derived trip's tail outside its unit set. Cavern does not reject an
        //! unknown equate operand — it creates the station — so the caller
        //! drops the whole tie instead.
        QString operand(const cwStationHandle& handle) const;

        //! The native trips of the sourced subtree under \a rootId, pre-order,
        //! each with the node that holds it: what the root's block writes after
        //! its *include.
        struct SourcedTrip {
            const cwCaveData* node = nullptr;
            const cwTripData* trip = nullptr;
        };
        QList<SourcedTrip> sourcedNativeTrips(const QUuid& rootId) const
        {
            return m_sourcedNativeTrips.value(rootId);
        }

    private:
        struct Owner {
            const cwCaveData* node = nullptr;
            const cwTripData* trip = nullptr;
            bool hasScope = false;
            //! The source root whose subtree holds the owner, for a trip inside
            //! one.
            const cwCaveData* sourceRoot = nullptr;
        };

        //! Holds the buffer the Owner pointers point into.
        const QList<cwCaveData> m_nodes;
        const cwScopeLabels m_labels;
        const QSet<QUuid> m_excludedExternalOwners;
        QSet<QUuid> m_emittingNodeIds;
        QHash<QUuid, Owner> m_nodeOwners;
        QHash<QUuid, Owner> m_tripOwners;
        QHash<QUuid, QList<SourcedTrip>> m_sourcedNativeTrips;

        bool index(const cwCaveData& node, bool scopeClosedAbove);
        void indexSourcedSubtree(const cwCaveData& root, const cwCaveData& node,
                                 bool scopeClosed);
        QString sourcedTripOperand(const Owner& owner, const QString& tail) const;
    };

    //! The single-cave export: \a cave and its subtree, standing alone, labeled
    //! by cwScopeLabels::forNode, followed by the ties from setEquates() whose
    //! stations all sit in that subtree.
    bool writeCave(QTextStream& stream, const cwCaveData &cave, const QString& globalCS = QString());

    //! The region's equates, for writeCave. Ties that reach outside the
    //! exported cave are left out of its file.
    void setEquates(const QList<cwEquate>& equates);

    // Writes \a node as "*begin <label>", its fixes, its own trips, then each
    // child node the same way, then its "*end" — one block per native node, so
    // a station's cavern name is its node's label path. A sourced root writes
    // its *include, then every native trip of its subtree as a block of its
    // own; its derived nodes are never blocks. A node that emits nothing
    // (DriverTree::emits) writes nothing at all. \a node is one of \a tree's
    // top-level nodes.
    bool writeNode(QTextStream& stream,
                   const cwCaveData& node,
                   const DriverTree& tree,
                   const QString& globalCS = QString());

    // Per-call options forwarded from cwSurvexExporterRegion. The
    // attachment-dir maps drive *include emission for caves and trips
    // whose externalCenterline is set. Set once before the writeNode
    // loop; cleared by passing a default-constructed value.
    void setExportOptions(const cwSurvexExporterRegion::Options& options);

    // Emit one "*equate <a> <b> ..." line from pre-rendered operands. An empty
    // operand (an unresolvable handle) drops the whole tie; operands are
    // de-duplicated and the line is skipped unless at least two distinct
    // survive, since cavern rejects a self-equate. A line with an operand in
    // mixed case is read under *case preserve.
    static void writeEquateLine(QTextStream& stream, const QStringList& operands);

    // Emit one "*equate" line per structurally-valid tie in `equates`, each
    // handle rendered fully qualified by tree.operand(). Each line is handed to
    // writeEquateLine, so invalid ties and unrenderable handles drop uniformly.
    // Shared by the cave and region exporters.
    static void writeEquates(QTextStream& stream,
                             const QList<cwEquate>& equates,
                             const DriverTree& tree);

    //! The station the driver fixes at the origin for an attached file that
    //! fixes none of its own (\a fileFixedStations empty): the file's first
    //! harvested station, named as cavern resolves it inside the *include.
    //! Survex entry files only, whose default *case lower matches the
    //! harvest's lowercase names; Compass and Walls spell names the harvest
    //! cannot give back. Empty when no fallback applies, including before the
    //! harvest has named the file's stations.
    static QString originStation(const cwExternalCenterline& centerline,
                                 const QStringList& externalStations,
                                 const QStringList& fileFixedStations);

protected:
    QString writeStandaloneHeader(QTextStream& stream) override;

private:
    //! What a node's block hands down to the blocks nested in it.
    struct Inherited {
        //! Some enclosing block already fixes a station, so this one adds no
        //! fallback fix: a second anchor would pin a connected survey twice.
        bool anchored = false;
        //! An enclosing block already wrote `*declination auto`, which cavern
        //! scopes to that block and every block nested in it.
        bool autoDeclinationInScope = false;
        //! The nearest enclosing located block's grid convergence, for a node
        //! with no location of its own.
        double gridConvergence = 0.0;
        //! The nearest enclosing located block's location when that block
        //! wrote no `*declination auto` for its whole scope, so a nested block
        //! still has one to declare.
        std::optional<cwSurvexExporterUtils::DeclinationContext> undeclaredLocation;
    };

    cwSurvexExporterTripTask* TripExporter;
    cwSurvexExporterRegion::Options ExportOptions;
    QList<cwEquate> m_equates;

    bool writeNodeBlock(QTextStream& stream,
                        const cwCaveData& node,
                        const DriverTree& tree,
                        const QString& globalCS,
                        const Inherited& inherited,
                        const cwSurvexExporterUtils::CsScope& enclosingScope);

    //! True when the *include of \a ownerId's file meets a fix that names no
    //! system: the file fixes stations of its own (Options::externalFixedStations)
    //! and no system is chosen for them.
    bool readsInheritedSystem(const QUuid& ownerId, const cwExternalCenterline& centerline) const;

    //! True when \a node's block holds an *include that readsInheritedSystem:
    //! the node's own, an attached trip's, or one anywhere in its subtree. Such
    //! a block ends every input *cs it writes before that *include, so cavern
    //! refuses the file's bare fixes when nothing is chosen for them.
    bool scopeReachesInheritingInclude(const cwCaveData& node, const DriverTree& tree) const;

    struct WrittenFixes {
        //! The node's fixes that validation kept, in the order written.
        QList<cwFixStation> kept;
        //! A fix was written — one of the node's own, or the fallback on its
        //! first station when nothing above is anchored.
        bool anchored = false;
        //! The kept fixes on an attached Compass .mak's stations, left for
        //! writeExternalInclude to write inside the .mak's translation. Keyed
        //! by owner (the node or its trip), named within the owner's scope.
        QHash<QUuid, QList<cwFixStation>> makFixes;
    };

    WrittenFixes writeFixStations(QTextStream& stream, const cwCaveData& node,
                                  const DriverTree& tree,
                                  const QString& globalCS, bool anchoredAbove,
                                  cwSurvexExporterUtils::CsScope& scope);

    //! Puts an input system in scope for the *include of \a ownerId's file, so
    //! a *fix in it with no *cs of its own reads in the system chosen for the
    //! attachment (cwExternalCenterline::fixCoordinateSystem). When the project
    //! names no output system (\a globalCS empty), the file's own *cs out
    //! (Options::externalOutputSystems) is written ahead of the file so every
    //! one of its fixes sees it, and the chosen system, if any, follows it;
    //! with neither, nothing is written and cavern reads the fixes raw. Under
    //! the project's output system, with nothing chosen, the file's own *cs
    //! out is written as the input system alone, and with no *cs out of the
    //! file's own either, the output system translated so the file's first
    //! bare fix (Options::externalBareFixOrigins) lands on the origin.
    void writeIncludeSystem(QTextStream& stream, const QUuid& ownerId,
                            const cwExternalCenterline& centerline,
                            const QString& globalCS,
                            cwSurvexExporterUtils::CsScope& scope);

    // Emits *include "<abs>" for the cave/trip's externalCenterline by
    // joining the owner's attachment dir with the project-relative
    // entry file. A Compass .mak is written as its Survex translation
    // (cwCompassMakTranslator) instead, carrying \a makFixes, so its fixes
    // read through their datum and zone. Returns false (and appends an
    // error) when the owner's attachment dir is missing from ExportOptions
    // — that state means reconcile has not run yet, so writing a stale
    // *include would surface as a cavern parse failure rather than a clear
    // message.
    bool writeExternalInclude(QTextStream& stream,
                              const QUuid& ownerId,
                              const QHash<QUuid, QString>& attachmentDirs,
                              const QString& entryFile,
                              const QString& ownerLabel,
                              const QString& globalCS,
                              cwSurvexExporterUtils::CsScope& scope,
                              const QList<cwFixStation>& makFixes);
};

#endif // CWSURVEXEXPORTERCAVETASK_H
