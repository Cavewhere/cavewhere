/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYNODE_H
#define CWSURVEYNODE_H

//Our include
class cwTrip;
class cwCavingRegion;
class cwKeywordModel;
#include "cwGridConvergence.h"
#include "cwErrorModel.h"
#include "cwEquate.h"
#include "cwExternalCenterline.h"
#include "cwLength.h"
#include "cwStation.h"
#include "cwUndoer.h"
#include "cwStationPositionLookup.h"
#include "cwGlobals.h"
#include "cwSanitizedNameSet.h"
#include "cwSurveyNetwork.h"
#include "cwCaveData.h"
#include "cwSurveyNodeKind.h"
#include "cwFixStationModel.h"
#include "cwFixStationDiagnosticsModel.h"
#include "cwAttachedFixModel.h"
#include "cwSiblingLabelCache.h"

//Qt includes
#include <QObject>
#include <QList>
#include <QPointer>
#include <QSharedPointer>
#include <QUndoCommand>
#include <QDebug>
#include <QWeakPointer>
#include <QVariant>
#include <QAbstractListModel>
#include <QConcatenateTablesProxyModel>
#include <QHash>
#include <QQmlEngine>
#include <QUuid>
#include <QSet>

//Std includes
#include <memory>

/**
 * One node of the survey tree: the region's root, a cave, a folder, or a node
 * mirroring one level of an attached survey file. A node holds trips
 * (cwTrip, the leaves) and child nodes, so the tree is Region → node … → trip.
 *
 * The list model rows are this node's DIRECT trips only, as cwCave's rows have
 * always been; child-node rows live in cwRegionTreeModel.
 */
class CAVEWHERE_LIB_EXPORT cwSurveyNode : public QAbstractListModel, public cwUndoer
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyNode)

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QUuid id READ id NOTIFY idChanged FINAL)
    Q_PROPERTY(cwLength* length READ length CONSTANT)
    Q_PROPERTY(cwLength* depth READ depth CONSTANT)
    Q_PROPERTY(cwErrorModel* errorModel READ errorModel CONSTANT)
    Q_PROPERTY(cwFixStationModel* fixStations READ fixStations CONSTANT)
    Q_PROPERTY(cwFixStationDiagnosticsModel* fixStationDiagnostics READ fixStationDiagnostics CONSTANT)
    Q_PROPERTY(cwGridConvergence* gridConvergence READ gridConvergence CONSTANT)
    Q_PROPERTY(cwExternalCenterline externalCenterline READ externalCenterline WRITE setExternalCenterline NOTIFY externalCenterlineChanged)
    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)
    Q_PROPERTY(cwSurveyNodeKind::Kind kind READ kind WRITE setKind NOTIFY kindChanged)
    Q_PROPERTY(bool isReadOnly READ isReadOnly NOTIFY sourceChanged)
    Q_PROPERTY(bool isSourced READ isSourced NOTIFY sourceChanged)
    Q_PROPERTY(bool isSourceRoot READ isSourceRoot NOTIFY sourceChanged)
    Q_PROPERTY(QUuid sourceId READ sourceId NOTIFY sourceChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY sourceChanged)
    Q_PROPERTY(cwSurveyNode* parentNode READ parentNode NOTIFY parentNodeChanged)
    Q_PROPERTY(int childNodeCount READ childNodeCount NOTIFY childNodeCountChanged)
    Q_PROPERTY(int tripCount READ tripCount NOTIFY tripCountChanged)
    Q_PROPERTY(bool takesCaves READ takesCaves NOTIFY takesCavesChanged)
    Q_PROPERTY(bool externallyBacked READ externallyBacked NOTIFY externallyBackedChanged)
    Q_PROPERTY(cwAttachedFixModel* attachedFixes READ attachedFixes CONSTANT)
    Q_PROPERTY(QAbstractItemModel* fixStationTable READ fixStationTable CONSTANT)
    Q_PROPERTY(int fixStationCount READ fixStationCount NOTIFY fixStationCountChanged)

