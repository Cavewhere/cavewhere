/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYEXPORTMANGER_H
#define CWSURVEYEXPORTMANGER_H

//Qt includes
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>

//Our includes
class cwCave;
class cwTrip;
class cwCavingRegion;

/**
    This class will open dialogs to export data.

    This class controls the different survey data exporters: survex and compass
  */
class cwSurveyExportManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyExportManager)

    Q_PROPERTY(QString currentCaveName READ currentCaveName NOTIFY updateMenu)
    Q_PROPERTY(QString currentTripName READ currentTripName NOTIFY updateMenu)

    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(cwCave* cave READ cave WRITE setCave NOTIFY caveChanged)
    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)

    // True when nothing in the region has an external-centerline attachment
    // (or a Scope sub-entity once Phase 3 introduces stationPrefix).
    // The Survex and Compass exports refuse to run when this is false because
    // those output formats cannot round-trip the *include topology the
    // attached entry brought in. See master plan §7.5.
    Q_PROPERTY(bool canExport READ canExport NOTIFY canExportChanged FINAL)

    // Human-readable explanation for the disabled state, or empty when
    // canExport is true. Bound by the export menu items as their tooltip.
    Q_PROPERTY(QString exportDisabledReason READ exportDisabledReason NOTIFY canExportChanged FINAL)

public:
    explicit cwSurveyExportManager(QObject *parent = 0);
    ~cwSurveyExportManager();

    QString currentCaveName() const;
    QString currentTripName() const;

    cwCavingRegion* cavingRegion() const;
    void setCavingRegion(cwCavingRegion* cavingRegion);

    cwCave* cave() const;
    void setCave(cwCave* cave);

    cwTrip* trip() const;
    void setTrip(cwTrip* trip);

    bool canExport() const { return m_canExport; }
    QString exportDisabledReason() const { return m_exportDisabledReason; }

signals:
    void updateMenu();
    void tripChanged();
    void caveChanged();
    void cavingRegionChanged();
    void canExportChanged();

public slots:
    void exportSurvexTrip(QString filename);
    void exportSurvexCave(QString filename);
    void exportSurvexRegion(QString filename);
    void exportCaveToCompass(QString filename);
    void exportCaveToChipdata(QString filename);

private slots:
    void exporterFinished();

    void updateActions();

private:
    //The data that will be exported
    QPointer<cwTrip> Trip; //!<
    QPointer<cwCave> Cave; //!<
    QPointer<cwCavingRegion> CavingRegion; //!<

    // Cached gate state. Recomputed by recomputeCanExport() whenever the
    // region's cave/trip set changes or any cave/trip's externalCenterline
    // changes. Defaults to true (pure-Native projects export normally).
    bool m_canExport = true;
    QString m_exportDisabledReason;

    // Every connection wired up by rewireExternalCenterlineTracking() so it
    // can tear them down before re-wiring against a new region or a mutated
    // cave/trip set. Holding the QMetaObject::Connection handles (rather
    // than relying on QObject parent deletion) keeps the design robust to
    // the manager outliving any individual cave/trip in the region.
    QList<QMetaObject::Connection> m_externalCenterlineConnections;

    void updateCaveActions(const QModelIndex& index);
    void updateTripActions(const QModelIndex& index);

    cwCave* currentCave() const;
    cwTrip* currentTrip() const;

    // Tears down and re-establishes every signal connection used to drive
    // recomputeCanExport(): region cave-insert/remove, per-cave
    // trip-insert/remove, and per-cave / per-trip externalCenterlineChanged.
    // Cheap (a handful of caves and trips in normal projects) and run only
    // on structural changes.
    void rewireExternalCenterlineTracking();

    // Walks every cave (and its trips) in the current region; updates
    // m_canExport / m_exportDisabledReason and emits canExportChanged()
    // only when the gate flips. Cheap and idempotent.
    void recomputeCanExport();

};

#endif // CWSURVEYEXPORTMANGER_H
