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
#include <QSharedPointer>
#include <QQmlEngine>
#include <QObjectBindableProperty>

//Our includes
class cwCave;
class cwProject;
#include "cwCavingRegionData.h"
#include "cwFixStationValidator.h"
#include "cwGeoReference.h"
#include "cwLocalProjectionManager.h"
#include "cwLazLayerModel.h"
#include "cwSanitizedNameSet.h"
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

    //! The project-wide default unit system, persisted with the project. It
    //! seeds the entry unit of new trips but never reinterprets existing ones.
    //! Defaults to Metric; cwProject seeds it from the app-level cwUnitSettings
    //! when creating a new project (a load overrides it via setData()).
    cwUnits::UnitSystem unitSystem() const { return m_unitSystem; }
    void setUnitSystem(cwUnits::UnitSystem system);

    bool hasCaves() const;
    Q_INVOKABLE int caveCount() const;
    Q_INVOKABLE cwCave* cave(int index) const;
    QList<cwCave*> caves() const;

    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;

    Q_INVOKABLE void addCave(cwCave* cave = nullptr);
    Q_INVOKABLE void addCaves(QList<cwCave*> cave);
    void insertCave(int index, cwCave* cave);
    Q_INVOKABLE void removeCave(int index);
    void removeCaves(int beginIndex, int endIndex);
    void clearCaves();

    int indexOf(cwCave* cave);

    cwSanitizedNameSet& caveNameSet() { return m_caveNames; }
    const cwSanitizedNameSet& caveNameSet() const { return m_caveNames; }

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

public slots:

protected:
    QList<cwCave*> m_caves;

    virtual void setUndoStackForChildren();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwCavingRegion, QString, m_name, QString(), &cwCavingRegion::nameChanged);

    cwSanitizedNameSet m_caveNames;

    cwGeoReference* m_geoReference = nullptr;

    cwLazLayerModel* m_lazLayers = nullptr;

    cwFixStationValidator* m_fixStationValidator = nullptr;

    cwLocalProjectionManager* m_localProjectionManager = nullptr;

    cwUnits::UnitSystem m_unitSystem = cwUnits::Metric;

    // cwCavingRegion& copy(const cwCavingRegion& object);

    void unparentCave(cwCave* cave);
    void addCaveHelper();

    ////////////////////// Undo Redo commands ///////////////////////////////////
    class InsertRemoveCave : public QUndoCommand {
    public:
        InsertRemoveCave(cwCavingRegion* region, int beginIndex, int endIndex);
        ~InsertRemoveCave();

    protected:
        void insertCaves();
        void removeCaves();

        QList< QPointer<cwCave> > Caves;
    private:
        cwCavingRegion* Region;
        int BeginIndex;
        int EndIndex;
        bool OwnsCaves; //!< If the undo command own the caves, ie, it'll delete them
    };

    class InsertCaveCommand : public InsertRemoveCave {

    public:
        InsertCaveCommand(cwCavingRegion* parentRegion, cwCave* cave, int index);
        InsertCaveCommand(cwCavingRegion* parentRegion, QList<cwCave*> cave, int index);
        virtual void redo();
        virtual void undo();
    };

    class RemoveCaveCommand : public InsertRemoveCave {
    public:
        RemoveCaveCommand(cwCavingRegion* region, int beginIndex, int endIndex);
        virtual void redo();
        virtual void undo();
    };
};

typedef QSharedPointer<cwCavingRegion> cwCavingRegionPtr;

/**
  \brief Get's the number of caves in the region
  */
inline int cwCavingRegion::caveCount() const {
    return m_caves.count();
}

/**
  \brief Returns true if the region has at least on cave, otherwise false
  */
inline bool cwCavingRegion::hasCaves() const {
    return !m_caves.isEmpty();
}

/**
  \brief Get's a cave at index
  */
inline cwCave* cwCavingRegion::cave(int index) const {
    if(index < 0 || index >= m_caves.size()) { return nullptr; }
    return m_caves[index];
}

/**
  \brief Gets all the caves in the region
  */
inline QList<cwCave*> cwCavingRegion::caves() const {
    return m_caves;
}

#endif // CWCAVINGREGION_H