public:
    enum Roles {
        TripObjectRole
    };
    Q_ENUM(Roles)

    //! What a node is called and drawn as, defined in cwSurveyNodeKind.h so
    //! cwCaveData can carry it too.
    using Kind = cwSurveyNodeKind::Kind;

    //! Constructor tag for the region's root node. Root-ness is fixed at birth,
    //! so no setter can turn an ordinary node into a second root.
    struct RootNodeTag {};

    explicit cwSurveyNode(QObject* parent = nullptr);
    explicit cwSurveyNode(RootNodeTag, QObject* parent = nullptr);
    virtual ~cwSurveyNode();

    QString name() const;
    void setName(QString name);
    Q_INVOKABLE QString validateName(const QString& proposedName) const;
    QUuid id() const;
    void setId(const QUuid& id);

    Kind kind() const { return m_kind; }
    void setKind(Kind kind);

    //! True when this node mirrors a file CaveWhere copied in and the user may
    //! not edit its survey data.
    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);

    //! The external source this node came from, null for a native node.
    QUuid sourceId() const { return m_sourceId; }
    void setSourceId(const QUuid& sourceId);

    //! Where this node sits inside its source's scan, empty for the source's
    //! own root node.
    QString sourcePath() const { return m_sourcePath; }
    void setSourcePath(const QString& sourcePath);

    //! True when this node came from an external source.
    bool isSourced() const { return !m_sourceId.isNull(); }

    //! True when this node is the top of a sourced subtree — the row that owns
    //! the copied file and carries the Reload and Replace verbs.
    bool isSourceRoot() const { return isSourced() && m_sourcePath.isEmpty(); }

    //! True where caves go: the root, or a Folder with no Cave and no sourced
    //! node at or above it. Everywhere else takes trips and sections, so a Cave
    //! is never offered inside a Cave. The Add verbs read this; it is the one
    //! place the kind of a node decides what may be added under it.
    bool takesCaves() const { return m_takesCaves; }

    cwExternalCenterline externalCenterline() const { return m_externalCenterline; }
    void setExternalCenterline(const cwExternalCenterline& value);

    //! The station names this node's own attached file declares, in the file's
    //! namespace ("doghill.d1"), canonical and sorted. Learned at scan time by
    //! cwExternalStationHarvest, so a fix on this node can name a station of a
    //! file the region solve has not placed yet. Empty for a node with no
    //! attachment of its own, and for now for any node below the top level,
    //! whose attachment the scan does not reach yet (survey tree W5). Derived
    //! state: rebuilt by every external-centerline scan, never persisted, and
    //! never a save trigger.
    QStringList externalStations() const { return m_externalStations; }
    void setExternalStations(const QStringList& stations) { m_externalStations = stations; }

    //! The fixes the files attached to this node and to its trips carry
    //! themselves, as the attach scan read them, for the Fix Stations page to
    //! list read-only. Derived like externalStations().
    cwAttachedFixModel* attachedFixes() const { return m_attachedFixes; }
    //! Feeds attachedFixes(), emitting attachedFixesChanged() when the list differs.
    void setAttachedFixes(const QList<cwAttachedFix>& fixes);

    //! Per owner (this node or one of its trips), the stations its attached
    //! file fixes itself, named as cavern names them in that file
    //! ("doghill.d1"). Derived like externalStations().
    void setFileFixedStations(const QHash<QUuid, QStringList>& stationsByOwner);

    //! The stations a node fix may not name because an attached file fixes
    //! them itself, keyed by cwStation::canonicalKey() of the name a fix on
    //! this node uses (a trip's file stations under its scopePrefix()), each
    //! with the attached file's name.
    QHash<QString, QString> fileFixedStations() const;

    //! True when an external survey file — this node's own attachment, or one an
    //! ancestor attached — is what places this node's stations.
    bool externallyBacked() const;

    //! True when \a equate is structurally valid (cwEquate::isValid) and every
    //! handle resolves into this node: a NativeCave handle's containerId equals
    //! this node's id, and a Trip handle's containerId is one of this node's
    //! trips. The tie itself lives in the region's list (cwCavingRegion::equates).
    bool validate(const cwEquate& equate) const;

    cwKeywordModel* keywordModel() const { return m_keywordModel; }

    cwLength* length() const;
    cwLength* depth() const;

    cwErrorModel* errorModel() const;
    cwFixStationModel* fixStations() const { return m_fixStations; }

    /// The fix stations plus their read-only, computed warnings (coordinate
    /// domain, station reference). A proxy over fixStations() that the
    /// FixStationPage delegates bind to; the warnings are derived from the solve
    /// and each row's own inputCS(), so they deliberately do not reach
    /// fixStations()' dataChanged, which means "persisted data changed" and
    /// nothing else.
    cwFixStationDiagnosticsModel* fixStationDiagnostics() const { return m_fixStationDiagnostics; }

    /// The Fix Stations page's table: fixStationDiagnostics()' rows, then
    /// attachedFixes()' read-only rows. The node's own rows keep their indices
    /// 0..n-1, and setData()/flags() reach the model that owns each row, so an
    /// attached row refuses edits. Map an attached row with mapFromSource().
    QConcatenateTablesProxyModel* fixStationTable() const { return m_fixStationTable; }

    /// The number of rows fixStationTable() lists: the node's own fixes plus
    /// the ones its attached files carry.
    int fixStationCount() const { return m_fixStationTable->rowCount(); }

    /// Per-node grid-convergence readout (angle + state + display text).
    /// Recomputed via recomputeGridConvergence() in the region's local
    /// projection — the grid cavern plots the stations in — at the location the
    /// node's first usable fix station gives. cwScrap reads
    /// gridConvergence()->angle() to remove that grid's rotation from the note
    /// transform, so it has to be the same grid the stations came back in.
    cwGridConvergence* gridConvergence() const { return m_gridConvergence; }

    int tripCount() const;
    Q_INVOKABLE cwTrip* trip(int index) const;
    bool hasTrips() const { return tripCount() > 0; }
    QList<cwTrip*> trips() const;

    void insertTrip(int i, cwTrip* trip);
    Q_INVOKABLE void removeTrip(int i);
    Q_INVOKABLE void clearTrips();
    Q_INVOKABLE void addTrip(cwTrip* trip = nullptr);
    Q_INVOKABLE int indexOf(cwTrip* trip) const;
    Q_INVOKABLE QString uniqueTripName(const QString& proposedName) const;

    cwSanitizedNameSet& tripNameSet() { return m_tripNames; }
    const cwSanitizedNameSet& tripNameSet() const { return m_tripNames; }

    //! The cavern survey label each of this node's trips takes, keyed by trip
    //! id. cwCavernNaming assigns them across the whole sibling set, so adding,
    //! removing, or renaming any one trip can move another trip's collision
    //! suffix — this node is the only object that can see that happen, and
    //! tripScopeLabelsChanged() is how it says so. Cached, so cwTrip::scopePrefix()
    //! costs a lookup rather than a walk over every sibling.
    const QHash<QUuid, QString>& tripScopeLabels() const;

    //! The same, for this node's child nodes, which also take labels apart from
    //! every trip label here, since both open blocks in this node's scope.
    const QHash<QUuid, QString>& childScopeLabels() const;

    //! The node holding this one, nullptr for the region's root node and for a
    //! node nothing has inserted yet.
    cwSurveyNode* parentNode() const { return m_parentNode; }

    //! True for the region's root node: unnamed, unsaved, and excluded from
    //! path().
    bool isRoot() const { return m_isRoot; }

    QList<cwSurveyNode*> childNodes() const;
    Q_INVOKABLE cwSurveyNode* childNode(int index) const;
    int childNodeCount() const;
    Q_INVOKABLE int indexOfNode(cwSurveyNode* node) const;

    //! True while a parent holds this node among its children. A removed node
    //! keeps its parent pointer so undo can put it back, so this, and not
    //! parentNode(), is what says the node currently hangs in the tree.
    bool isListedByParent() const;

    Q_INVOKABLE void addNode(cwSurveyNode* node);
    void addNodes(const QList<cwSurveyNode*>& nodes);

    //! Places \a node at \a row among this node's children.
    //!
    //! A node no parent lists is inserted. A node another parent already lists
    //! MOVES here as one undo step (MoveNodeCommand): undo puts it back under
    //! its old parent, at its old row and under its old name, and the node and
    //! its whole subtree keep their QObject identities throughout. \a row is
    //! the destination index once the node is off its old place, so moving
    //! within one parent counts rows as if the node were already gone; it is
    //! clamped to the sibling list rather than refused.
    void insertNode(int row, cwSurveyNode* node);
    Q_INVOKABLE void removeNode(int row);
    Q_INVOKABLE void clearNodes();

    Q_INVOKABLE QString uniqueChildName(const QString& proposedName) const;

    //! True when \a subject, a cwSurveyNode or a cwTrip, may be moved at all:
    //! a native node other than the root, or a native trip of a native node.
    //! Survey data an attached file places — a sourced node, a node or trip
    //! with an attachment of its own or under one, a Scope trip — stays where
    //! its file puts it.
    static bool isMovable(const QObject* subject);

    //! Why \a subject, a cwSurveyNode or a cwTrip, cannot move here, empty
    //! when it can. A node takes the subjects the Add verbs would offer at its
    //! position: a trip lands on a native node where caves do not go, a Cave
    //! only where caves go, and a node never inside itself.
    Q_INVOKABLE QString moveRefusal(QObject* subject) const;

    //! The sentence a move of \a subject here owes the user before it runs:
    //! the station names it ties back to the node it leaves, the names it
    //! joins here, and the fixes that travel with it. Empty when the move is
    //! the move and nothing more.
    Q_INVOKABLE QString moveConsequences(QObject* subject) const;

    //! Moves \a subject here as one undo step: the move itself, a tie for
    //! every station name a trip leaves behind on the far side of a scope
    //! boundary, the equates that named a leaving station re-keyed to where it
    //! now lives, and the fixes of leaving stations carried to this node.
    //! Returns false and changes nothing when moveRefusal() names a reason.
    Q_INVOKABLE bool moveHere(QObject* subject);

    cwSanitizedNameSet& childNameSet() { return m_childNames; }
    const cwSanitizedNameSet& childNameSet() const { return m_childNames; }

    //! Every trip at or below this node, in the tree's document order: each
    //! child node's trips in row order, then this node's own. The order
    //! cwRegionTreeModel lists rows in, so an aggregator may swap one for the
    //! other.
    QList<cwTrip*> allTrips() const;

    //! Every node below this one, pre-order, this node excluded.
    QList<cwSurveyNode*> allNodes() const;

    //! Calls \a f(const cwSurveyNode*) on this node, then on each descendant,
    //! pre-order.
    template <class F>
    void walk(F&& f) const
    {
        f(this);
        for(const cwSurveyNode* child : m_childNodes) {
            child->walk(f);
        }
    }

    //! The names from the root down to this node, the root's own excluded — the
    //! key this node is persisted under.
    QStringList path() const;

    //! The same walk, by id.
    QList<QUuid> pathIds() const;

    //! The nearest ancestor-or-self that is a source root, nullptr for a fully
    //! native node. Keyed off the source fields, never off kind().
    cwSurveyNode* sourceRoot() const;

    //! The deepest node that holds both this node and \a other, nullptr when
    //! they belong to different trees. A node is its own ancestor, so
    //! lowestCommonAncestor(this) is this.
    cwSurveyNode* lowestCommonAncestor(const cwSurveyNode* other) const;

    cwCavingRegion* parentRegion() const;

    //! The unit system in effect for this node: its region's, or Metric when the
    //! node has no region yet. The single conduit for the project default that
    //! new trips/sketches/scraps seed from (mirrors ProjectUnits.qml in QML).
    cwUnits::UnitSystem unitSystem() const;

    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const;

    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;

    cwStationPositionLookup stationPositionLookup() const;
    void setStationPositionLookup(const cwStationPositionLookup& model);

    cwSurveyNetwork network() const;
    void setSurveyNetwork(const cwSurveyNetwork& network);

    void setStationPositionLookupStale(bool isStale);
    bool isStationPositionLookupStale() const;

    QList< cwStation > stations() const;

    cwCaveData data() const;
    void setData(const cwCaveData& data);

