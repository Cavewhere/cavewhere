/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef SURVEYEDITORTESTHELPER_H
#define SURVEYEDITORTESTHELPER_H

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwShot.h"
#include "cwShotMeasurement.h"
#include "cwSurveyChunk.h"

//Test includes
#include "SplayFixtureHelper.h"

//Qt includes
#include <QAbstractItemModel>
#include <QObject>
#include <QStringList>

inline cwShot readingShot()
{
    return cwShot(QStringLiteral("10"),
                  QStringLiteral("52"),
                  QStringLiteral("232"),
                  QStringLiteral("-31"),
                  QStringLiteral("31"));
}

inline cwShotMeasurement splayWithDistance(const QString& distance)
{
    return makeSplay(distance, QStringLiteral("90"), QStringLiteral("5"));
}

//An empty name gives an empty station
inline QList<cwStation> namedStations(const QStringList& names)
{
    QList<cwStation> stations;
    for(const auto& name : names) {
        stations.append(name.isEmpty() ? cwStation() : cwStation(name));
    }
    return stations;
}

//A new chunk of the named stations with readings on every shot
inline cwSurveyChunk* chunkWithNames(const QStringList& names)
{
    auto chunk = new cwSurveyChunk();
    QList<cwShot> shots;
    for(int i = 1; i < names.size(); ++i) {
        shots.append(readingShot());
    }
    chunk->setData(cwSurveyChunkData{QUuid::createUuid(), namedStations(names), shots});
    return chunk;
}

inline QStringList stationNames(const cwSurveyChunk* chunk)
{
    QStringList names;
    for(const auto& station : chunk->stations()) {
        names.append(station.name());
    }
    return names;
}

/**
 * Holds a model to the rows it announced: the rows told to the view through
 * beginInsertRows and beginRemoveRows have to add up to the rows the model
 * really gained or lost. A view told the wrong range shows stale rows rather
 * than failing.
 */
class SurveyEditorRowTally
{
public:
    explicit SurveyEditorRowTally(const QAbstractItemModel* model)
        : m_model(model)
    {
        QObject::connect(model, &QAbstractItemModel::rowsInserted, &m_context,
                         [this](const QModelIndex&, int first, int last) {
                             m_announcedRows += last - first + 1;
                         });
        QObject::connect(model, &QAbstractItemModel::rowsRemoved, &m_context,
                         [this](const QModelIndex&, int first, int last) {
                             m_announcedRows -= last - first + 1;
                         });

        //A reset tells the view to throw every row away, so the tally restarts
        QObject::connect(model, &QAbstractItemModel::modelReset, &m_context,
                         [this]() { restart(); });

        restart();
    }

    //! Starts over from the model's current row count
    void restart()
    {
        m_lastCheckedRowCount = m_model->rowCount();
        m_announcedRows = 0;
    }

    void check(int expected)
    {
        CHECK(m_model->rowCount() == expected);
        CHECK(m_lastCheckedRowCount + m_announcedRows == expected);
        restart();
    }

private:
    const QAbstractItemModel* m_model;
    QObject m_context;
    int m_lastCheckedRowCount = 0;
    int m_announcedRows = 0;
};

#endif //SURVEYEDITORTESTHELPER_H
