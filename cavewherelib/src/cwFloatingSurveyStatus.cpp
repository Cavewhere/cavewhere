/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwFloatingSurveyStatus.h"
#include "cwFloatingSurveyModel.h"
#include "cwTrip.h"

//Std includes
#include <utility>

cwFloatingSurveyStatus::cwFloatingSurveyStatus(QObject* parent) :
    QObject(parent)
{
}

void cwFloatingSurveyStatus::setTrip(cwTrip* trip)
{
    if (m_trip == trip) {
        return;
    }

    //Nothing subscribes to the trip: nothing it says about itself changes
    //whether a finished run found it floating, and a trip that goes away takes
    //its row with it, which the model announces.
    m_trip = trip;

    emit tripChanged();
    refresh();
}

void cwFloatingSurveyStatus::setModel(cwFloatingSurveyModel* model)
{
    if (m_model == model) {
        return;
    }

    setModelConnected(m_model, false);
    m_model = model;
    setModelConnected(m_model, true);

    emit modelChanged();
    refresh();
}

void cwFloatingSurveyStatus::setModelConnected(cwFloatingSurveyModel* model, bool connected)
{
    if (model == nullptr) {
        return;
    }

    //Every way the model's content can move, named in one place. The model
    //resets wholesale today, so modelReset alone would do — and that is exactly
    //the trap: a later row diff for the region-wide surfaces would leave a
    //per-trip binding silently frozen. The reordering signals are absent on
    //purpose, since a row changing place cannot change what it says.
    const auto forEachSignal = [model, this](auto apply) {
        apply(model, &QAbstractItemModel::modelReset);
        apply(model, &QAbstractItemModel::dataChanged);
        apply(model, &QAbstractItemModel::rowsInserted);
        apply(model, &QAbstractItemModel::rowsRemoved);
        apply(model, &QObject::destroyed);
    };

    if (connected) {
        forEachSignal([this](auto* sender, auto signal) {
            QObject::connect(sender, signal, this, &cwFloatingSurveyStatus::refresh);
        });
    } else {
        forEachSignal([this](auto* sender, auto signal) {
            QObject::disconnect(sender, signal, this, &cwFloatingSurveyStatus::refresh);
        });
    }
}

void cwFloatingSurveyStatus::refresh()
{
    bool floating = false;
    QStringList stations;
    QList<cwStationHandle> stationHandles;

    if (!m_model.isNull() && !m_trip.isNull()) {
        const QUuid tripId = m_trip->id();
        floating = m_model->isFloating(tripId);
        if (floating) {
            stations = m_model->floatingStations(tripId);
            stationHandles = m_model->floatingStationHandles(tripId);
        }
    }

    if (floating == m_floating
            && stations == m_stations
            && stationHandles == m_stationHandles) {
        return;
    }

    m_floating = floating;
    m_stations = std::move(stations);
    m_stationHandles = std::move(stationHandles);

    emit statusChanged();
}