signals:
    void beginInsertTrips(int begin, int end);
    void insertedTrips(int begin, int end);

    void beginRemoveTrips(int begin, int end);
    void removedTrips(int begin, int end);

    void beginInsertNodes(int begin, int end);
    void insertedNodes(int begin, int end);

    void beginRemoveNodes(int begin, int end);
    void removedNodes(int begin, int end);

    void nameChanged();

    void idChanged();

    void kindChanged();

    //! takesCaves() moved: this node's kind or source changed, or one of its
    //! ancestors' did, or the node moved.
    void takesCavesChanged();

    //! One of readOnly, sourceId, or sourcePath moved, so isSourced() and
    //! isSourceRoot() may have moved with them.
    void sourceChanged();

    void parentNodeChanged();

    //! This node is about to leave one parent for another as a single move.
    //! The beginRemoveNodes/removedNodes and beginInsertNodes/insertedNodes
    //! pairs still carry it out, so a view needs nothing new — this pair is for
    //! a consumer that would otherwise read the removal as a deletion, such as
    //! cwSaveLoad, which deletes a removed node's directory.
    void beginMoveNode();

    //! The move is done: the node hangs under its new parent.
    void nodeMoved();

    void childNodeCountChanged();

    void tripCountChanged();

    //! Some trip label in this node may have moved: a trip was added, removed,
    //! or renamed. Chained to each held trip's cwTrip::scopeChanged, so a trip
    //! learns when a *sibling* moved its label — which the trip cannot see for
    //! itself.
    void tripScopeLabelsChanged();

    //! The same, for this node's child nodes: one was added, removed, or
    //! renamed, or a trip here took a label a child held, so a child's
    //! collision suffix may have moved.
    void childScopeLabelsChanged();

    //! A label at or below this node moved. The aggregate of
    //! tripScopeLabelsChanged and childScopeLabelsChanged here (wired in the
    //! constructor, so no emit site can forget it) plus every descendant's, so a
    //! consumer that resolves names over a whole subtree subscribes once. A
    //! sibling-set signal stays honest: it means that set moved.
    void scopeLabelsChanged();

    //! Some trip in this node replaced its harvested station names — one pulse
    //! for "a station appeared somewhere in here", for the consumers that read
    //! more than the trip they are bound to (the tie suggester reads every trip
    //! in the node, looking for a partner). Without it the only pulse for a
    //! *sibling's* harvest is the re-solve a scan happens to request as well,
    //! which is an invariant spread across the scan's call sites and enforced by
    //! nothing.
    //!
    //! Deliberately not relayed back into each trip's knownStationsChanged, the
    //! way stationPositionPositionChanged is. A trip's harvest is its own field,
    //! so it already pulses for itself and the relay would double every one of
    //! those pulses; the asymmetry is honest, because positions do move for
    //! every trip at once and a harvest does not.
    void tripExternalStationsChanged();

    //! attachedFixes()' rows or the stations behind fileFixedStations() changed.
    void attachedFixesChanged();

    //! fixStationTable()'s row count changed.
    void fixStationCountChanged();

    void stationPositionPositionChanged();
    void surveyNetworkChanged();

    void externalCenterlineChanged();

    //! The NOTIFY for externallyBacked(). Fired when this node's own attachment
    //! flipped the answer, and relayed DOWN to each child node, because an
    //! ancestor's attachment backs everything beneath it. Never relayed upward,
    //! so the two directions cannot loop.
    void externallyBackedChanged();

    //! A structural change landed at or below this node: a trip or child node was
    //! inserted or removed. Emitted last by each funnel, so a consumer reads a
    //! settled node, and relayed one hop up per level.
    void subtreeChanged();

    //! The user deleted trips from this node through removeTrip(), the verb the
    //! deletion UI calls. Carries their ids, so a consumer holding per-trip
    //! state outside the project (the external-source breadcrumb store) can
    //! forget them. Silent on every path that takes a trip off the list while
    //! its id lives on: project close, load replacing the region, and a move to
    //! another node. Relayed one hop up per level; the region relays it out as
    //! cwCavingRegion::ownersDeleted.
    void tripsDeleted(const QList<QUuid>& tripIds);

    //! The user deleted a child node through removeNode(). Carries every owner
    //! id in the removed subtree — the node's own, its descendant nodes', and
    //! every descendant trip's — as one list, so a consumer forgets the whole
    //! subtree in one pass. Silent on a move and on undo/redo, for the reason
    //! tripsDeleted() gives. Relayed one hop up per level.
    void nodesDeleted(const QList<QUuid>& ownerIds);

