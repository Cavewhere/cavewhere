/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVERYCHUNKGROUP_H
#define CWSURVERYCHUNKGROUP_H

#include <QPointer>

//Our includes
#include "cwStation.h"
#include "cwStationHandle.h"
#include "cwGlobals.h"
#include "cwExternalCenterline.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "cwTripData.h"
class cwScopeLabels;
// #include "cwTripCalibration.h"
class cwTripCalibration;
#include "cwUndoer.h"
#include "cwSurveyNode.h"
class cwCave;
// #include "cwTeam.h"
#include "cwSurveyChunk.h"
class cwSurveyChunk;
// class cwCave;
class cwSurveyNoteModel;
class cwSurveyNoteLiDARModel;
class cwSurveyNoteSketchModel;
class cwShot;
class cwErrorModel;
class cwTeam;
class cwKeywordModel;

//Qt include
#include <QObject>
#include <QList>
#include <QPair>
#include <QVector3D>
#include <QAbstractListModel>
#include <QWeakPointer>
#include <QDate>
#include <QUndoCommand>
#include <QQmlEngine>
#include <QStringList>
#include <QUuid>

//Std includes
#include <optional>


class CAVEWHERE_LIB_EXPORT cwTrip : public QObject, public cwUndoer
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Trip)

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QUuid id READ id NOTIFY idChanged FINAL)
    Q_PROPERTY(QDateTime date READ date WRITE setDate NOTIFY dateChanged)
    Q_PROPERTY(cwSurveyNoteModel* notes READ notes CONSTANT)
    Q_PROPERTY(cwSurveyNoteLiDARModel* notesLiDAR READ notesLiDAR CONSTANT);
    Q_PROPERTY(cwSurveyNoteSketchModel* notesSketch READ notesSketch CONSTANT);
    Q_PROPERTY(cwTeam* team READ team CONSTANT)
    Q_PROPERTY(int chunkCount READ chunkCount NOTIFY numberOfChunksChanged)
    Q_PROPERTY(cwTripCalibration* calibration READ calibrations CONSTANT)
    Q_PROPERTY(cwSurveyNode* parentNode READ parentNode NOTIFY parentNodeChanged)
    Q_PROPERTY(cwCave* parentCave READ parentCave WRITE setParentCave NOTIFY parentCaveChanged)
    Q_PROPERTY(cwErrorModel* errorModel READ errorModel CONSTANT)
    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)
    Q_PROPERTY(cwExternalCenterline externalCenterline READ externalCenterline WRITE setExternalCenterline NOTIFY externalCenterlineChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY sourcePathChanged)
    Q_PROPERTY(QString scopePrefix READ scopePrefix NOTIFY scopeChanged)
    Q_PROPERTY(bool isScoped READ isScoped NOTIFY scopeChanged)
    Q_PROPERTY(bool externallyBacked READ externallyBacked NOTIFY externallyBackedChanged)
    Q_PROPERTY(bool windowsWholeNode READ windowsWholeNode NOTIFY externallyBackedChanged)
    Q_PROPERTY(double solvedLength READ solvedLength NOTIFY solvedLengthChanged)
    Q_PROPERTY(QStringList externalStations READ externalStations NOTIFY externalStationsChanged FINAL)
    Q_PROPERTY(QString externalStationsError READ externalStationsError NOTIFY externalStationsErrorChanged FINAL)
    Q_PROPERTY(cwAttachedFixModel* attachedFixes READ attachedFixes CONSTANT)

