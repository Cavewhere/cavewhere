/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAVINGREGION_H
#define CWCAVINGREGION_H

//Qt includes
#include <QObject>
#include <QList>
#include <QUndoCommand>
#include <QWeakPointer>
#include <QAbstractListModel>
#include <QDebug>
#include <QHash>
#include <QSharedPointer>
#include <QQmlEngine>
#include <QObjectBindableProperty>
#include <QUuid>

//Our includes
class cwCave;
class cwProject;
#include "cwCavingRegionData.h"
#include "cwEquateModel.h"
#include "cwFixStationValidator.h"
#include "cwGeoReference.h"
#include "cwLocalProjectionManager.h"
#include "cwLazLayerModel.h"
#include "cwSurveyNode.h"
#include "cwUndoer.h"
#include "cwGlobals.h"
#include "cwFutureManagerToken.h"
#include "cwUnits.h"


class CAVEWHERE_LIB_EXPORT cwCavingRegion : public QAbstractListModel, public cwUndoer
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CavingRegion)

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged BINDABLE bindableName)
    Q_PROPERTY(int caveCount READ caveCount NOTIFY caveCountChanged)
    Q_PROPERTY(cwGeoReference* geoReference READ geoReference CONSTANT)
    Q_PROPERTY(cwFixStationValidator* fixStationValidator READ fixStationValidator CONSTANT)
    Q_PROPERTY(cwLazLayerModel* lazLayers READ lazLayers CONSTANT)
    Q_PROPERTY(cwLocalProjectionManager* localProjection READ localProjection CONSTANT)
    Q_PROPERTY(cwUnits::UnitSystem unitSystem READ unitSystem WRITE setUnitSystem NOTIFY unitSystemChanged)
    Q_PROPERTY(QString defaultFixDatum READ defaultFixDatum NOTIFY defaultFixDatumChanged)
    Q_PROPERTY(cwEquateModel* equates READ equates CONSTANT)

public:
    enum Roles {
        CaveObjectRole
    };
    Q_ENUM(Roles)

    explicit cwCavingRegion(QObject *parent = nullptr);
    // cwCavingRegion(const cwCavingRegion& object);
    // cwCavingRegion& operator=(const cwCavingRegion& object);
//    ~cwCavingRegion() { qDebug() << "Deleted: " << this; }

    QString name() const { return m_name.value(); }
    void setName(const QString& name) { m_name = name; }
    QBindable<QString> bindableName() { return &m_name; }

    // The geo-reference (the project's local projection) is owned here but is
    // the single home for that state: consumers read it through
    // region.geoReference, not through the region itself. The region only
    // retains the responsibility that genuinely needs its other data — pushing
    // the frame into lazLayers, which it owns.
    cwGeoReference* geoReference() const { return m_geoReference; }

    //! Detects fix stations whose coordinate is a data-entry error (far from the
    //! rest of the survey). Owned here because the check is region-scoped, but
    //! the geometry/attribution logic lives in the validator, not the region.
    cwFixStationValidator* fixStationValidator() const { return m_fixStationValidator; }

    cwLazLayerModel* lazLayers() const { return m_lazLayers; }

    //! Drives the local projection through its lifecycle, and is where the user
    //! recenters it from. Owned here because the policy needs the region's caves
    //! and layers; read through region.localProjection, not through the region.
    cwLocalProjectionManager* localProjection() const { return m_localProjectionManager; }

    //! The datum a coordinate typed into this project is most likely on, as a
    //! geographic code from cwCoordinateSystem's datum table. Read by every
    //! surface that offers a datum, so that the answer comes from what the
    //! project already holds rather than from a hardcoded WGS84.
    //!
    //! The first enabled GIS layer that names a datum wins, because a fix is
    //! usually being placed against the terrain those tiles draw and a datum
    //! shift shows up as the cave sitting beside the sinkhole it belongs in.
    //! Without a layer the frame answers, since it inherited its datum from
    //! whatever georeferenced the project. With neither, WGS84 — what a phone
    //! reports.
    //!
    //! Answered on demand rather than cached: it is asked at commit and pick
    //! time, and the PROJ lookup behind it is memoized per thread.
    Q_INVOKABLE QString defaultFixDatum() const;

    //! The system defaultFixDatum() reads its answer off — the enabled GIS
    //! layer that names a datum, else the frame — and empty when neither does,
    //! which is the case the datum answers with the WGS84 fallback.
    QString defaultFixSourceCS() const;

    void setFutureManagerToken(const cwFutureManagerToken& token);

    //! Every equate tie in the project, wherever its stations sit: within one
    //! node, across nodes, or across caves. The exporter emits each one at
    //! region scope with fully qualified operands.
    cwEquateModel* equates() const { return m_equates; }

    //! The cave holding the container a handle names, or nullptr when no cave in
    //! this region does. A NativeCave handle names its cave outright; a Trip
    //! handle names a trip, and exactly one cave lists that trip.
    cwCave* caveFor(const cwStationHandle& handle) const;

    //! Record that two stations are one physical point, as an equate in this
    //! region's list. A pair inside one node must also pass that node's
    //! cwSurveyNode::validate.
    //!
    //! Returns true once the region declares the tie, including when it already
    //! did. False means the pair cannot be tied at all: a handle naming no
    //! container this region holds, or one station tied to itself.
    //!
    //! Always a cwEquate, never a re-scope. Making two stations coincide by
    //! sharing a qualified name (the D5 implicit tie) needs a per-station scope
    //! escape that only commit 8's dotted grammar gives, and an attached
    //! centerline's names belong to its file besides — so the mechanism the
    //! plan's suggester picks between has one arm built today.
    Q_INVOKABLE bool tieStations(const cwStationHandle& first,
                                 const cwStationHandle& second);

    //! The project-wide default unit system, persisted with the project. It
    //! seeds the entry unit of new trips but never reinterprets existing ones.
    //! Defaults to Metric; cwProject seeds it from the app-level cwUnitSettings
    //! when creating a new project (a load overrides it via setData()).
    cwUnits::UnitSystem unitSystem() const { return m_unitSystem; }
    void setUnitSystem(cwUnits::UnitSystem system);

    //! The unnamed, never-saved node every cave hangs from. The region owns it
    //! rather than being one, so insert, remove and move have a single owner
    //! type at every depth; path() and the persistence key exclude it.
    cwSurveyNode* rootNode() const { return m_root; }

    //! The root's child nodes, which today are all caves. A node deeper in the
    //! tree is reachable through rootNode()->allNodes() instead.
    bool hasCaves() const;
    Q_INVOKABLE int caveCount() const;
    Q_INVOKABLE cwCave* cave(int index) const;
    QList<cwCave*> caves() const;

    //! Creates a node of \a kind under \a parent (the root when null), named
    //! "<Kind> N" and deduplicated against its siblings, as one undo step.
    //! Returns the new node, or nullptr when \a parent belongs to another region
    //! or \a kind is outside cwSurveyNode::Kind.
    Q_INVOKABLE cwSurveyNode* addNode(cwSurveyNode* parent, cwSurveyNode::Kind kind);

    //! Moves \a node under \a newParent (the root when null) at \a row, as one
    //! undo step that undo puts back.
    //!
    //! \a row is the destination index once the node is off its old place, so a
    //! move within one parent counts rows as if the node were already gone; it
    //! is clamped to the sibling list. The node object and its whole subtree
    //! keep their identities, so the project's per-object save state stays keyed
    //! correctly and one directory move on disk is what the move costs.
    //!
    //! Does nothing when \a node is null, is the root, belongs to another
    //! region, is listed by no parent (an insert is the verb for that), would
    //! land inside its own subtree, or already sits at that place.
    Q_INVOKABLE void moveNode(cwSurveyNode* node, cwSurveyNode* newParent, int row);

    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;

    Q_INVOKABLE void addCave(cwCave* cave = nullptr);
    Q_INVOKABLE void addCaves(QList<cwCave*> cave);
    void insertCave(int index, cwCave* cave);
    Q_INVOKABLE void removeCave(int index);
    void clearCaves();

    Q_INVOKABLE int indexOf(cwCave* cave);

    //! A sanitized cave name derived from proposedName, unique among the root's
    //! children. Mirrors cwSurveyNode::uniqueTripName: setName() silently
    //! rejects a collision or an unsanitized name, so a name taken from an
    //! arbitrary filename has to come through here first.
    Q_INVOKABLE QString uniqueCaveName(const QString& proposedName) const;

    //! The cavern survey label each cave takes, keyed by cave id. Assigned
    //! across the root's whole child set (cwCavernNaming), so adding, removing,
    //! or renaming any cave can move another cave's collision suffix. Cached;
    //! see cwSurveyNode::tripScopeLabels() for the per-node trip half.
    const QHash<QUuid, QString>& caveScopeLabels() const;

    cwProject* parentProject() const;

    void setData(const cwCavingRegionData &data);
    cwCavingRegionData data() const;