public slots:
    /// Feed the node's current fix stations and the region's frame into the
    /// gridConvergence() readout, which caches the PROJ result and only re-emits
    /// when it actually changes. Wired to fix-station edits here, and to frame
    /// moves by the region.
    void recomputeGridConvergence();

private:
    cwSurveyNode(bool isRoot, QObject* parent);

    QList<cwTrip*> m_trips;
    QList<cwSurveyNode*> m_childNodes;
    QString m_name;

    cwLength* m_length;
    cwLength* m_depth;

    cwErrorModel* m_errorModel;
    cwFixStationModel* m_fixStations;
    //! Declared after m_fixStations, which it proxies.
    cwFixStationDiagnosticsModel* const m_fixStationDiagnostics;
    cwAttachedFixModel* const m_attachedFixes;
    //! Declared after the two models it concatenates.
    QConcatenateTablesProxyModel* const m_fixStationTable;

    cwStationPositionLookup m_stationPositionLookup;
    bool m_stationPositionLookupStale;
    QUuid m_id;

    cwSurveyNetwork m_network;
    cwSanitizedNameSet m_tripNames;
    cwSanitizedNameSet m_childNames;

    cwGridConvergence* m_gridConvergence;

    cwExternalCenterline m_externalCenterline;
    QStringList m_externalStations;
    QHash<QUuid, QStringList> m_fileFixedStations;

    cwKeywordModel* m_keywordModel = nullptr;
    void updateKeywords();
    void updateSubtreeKeywords();

    QPointer<cwSurveyNode> m_parentNode;
    const bool m_isRoot;

    Kind m_kind = Kind::Cave;
    bool m_takesCaves = false;
    bool m_readOnly = false;
    QUuid m_sourceId;
    QString m_sourcePath;

    cwSiblingLabelCache m_tripScopeLabels;
    void invalidateTripScopeLabels();

    cwSiblingLabelCache m_childScopeLabels;
    void invalidateChildScopeLabels();
    void invalidateChildScopeLabelsAfterTripChange();

    void wireScopeLabelAggregate();

    //! This node, then each node above it, ending at the topmost.
    QList<const cwSurveyNode*> ancestorsOrSelf() const;

    //! Recomputes takesCaves() here and below: a change at this node moves the
    //! answer for its whole subtree.
    void updateTakesCaves();
    bool computeTakesCaves() const;

    //! The chain path() and pathIds() read, root-most first, the root excluded.
    QList<const cwSurveyNode*> pathNodes() const;

    //! Wire a trip this node now lists, and unwire one it no longer does.
    //! Called from InsertRemoveTrip, the single funnel every insert and remove
    //! passes through.
    void connectTrip(cwTrip* trip);
    void disconnectTrip(cwTrip* trip);

    //! The same, for a child node, called only from InsertRemoveNode. Every
    //! relay is one hop: upward for the aggregates, downward for
    //! externallyBackedChanged, and never both.
    void connectNode(cwSurveyNode* node);
    void disconnectNode(cwSurveyNode* node);

    //! Only InsertRemoveNode writes parentage, so there is one place a node's
    //! place in the tree can change.
    void setParentNode(cwSurveyNode* node);

    //! removeTrip()'s body without the tripsDeleted() notification, for the
    //! paths that take a trip off this node's list without deleting it.
    void removeTripInternal(int i);

    //! removeNode()'s body without the nodesDeleted() notification — the move
    //! path, where the node's ids live on under their new parent.
    void removeNodeInternal(int row);

    //! The shared front half of insertNode() and addNodes() for a node no
    //! parent lists yet: cycle refusal and the sibling-unique rename.
    bool prepareChildForInsert(cwSurveyNode* node, cwSanitizedNameSet& siblingNames);

    //! insertNode()'s branch for a node another parent already lists: the
    //! cycle and no-op refusals, then one MoveNodeCommand.
    void moveNodeHere(int row, cwSurveyNode* node);

    void addTripNullHelper();

    //! What a move of a subject to this node does beyond the move, worked out
    //! from the tree as it stands before the move.
    struct MovePlan {
        //! The node the subject leaves.
        cwSurveyNode* source = nullptr;
        //! A trip move's new ties, one per station name shared with the trips
        //! left behind.
        QList<cwEquate> ties;
        //! Station names a moving trip shares with the trips already here.
        int joinedCount = 0;
        //! Canonical station names leaving \a source: a trip move's names no
        //! trip left behind carries.
        QSet<QString> leavingNames;
        //! A node move's scope label under \a source; its stations are
        //! named "<label>.<tail>" there.
        QString leavingLabel;
        //! Rows of the source's fix table whose station leaves with the subject.
        QList<int> leavingFixRows;
    };
    MovePlan planMove(const QObject* subject) const;

    virtual void setUndoStackForChildren();