public:
    explicit cwTrip(QObject *parent = 0);

    // [[deprecated]]
    // cwTrip(const cwTrip& object);
    // [[deprecated]]
    // cwTrip& operator=(const cwTrip& object);

    ~cwTrip();

    QString name() const;
    void setName(QString name);
    Q_INVOKABLE QString validateName(const QString& proposedName) const;
    QUuid id() const;
    void setId(const QUuid& id);

    cwExternalCenterline externalCenterline() const { return m_externalCenterline; }
    void setExternalCenterline(const cwExternalCenterline& value);

    //! Where this trip sits inside its source's scan ("BLOWING3.DAT/OF50-87"),
    //! empty on a native trip — including a native trip under a derived node.
    //! Persisted so a derived trip keeps its identity across a save; the
    //! reconcile pass that fills it arrives with external sources.
    QString sourcePath() const { return m_sourcePath; }
    void setSourcePath(const QString& sourcePath);

    //! The sourcePath() of a source root's own unit: the entry file's own
    //! shots, which the root's whole-node window owns.
    static QString ownUnitSourcePath();

    //! True when a survey file derives this trip: its stations are one unit of
    //! the file's scan, pushed into externalStations() by the harvest.
    bool isDerived() const { return !m_sourcePath.isEmpty(); }

    //! The station names this trip's attached external centerline declares, in
    //! the trip's own namespace — canonical and sorted, the same spelling
    //! solvedStations() yields once the region solve does place the survey.
    //! A derived trip holds its unit's station set instead, from its source's
    //! units.json, with its node's cavern path and its own cavernLabel()
    //! stripped ("a5" under Alpha).
    //! Learned at scan time by running cavern over the attachment alone
    //! (cwExternalStationHarvest), so they exist whether or not the region solve
    //! placed it: cavern drops a survey nothing fixes and nothing ties in, which
    //! is exactly the survey whose names the user needs in order to tie it in.
    //!
    //! Empty for a native trip, and for an attachment cavern cannot read — in
    //! which case externalStationsError() says why. Derived state: rebuilt
    //! wholesale by every external-centerline scan, never persisted, and never a
    //! save trigger.
    QStringList externalStations() const { return m_externalStations; }
    void setExternalStations(const QStringList& stations);

    //! Why this trip's attached file yielded no station names; empty when the
    //! file read cleanly or the trip has no attachment. Usually cavern's own
    //! complaint, from the same scan-time run that fills externalStations().
    //! It can also be a CaveWhere policy refusal decided before cavern ran at
    //! all — an attachment whose *include reaches outside the project's data
    //! root is rejected unread (see B7 in plans/EXTERNAL_FILE_PHASE2.html) — so
    //! a non-empty error alongside empty names does not imply the file is
    //! malformed. Derived state, with the same no-persist, no-save rules as
    //! externalStations().
    QString externalStationsError() const { return m_externalStationsError; }
    void setExternalStationsError(const QString& error);

    //! The naming level cavern builds for this derived trip, from its source's
    //! units.json: the block name of a leaf Survex block, which is a trip rather
    //! than a node. Empty for every other trip. Derived like externalStations().
    QString cavernLabel() const { return m_cavernLabel; }
    void setCavernLabel(const QString& label);

    //! The fixes this derived trip's leaf Survex block carries, as the attach
    //! scan read them, for the Fix Stations page to list read-only under the
    //! trip. Empty for every other trip. Derived like externalStations().
    cwAttachedFixModel* attachedFixes() const { return m_attachedFixes; }
    //! Feeds attachedFixes(), emitting attachedFixesChanged() when the list differs.
    void setAttachedFixes(const QList<cwAttachedFix>& fixes);

    //! The scope prefix this trip's stations carry in its node's namespace:
    //! "<tripLabel>." for an externally-attached trip (see cwCavernNaming) and
    //! for a native trip inside a sourced subtree, which the driver writes as
    //! its own block in the root's (cwScopeLabels); "<cavernLabel>." for a
    //! derived trip with a cavernLabel(), the leaf Survex block it stands for;
    //! empty for a flat native trip and every other derived trip, whose
    //! stations sit in its node's view as they are.
    //! The one accessor every scope consumer — equates, the solved-* accessors,
    //! and the geometry enumeration — speaks, so the trip kinds never diverge.
    QString scopePrefix() const;

    //! Snapshot overload: the same policy computed from a cwTripData held by
    //! the node \a nodeId, so the worker-thread passes share this one source of
    //! truth. Takes the labels cwScopeLabels assigned, so an emitter that
    //! already holds that pool (the survex exporter's "*begin" loop, its equate
    //! operands, the geometry pass) names the very same scope it opened rather
    //! than deriving a second answer that then has to agree.
    static QString scopePrefix(const cwTripData& data, const cwScopeLabels& labels,
                               const QUuid& nodeId);

    //! True when this trip's stations carry a scope prefix: an attached trip,
    //! a native trip inside a sourced subtree, or a derived trip with a
    //! cavernLabel().
    bool isScoped() const;

    //! True when an external survey file — this trip's own attachment, or the
    //! one its cave attached — is what places this trip's stations. Both kinds
    //! of externally backed trip take dotted station names, show the external
    //! panel instead of the shot editor, and carry the 📎 marker; this is the
    //! one predicate all three surfaces ask.
    //!
    //! Derived from the trip's own attachment and its node's, so it can never
    //! disagree with either. A native trip inside a sourced subtree is the
    //! user's own survey, so it answers false.
    bool externallyBacked() const;

    //! True when this trip is its source root's whole-node window: the root's
    //! own unit (ownUnitSourcePath()), which owns every station of the root's
    //! view no other unit and no native trip claims — a Survex station outside
    //! every named "*begin", a Walls station with no prefix in force.
    //!
    //! Its scope is the root itself, so scopePrefix() stays empty. What the
    //! predicate changes is who lists and draws those stations. Derived state:
    //! nothing persists it.
    bool windowsWholeNode() const;

    //! Snapshot overload: the same policy over a cwTripData and the cwCaveData
    //! that holds it, so the worker-thread geometry pass and the UI decide
    //! alike.
    static bool windowsWholeNode(const cwTripData& trip, const cwCaveData& node);

    //! The length in meters of the legs this trip draws in the last solve, the
    //! per-trip counterpart of cwSurveyNode::length(). It is what an externally
    //! backed trip, which has no shots to add up, reports as its length.
    //! Recomputed every solve and never saved; 0 before the first solve.
    double solvedLength() const;
    void setSolvedLength(double meters);

    QDateTime date() const;
    void setDate(QDateTime date);

    // void setTeam(cwTeam* team);
    cwTeam* team() const;

    // void setCalibration(cwTripCalibration* calibrations);
    cwTripCalibration* calibrations() const;

    cwSurveyNoteModel* notes() const;
    cwSurveyNoteLiDARModel* notesLiDAR() const;
    cwSurveyNoteSketchModel* notesSketch() const;
    cwKeywordModel* keywordModel() const;
    cwKeywordModel* linePlotKeywordModel();
    cwKeywordModel* splaysKeywordModel();

    void addShotToLastChunk(const cwStation& fromStation, const cwStation& toStation, const cwShot& shot);
    void removeChunks(int begin, int end);
    void insertChunk(int row, cwSurveyChunk* chunk);
    void addChunk(cwSurveyChunk* chunk);
    Q_INVOKABLE void addNewChunk();
    Q_INVOKABLE void removeChunk(cwSurveyChunk* chunk);

    int chunkCount() const;
    Q_INVOKABLE cwSurveyChunk* chunk(int i) const;
    QList<cwSurveyChunk*> chunks() const;

    //! The survey node holding this trip — the trip's place in the tree, and the
    //! sibling set its label is unique in.
    cwSurveyNode* parentNode() const { return m_parentNode; }
    void setParentNode(cwSurveyNode* parentNode);

    //! parentNode() as a cwCave, for the QML pages and callers still typed that
    //! way. Every node constructed today is a cwCave, so it answers for all of
    //! them; it goes away with cwCave itself.
    void setParentCave(cwCave* parentCave);
    cwCave* parentCave() const;

    //! The unit system in effect for this trip: its node's, or Metric with no
    //! node yet. Delegates up to cwSurveyNode::unitSystem() — the project default.
    cwUnits::UnitSystem unitSystem() const;

    QList< cwStation > stations() const;
    QList< cwStation > uniqueStations() const;

    //Network operations
    Q_INVOKABLE int numberOfStations() const;
    bool hasStation(QString stationName) const;
    QSet<cwStation> neighboringStations(QString stationName) const;

    //! This trip's solved station positions, keyed in the trip's own local
    //! namespace. A native trip returns the cave lookup unchanged, and so does
    //! a derived trip, whose node's view already names its stations locally. An
    //! externally-attached trip (whose solved stations are keyed
    //! "<scopePrefix><tail>" in the cave) gets a stripped-tail alias for each of
    //! its scoped entries so bare note/scrap/lead station names resolve, while
    //! the full cave data is retained so a cross-trip tie-in neighbor still
    //! carries a position. Consumers never need to know the trip is external.
    cwStationPositionLookup solvedStationPositions() const;

    //! This trip's solved survey network in the trip's own local namespace,
    //! with the same native pass-through / external re-key contract as
    //! solvedStationPositions(). Prefer this over the chunk-based
    //! neighboringStations() when resolving solved data: it is non-empty for
    //! an externally-attached (chunk-less) trip and includes cross-trip
    //! junction neighbors.
    cwSurveyNetwork solvedNetwork() const;

    //! This trip's own solved stations as (local name, position) pairs. Unlike
    //! solvedStationPositions() — a resolution lookup that carries the full cave
    //! as a superset — this enumerates only the stations this trip owns, in the
    //! trip's local namespace: a native trip's chunk stations that have a solved
    //! position, a scoped trip's scope-relative tails, a derived trip's unit
    //! set (a station two units share belongs to both), or — for the
    //! whole-node window (windowsWholeNode()) — every solved root station no
    //! other unit or native trip claims, named as the root's view keys it. The
    //! authoritative answer to "what are my stations, and where are they?" for
    //! the line-plot geometry and label views, native or external alike.
    QList<QPair<QString, QVector3D>> solvedStations() const;

    //! One station this trip is known to have, named by the scope-relative tail
    //! knownStations() speaks. The position is set only where the region solve
    //! placed it: a station can be known without being anywhere yet.
    struct KnownStation {
        QString name;
        std::optional<QVector3D> position;
    };

    //! Every station this trip is known to have, in the trip's own namespace.
    //!
    //! The union of every source that knows one — the trip's chunks, the scan's
    //! per-file harvest (externalStations()), and the region solve
    //! (solvedStations()) — never a precedence among them, because each of the
    //! three genuinely knows names the others cannot. Chunks name a station the
    //! solve dropped for being unreachable; the harvest names an attachment
    //! cavern dropped whole; and the solve names the parts of a multi-component
    //! attachment the harvest itself lost (issue #651). Preferring one source
    //! would make each consumer's answer depend on which one it picked, which is
    //! the disagreement this accessor exists to end.
    //!
    //! Deduplicated by canonical key, keeping the authored spelling where a
    //! source has one, and sorted in natural order ("a2" before "a10"). The one
    //! answer to "what stations does this trip have?" — every consumer asking it
    //! of a live trip calls this, and knownStationsChanged() is the single pulse
    //! that says the answer moved.
    QList<KnownStation> knownStations() const;

    //! Snapshot overload: the same union over a cwTripData, for the worker-side
    //! passes that hold no live trip.
    //!
    //! \a scopeLocalSolved is what the solve knows for this trip, in the trip's
    //! own namespace — a snapshot carries no cave lookup, so the caller that
    //! already derived the solved names (cwFindFloatingSurveys, off the region
    //! network) hands them in rather than this deriving a second answer that
    //! then has to agree. Empty is the honest answer when nothing solved.
    static QStringList knownStationNames(const cwTripData& trip,
                                         const QStringList& scopeLocalSolved);

    //! The identity of one of this trip's stations, named by the tail
    //! solvedStations() yields. Branches on the same scopePrefix() emptiness
    //! that solvedStations() does, so a tail and the handle naming it can never
    //! disagree about which scope it came from: a native trip's stations belong
    //! to the cave's scope, a scoped trip's to the trip's. The one producer of
    //! a cwStationHandle from a live object.
    cwStationHandle stationHandle(const QString& tail) const;

    // void stationPositionModelUpdated();

    cwErrorModel* errorModel() const;

    cwTripData data() const;
    void setData(const cwTripData& data);

