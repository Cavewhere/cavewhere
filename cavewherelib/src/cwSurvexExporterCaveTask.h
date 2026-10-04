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
        //! handle's trip scope, then the tail.
        //!
        //! Empty when the driver opens no scope for the handle: its container is
        //! in no node of the tree, sits inside an excluded owner or below a
        //! sourced root's own level, or is a node that emits nothing. Cavern
        //! does not reject an unknown equate operand — it creates the station —
        //! so the caller drops the whole tie instead.
        QString operand(const cwStationHandle& handle) const;

    private:
        struct Owner {
            const cwCaveData* node = nullptr;
            const cwTripData* trip = nullptr;
            bool hasScope = false;
        };

        //! Holds the buffer the Owner pointers point into.
        const QList<cwCaveData> m_nodes;
        const cwScopeLabels m_labels;
        const QSet<QUuid> m_excludedExternalOwners;
        QSet<QUuid> m_emittingNodeIds;
        QHash<QUuid, Owner> m_nodeOwners;
        QHash<QUuid, Owner> m_tripOwners;

        bool index(const cwCaveData& node, bool scopeClosedAbove);
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
    // its *include and nothing below it. A node that emits nothing
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
    // survive, since cavern rejects a self-equate.
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