signals:
    void nameChanged();
    void unitSystemChanged();

    //! Something defaultFixDatum() reads changed — a layer arrived, left, was
    //! enabled or disabled or renamed its system, or the frame moved. Coarse on
    //! purpose: it reports the inputs moving rather than the answer changing,
    //! which keeps the PROJ-backed resolve on the reader's side of the signal.
    void defaultFixDatumChanged();

    void beginInsertCaves(int begin, int end);
    void insertedCaves(int begin, int end);

    void beginRemoveCaves(int begin, int end);
    void removedCaves(int begin, int end);

    void caveCountChanged();

    //! Some scope label anywhere in this region may have moved — a cave or a
    //! trip was added, removed, or renamed. The one pulse for a consumer that
    //! shows labels across more than one cave (the ties audit, the tie-in
    //! suggester); a consumer watching a single trip binds cwTrip::scopeChanged
    //! instead. A label has no NOTIFY of its own, so anything that caches one
    //! must invalidate on this.
    void scopeLabelsChanged();

    //! The user deleted external-centerline owners somewhere in this region.
    //! Carries their ids: a trip deleted on its own through cwCave::removeTrip
    //! (relayed from the cave), or a cave deleted through removeCave(), which
    //! carries the cave's own id together with every trip it held. An owner is
    //! whatever an external centerline can be attached to, so a consumer
    //! holding per-owner state outside the project (the external-source
    //! breadcrumb store) can forget all of them from this one pulse.
    //!
    //! Silent on every path that takes a cave or a trip off a list while its id
    //! lives on: project close, load replacing the region, clearCaves(), and a
    //! move to another cave or region.
    void ownersDeleted(const QList<QUuid>& ownerIds);

public slots:

protected:
    virtual void setUndoStackForChildren();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwCavingRegion, QString, m_name, QString(), &cwCavingRegion::nameChanged);

    cwSurveyNode* m_root = nullptr;

    cwGeoReference* m_geoReference = nullptr;

    cwLazLayerModel* m_lazLayers = nullptr;

    cwFixStationValidator* m_fixStationValidator = nullptr;

    cwLocalProjectionManager* m_localProjectionManager = nullptr;

    cwEquateModel* m_equates = nullptr;

    cwUnits::UnitSystem m_unitSystem = cwUnits::Metric;

    // cwCavingRegion& copy(const cwCavingRegion& object);

};

typedef QSharedPointer<cwCavingRegion> cwCavingRegionPtr;

#endif // CWCAVINGREGION_H