signals:
    void nameChanged();
    void idChanged();
    void dateChanged(QDateTime date);
    void chunksInserted(int begin, int end);
    void chunksAboutToBeRemoved(int begin, int end);
    void chunksRemoved(int begin, int end);
    // void teamChanged();
    // void calibrationChanged();
    // void notesChanged();
    void numberOfChunksChanged();
    void parentNodeChanged();
    void parentCaveChanged();
    void externalCenterlineChanged();

    void sourcePathChanged();

    //! Fired when the scan-time harvest replaced this trip's externalStations().
    //! A consumer that also cares when cavern's complaint changes wants
    //! externalStationsErrorChanged() as well — the two fields move
    //! independently, and each signal covers only its own. Deliberately not
    //! wired to anything that saves: both are derived from files the project
    //! already owns.
    void externalStationsChanged();
    void externalStationsErrorChanged();

    //! attachedFixes()' rows changed. Chained to the parent node's
    //! cwSurveyNode::attachedFixesChanged, so a node-level listener hears it.
    void attachedFixesChanged();

    //! Fired when this trip's scope may have moved: its own externalCenterline,
    //! cavernLabel or name changed, or — chained from
    //! cwCave::tripScopeLabelsChanged — a *sibling* was renamed, added or
    //! removed, which can move this trip's collision suffix. The NOTIFY for both
    //! derived properties, and safe to bind against.
    //!
    //! Like most NOTIFY signals it may fire when nothing actually moved (a
    //! sibling's rename pulses every trip in the cave), so consumers must be
    //! idempotent. A trip its cave no longer lists gets only the first half.
    void scopeChanged();

    //! The NOTIFY for externallyBacked(). Fired when this trip's own
    //! externalCenterline or parent cave changed, and — chained
    //! from cwCave::externalCenterlineChanged in cwCave::connectTrip — when the
    //! cave that backs it attached or detached a source. May fire when the
    //! answer did not move, as scopeChanged() notes.
    void externallyBackedChanged();

    //! The NOTIFY solvedStations() never had. Chained from the owning node's
    //! scopedPositionsChanged in cwSurveyNode::connectTrip, which the region
    //! relays down the tree once per solve: a trip's solved positions live in
    //! the region's lookup, so the getter is here but only the region knows
    //! when it moved.
    //!
    //! Deliberately not chained from scopeChanged: a scope move lands before the
    //! re-solve that follows it, and solvedStations() would match the new prefix
    //! against the old keys and answer empty until the solve catches up. Holding
    //! the stale-but-populated answer is the better failure. A trip its cave no
    //! longer lists is cut off from this, as it is from the cave's other pulses.
    void solvedStationsChanged();

    //! The one pulse for knownStations(), so a consumer subscribes to the
    //! accessor it reads instead of to the fields behind it and cannot be caught
    //! out by a source it forgot. May fire spuriously, as scopeChanged() notes.
    //!
    //! It speaks for this trip only. A consumer that reads more than the trip it
    //! is bound to wants cwCave::tripExternalStationsChanged as well — that is
    //! the cave's aggregate, and the harvest is the one source a sibling can
    //! move without the cave's lookup moving too.
    //!
    //! Deliberately not chained from scopeChanged(), for the reason
    //! solvedStationsChanged() gives: a scope move lands before the re-solve
    //! that follows it, and the stale-but-populated answer is the better one to
    //! hold in that window.
    //!
    //! A chunk edit reaches this only through the solve that follows it, since
    //! nothing here watches chunks station by station. Wiring it would mean
    //! per-chunk connections plus a debounce — a station's name arrives through
    //! cwSurveyChunk::dataChanged, which fires per keystroke — so there is no
    //! cheap structural subset to wire. It is why a station typed into a cave
    //! that will not solve stays unlisted until one does.
    void knownStationsChanged();

    void solvedLengthChanged();

