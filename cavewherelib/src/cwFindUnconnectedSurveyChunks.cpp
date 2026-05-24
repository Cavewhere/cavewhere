/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFindUnconnectedSurveyChunks.h"
#include "cwCave.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

#include <QHash>
#include <QMultiHash>
#include <QSet>

#include <memory>

namespace {

QSet<QString> chunkToStationNameSet(const cwSurveyChunk* chunk)
{
    QSet<QString> stationNameSet;
    for (int i = 0; i < chunk->stationCount(); i++) {
        QString name = cwStation::canonicalKey(
            chunk->data(cwSurveyChunk::StationNameRole, i).toString());
        if (!name.isEmpty()) {
            stationNameSet.insert(name);
        }
    }
    return stationNameSet;
}

bool isChunksConnect(const cwSurveyChunk* chunk1,
                     const cwSurveyChunk* chunk2,
                     const QHash<const cwSurveyChunk*, QSet<QString>>& chunkToStationSet)
{
    if (chunk1 == chunk2) {
        return false;
    }
    Q_ASSERT(chunkToStationSet.contains(chunk1) && chunkToStationSet.contains(chunk2));

    const QSet<QString>& set1 = chunkToStationSet.value(chunk1);
    const QSet<QString>& set2 = chunkToStationSet.value(chunk2);

    for (auto iter = set1.begin(); iter != set1.end(); iter++) {
        if (set2.contains(*iter)) {
            return true;
        }
    }
    return false;
}

void insertChunkToConnected(const cwSurveyChunk* chunk,
                            QSet<const cwSurveyChunk*>& connected,
                            const QMultiHash<const cwSurveyChunk*, const cwSurveyChunk*>& chunkConnectedTo)
{
    if (!connected.contains(chunk)) {
        connected.insert(chunk);

        const auto chunksConnectedToChunk = chunkConnectedTo.values(chunk);
        for (const cwSurveyChunk* currentChunk : chunksConnectedToChunk) {
            insertChunkToConnected(currentChunk, connected, chunkConnectedTo);
        }
    }
}

} // namespace

Monad::Result<QList<cwFindUnconnectedSurveyChunks::Result>>
cwFindUnconnectedSurveyChunks::find(const cwCaveData& caveData)
{
    auto cave = std::make_unique<cwCave>();
    cave->setData(caveData);

    QSet<const cwSurveyChunk*> connected;
    QMultiHash<const cwSurveyChunk*, const cwSurveyChunk*> chunkConnectedTo;
    QHash<const cwSurveyChunk*, QSet<QString>> chunkToStationSet;

    // Index every chunk to its station-name set.
    for (cwTrip* trip : cave->trips()) {
        for (cwSurveyChunk* chunk : trip->chunks()) {
            chunkToStationSet.insert(chunk, chunkToStationNameSet(chunk));
        }
    }

    // Build the connected-to graph.
    for (cwTrip* tripA : cave->trips()) {
        for (cwSurveyChunk* chunkA : tripA->chunks()) {
            for (cwTrip* tripB : cave->trips()) {
                for (cwSurveyChunk* chunkB : tripB->chunks()) {
                    if (isChunksConnect(chunkA, chunkB, chunkToStationSet)) {
                        if (!chunkConnectedTo.contains(chunkA, chunkB)) {
                            chunkConnectedTo.insert(chunkA, chunkB);
                            chunkConnectedTo.insert(chunkB, chunkA);
                        }
                    }
                }
            }
        }
    }

    const auto seedConnected = [&]() {
        for (cwTrip* trip : cave->trips()) {
            for (cwSurveyChunk* chunk : trip->chunks()) {
                if (!chunk->isValid()) {
                    continue;
                }
                for (int i = 0; i < chunk->stationCount(); i++) {
                    QString stationName = cwStation::canonicalKey(
                        chunk->data(cwSurveyChunk::StationNameRole, i).toString());
                    if (!stationName.isEmpty()) {
                        insertChunkToConnected(chunk, connected, chunkConnectedTo);
                        return;
                    }
                }
            }
        }
    };
    seedConnected();

    QList<Result> results;

    cwError error;
    error.setMessage(QStringLiteral("Survey leg isn't connect to the cave"));
    error.setType(cwError::Fatal);

    for (int t = 0; t < cave->tripCount(); t++) {
        cwTrip* trip = cave->trip(t);
        for (int c = 0; c < trip->chunkCount(); c++) {
            cwSurveyChunk* chunk = trip->chunk(c);

            if (!connected.contains(chunk) && !chunk->isStationAndShotsEmpty()) {
                results.append(Result(t, c, error));
            }
        }
    }

    return Monad::Result<QList<Result>>(results);
}