////////////////////// Undo Redo commands ///////////////////////////////////
    class NameCommand : public QUndoCommand {
    public:
        NameCommand(cwSurveyNode* node, QString name);
        void redo();
        void undo();
    private:
        void rename(const QString& oldName, const QString& newName);

        cwSurveyNode* NodePtr;
        QString newName;
        QString oldName;
    };

    class InsertRemoveTrip : public QUndoCommand {
    public:
        InsertRemoveTrip(cwSurveyNode* node, int beginIndex, int endIndex);
        ~InsertRemoveTrip();

        //! True while a node lists \a trip among its trips.
        static bool isListed(const cwTrip* trip);

    protected:
        void insertTrips();
        void removeTrips();

        QList<QPointer<cwTrip>> Trips;
    private:
        cwSurveyNode* NodePtr;
        int BeginIndex;
        int EndIndex;
        bool OwnsTrips;
    };

    class InsertTripCommand : public InsertRemoveTrip {
    public:
        InsertTripCommand(cwSurveyNode* node, cwTrip* Trip, int index);
        InsertTripCommand(cwSurveyNode* node, QList<cwTrip*> Trip, int index);
        virtual void redo();
        virtual void undo();
    };

    class RemoveTripCommand : public InsertRemoveTrip {
    public:
        RemoveTripCommand(cwSurveyNode* node, int beginIndex, int endIndex);
        virtual void redo();
        virtual void undo();
    };

    class InsertRemoveNode : public QUndoCommand {
    public:
        InsertRemoveNode(cwSurveyNode* parentNode, int beginIndex, int endIndex);
        ~InsertRemoveNode();

    protected:
        void insertNodes();
        void removeNodes();

        QList<QPointer<cwSurveyNode>> Nodes;
    private:
        cwSurveyNode* ParentPtr;
        int BeginIndex;
        int EndIndex;
        bool OwnsNodes;
    };

    class InsertNodeCommand : public InsertRemoveNode {
    public:
        InsertNodeCommand(cwSurveyNode* parentNode, cwSurveyNode* node, int index);
        InsertNodeCommand(cwSurveyNode* parentNode, const QList<cwSurveyNode*>& nodes, int index);
        virtual void redo();
        virtual void undo();
    };

    class RemoveNodeCommand : public InsertRemoveNode {
    public:
        RemoveNodeCommand(cwSurveyNode* parentNode, int beginIndex, int endIndex);
        virtual void redo();
        virtual void undo();
    };

    //! One node's move to another parent, or to another row of the same parent,
    //! as a single undo step.
    //!
    //! Composed of the remove and insert commands the tree already uses, run
    //! back to back, so every consumer sees the signal pairs it always has and
    //! no model learns a new shape. What the composition adds is the undo: the
    //! node returns to the parent, row and name it left, rather than being
    //! removed a second time.
    class MoveNodeCommand : public QUndoCommand {
    public:
        MoveNodeCommand(cwSurveyNode* node, cwSurveyNode* newParent, int newRow);
        virtual void redo();
        virtual void undo();

    private:
        //! Writes \a desiredName, deduplicated against \a siblingNames, onto
        //! the node while no sibling set holds it — so the name is a plain
        //! assignment and the insert that follows is what registers it.
        void renameWhileUnlisted(const cwSanitizedNameSet& siblingNames,
                                 const QString& desiredName);

        cwSurveyNode* NodePtr;
        cwSurveyNode* OldParentPtr;
        cwSurveyNode* NewParentPtr;
        RemoveNodeCommand Remove;
        InsertNodeCommand Insert;
        QString OldName;
    };

    //! One trip's move to another node as a single undo step, the trip-side
    //! twin of MoveNodeCommand: the trip returns to the node, row and name it
    //! left.
    class MoveTripCommand : public QUndoCommand {
    public:
        MoveTripCommand(cwTrip* trip, cwSurveyNode* newParent);
        virtual void redo();
        virtual void undo();

    private:
        void renameWhileUnlisted(const cwSanitizedNameSet& siblingNames,
                                 const QString& desiredName);

        cwTrip* TripPtr;
        cwSurveyNode* OldParentPtr;
        cwSurveyNode* NewParentPtr;
        RemoveTripCommand Remove;
        InsertTripCommand Insert;
        QString OldName;
    };

    //! "Move to…": the move of a node or trip plus what crossing a scope
    //! boundary costs — new ties, re-keyed equates, traveling fixes — as one
    //! undo step.
    class MoveToCommand : public QUndoCommand {
    public:
        MoveToCommand(QObject* subject, cwSurveyNode* destination, const MovePlan& plan);
        virtual void redo();
        virtual void undo();

    private:
        struct EquateEdit {
            cwEquate before;
            cwEquate after;
        };

        struct FixEdit {
            int sourceRow;
            cwFixStation before;
            cwFixStation after;
        };

        //! Works out the equate re-keys and fix edits once the subject hangs
        //! in its new place, where a node's new scope label is known.
        void finishPlan();

        QObject* SubjectPtr;
        cwSurveyNode* SourcePtr;
        cwSurveyNode* DestinationPtr;
        QPointer<cwCavingRegion> Region;
        std::unique_ptr<QUndoCommand> Move;
        MovePlan Plan;
        bool Planned = false;
        QList<EquateEdit> EquateEdits;
        QList<FixEdit> FixEdits;
    };

};