public slots:
    void setChucks(QList<cwSurveyChunk*> chunks);


protected:
    QList<cwSurveyChunk*> Chunks;
    QString Name;
    QDateTime DateTime;
    cwTeam* Team;
    cwTripCalibration* Calibration;
    QPointer<cwSurveyNode> m_parentNode;
    cwSurveyNoteModel* Notes;
    cwSurveyNoteLiDARModel* NotesLidar;
    cwSurveyNoteSketchModel* NotesSketch;
    cwErrorModel* ErrorModel; //!<
    cwKeywordModel* KeywordModel;

    //Lazily created keyword model that identifies this trip's line plot
    //(Type="Line Plot") and inherits the trip's keywords via an extension. The
    //line plot geometry's and the station labels' keyword items both reference
    //this, so the "this is line plot" identity is owned by the trip.
    cwKeywordModel* m_linePlotKeywordModel = nullptr;

    //Lazily created keyword model that identifies this trip's splay shots
    //(Type="Splays") and inherits the trip's keywords via an extension, the
    //splay counterpart of m_linePlotKeywordModel.
    cwKeywordModel* m_splaysKeywordModel = nullptr;

    cwKeywordModel* identityKeywordModel(cwKeywordModel*& model, const QString& type);

    QUuid Id;
    cwExternalCenterline m_externalCenterline;
    QString m_sourcePath;
    QStringList m_externalStations;
    QString m_externalStationsError;
    QString m_cavernLabel;
    double m_solvedLength = 0.0;
    cwAttachedFixModel* const m_attachedFixes;

    //Units


    virtual void setUndoStackForChildren();
