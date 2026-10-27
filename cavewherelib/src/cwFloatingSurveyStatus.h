/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFLOATINGSURVEYSTATUS_H
#define CWFLOATINGSURVEYSTATUS_H

//Our includes
#include "cwGlobals.h"
#include "cwStationHandle.h"
class cwFloatingSurveyModel;
class cwTrip;

//Qt includes
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

/**
 * Whether one trip is tied into its cave, as properties a view can bind.
 *
 * Set trip and model, then read floating, stations and stationHandles. The
 * answers re-derive themselves whenever the model's rows move, so a view binds
 * them once and never asks again.
 *
 * The *trip* is what floats, not its stations. A trip floats when nothing joins
 * its survey to the rest of its cave — no shared station name, no equate — so
 * the solver had to place it in a frame of its own. stations is then the whole
 * trip's stations, so a reader can see which part of the cave is adrift; it is
 * not a subset that floats while the rest of the trip does not. The two are
 * independent, and that is the point of having both: a trip nothing fixes and
 * nothing ties in is dropped by cavern outright, so it floats with an empty
 * station list. Deriving "is it floating" from "are there stations" goes silent
 * for exactly that case.
 *
 * cwFloatingSurveyModel holds the same answer for a whole region, as rows keyed
 * by uuid. That is the wrong shape for the banner, the editor's cell marker and
 * the suggester, each of which wants one trip — so this is the per-trip view of
 * it, the same way cwScopeStationListModel is the per-trip view of a cave's
 * solved stations.
 *
 * stations and stationHandles name the same stations in the same order:
 * stations is what a label shows, stationHandles is what a tie stores.
 */
class CAVEWHERE_LIB_EXPORT cwFloatingSurveyStatus : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FloatingSurveyStatus)

    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged FINAL)
    Q_PROPERTY(cwFloatingSurveyModel* model READ model WRITE setModel NOTIFY modelChanged FINAL)
    Q_PROPERTY(bool floating READ floating NOTIFY statusChanged FINAL)
    Q_PROPERTY(QStringList stations READ stations NOTIFY statusChanged FINAL)
    Q_PROPERTY(QList<cwStationHandle> stationHandles READ stationHandles NOTIFY statusChanged FINAL)

public:
    explicit cwFloatingSurveyStatus(QObject* parent = nullptr);

    cwTrip* trip() const { return m_trip; }
    void setTrip(cwTrip* trip);

    cwFloatingSurveyModel* model() const { return m_model; }
    void setModel(cwFloatingSurveyModel* model);

    //! True when the last run found nothing joining this trip to its cave,
    //! whichever of the two passes found it. Never derivable from stations: a
    //! survey nothing fixes and nothing ties in is dropped outright, so it
    //! floats with no station to name.
    bool floating() const { return m_floating; }

    //! This trip's stations, in the trip's own namespace, for showing. Empty
    //! when the trip does not float, and also when its stations could not be
    //! learned at all — cavern placed none of them and the scan-time harvest of
    //! its file failed too.
    QStringList stations() const { return m_stations; }

    //! The same stations as identities, for tying.
    QList<cwStationHandle> stationHandles() const { return m_stationHandles; }

signals:
    void tripChanged();
    void modelChanged();
    void statusChanged();

private:
    QPointer<cwTrip> m_trip;
    QPointer<cwFloatingSurveyModel> m_model;
    bool m_floating = false;
    QStringList m_stations;
    QList<cwStationHandle> m_stationHandles;

    void setModelConnected(cwFloatingSurveyModel* model, bool connected);
    void refresh();
};

#endif // CWFLOATINGSURVEYSTATUS_H