//We need to use the forward declared version of cwTrip above because cwTrip and
//cwSurveyNode are circularly dependent. moc needs cwTrip fully declared, so
//cwTrip.h is included after cwSurveyNode is fully declared.
//Q_DECLARE_OPAQUE_POINTER(cwTrip*) causes QML types to abort.
#include "cwTrip.h"

/**
  \brief Get's the name of the node
  */
inline QString cwSurveyNode::name() const {
    return m_name;
}

inline QUuid cwSurveyNode::id() const
{
    return m_id;
}

/**
  \brief Get's the number of survey trips directly in the node
  */
inline int cwSurveyNode::tripCount() const {
    return m_trips.count();
}

/**
  \brief Get's all the trips directly in the node
  */
inline QList<cwTrip*> cwSurveyNode::trips() const {
    return m_trips;
}

/**
  \brief Get's the trip at an index

  If the index is out of bounds this return nullptr
  */
inline cwTrip* cwSurveyNode::trip(int index) const {
    if(index < 0 || index >= m_trips.size()) { return nullptr; }
    return m_trips.at(index);
}

/**
  \brief Get's all the child nodes of this node
  */
inline QList<cwSurveyNode*> cwSurveyNode::childNodes() const {
    return m_childNodes;
}

/**
  \brief Get's the child node at an index, nullptr when out of bounds
  */