private:
    //A trip moving between nodes is renamed while no node lists it, as part of
    //the node's own move command, so the rename and the move undo as one.
    friend class cwSurveyNode;

    // void Copy(const cwTrip& object);

    class NameCommand : public QUndoCommand {
    public:
        NameCommand(cwTrip* trip, QString name);
        void redo();
        void undo();
    private:
        cwTrip* Trip;
        QString NewName;
        QString OldName;
    };

    class DateCommand : public QUndoCommand {
    public:
        DateCommand(cwTrip* trip, QDateTime date);
        void redo();
        void undo();
    private:
        cwTrip* Trip;
        QDateTime NewDate;
        QDateTime OldDate;
    };


    void updateKeywordMetadata();
};

// Q_DECLARE_METATYPE(cwTrip*)

/**
Gets numberOfChunks
*/
inline int cwTrip::chunkCount() const {
    return chunks().size();
}

/**
  \brief Get's the name of the survey trip
  */
inline QString cwTrip::name() const {
    return Name;
}

inline QUuid cwTrip::id() const
{
    return Id;
}

inline double cwTrip::solvedLength() const
{
    return m_solvedLength;
}

/**
  Gets the date of the survey trip
  */
inline QDateTime cwTrip::date() const {
    return DateTime;
}

/**
  \brief Gets the survey team for the trip
  */
inline cwTeam* cwTrip::team() const {
    return Team;
}

/**
  \brief Gets the trip's calibration
  */
inline cwTripCalibration* cwTrip::calibrations() const {
    return Calibration;
}

/**
  \brief This gets the notes for a trip
  */
inline cwSurveyNoteModel* cwTrip::notes() const {
    return Notes;
}

inline cwSurveyNoteLiDARModel *cwTrip::notesLiDAR() const
{
    return NotesLidar;
}

inline cwSurveyNoteSketchModel *cwTrip::notesSketch() const
{
    return NotesSketch;
}

/**
* @brief class::errorModel
* @return
*/
inline cwErrorModel* cwTrip::errorModel() const {
    return ErrorModel;
}

inline cwKeywordModel* cwTrip::keywordModel() const
{
    return KeywordModel;
}

//cwCave completes the circular trio: a TU that reaches here through cwCave.h or
//cwSurveyNode.h parses cwTrip's body while cwCave is still incomplete, which is
//why parentCave() is out of line in cwTrip.cpp.
#include "cwCave.h"

#endif // CWSURVERYCHUNKGROUP_H