inline cwSurveyNode* cwSurveyNode::childNode(int index) const {
    if(index < 0 || index >= m_childNodes.size()) { return nullptr; }
    return m_childNodes.at(index);
}

inline int cwSurveyNode::childNodeCount() const {
    return m_childNodes.count();
}

inline int cwSurveyNode::indexOfNode(cwSurveyNode* node) const {
    return m_childNodes.indexOf(node);
}

inline bool cwSurveyNode::isListedByParent() const {
    cwSurveyNode* parent = m_parentNode;
    return parent != nullptr
            && parent->indexOfNode(const_cast<cwSurveyNode*>(this)) >= 0;
}

/**
 * @brief cwSurveyNode::length
 * @return The node's current length
 */
inline cwLength *cwSurveyNode::length() const
{
   return m_length;
}

/**
 * @brief cwSurveyNode::depth
 * @return The node's current depth
 */
inline cwLength *cwSurveyNode::depth() const
{
    return m_depth;
}

/**
  \brief Gets the index of the trip inside of the node
  */
inline int cwSurveyNode::indexOf(cwTrip* trip) const {
    return m_trips.indexOf(trip);
}

/**
  \brief Gets the station position model for the node
  */
inline cwStationPositionLookup cwSurveyNode::stationPositionLookup() const
{
    return m_stationPositionLookup;
}

/**
 * @brief cwSurveyNode::network
 * @return This returns a lookup for stations to station neighbors. This is undirected graph
 * of stations.
 */
inline cwSurveyNetwork cwSurveyNode::network() const
{
    return m_network;
}

/**
* @brief cwSurveyNode::errorModel
* @return Returns the error model, the database of current errors for the node.
*
* These are errors that are created by the user and should fixed by the user. The
* error model doesn't report bugs in cavewhere. For example the error model will have
* store errors like a survey leg isn't corrected to the rest of the cave.
*/
inline cwErrorModel* cwSurveyNode::errorModel() const {
    return m_errorModel;
}

#endif // CWSURVEYNODE_H
