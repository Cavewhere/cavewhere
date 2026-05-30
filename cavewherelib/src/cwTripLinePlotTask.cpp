/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTripLinePlotTask.h"
#include "cwConcurrent.h"
#include "cwLinePlotTask.h"
#include "cwCavingRegion.h"
#include "cwCavingRegionData.h"
#include "cwCave.h"
#include "cwCaveData.h"
#include "cwStation.h"
#include "cwTrip.h"
#include "cwTripData.h"
#include "cwSurveyChunk.h"

//Qt includes
#include <QAtomicInt>
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QUuid>
#include <algorithm>

namespace {

QList<QList<int>> splitIntoComponents(const QList<cwSurveyChunkData> &chunks)
{
    QList<QList<int>> components;
    if (chunks.isEmpty()) {
        return components;
    }

    // station (lowercased) → chunk indices that touch it
    QHash<QString, QList<int>> stationToChunks;
    for (int i = 0; i < chunks.size(); ++i) {
        QSet<QString> seen;
        for (const auto &station : chunks.at(i).stations) {
            const QString key = cwStation::canonicalKey(station.name());
            if (key.isEmpty() || seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            stationToChunks[key].append(i);
        }
    }

    QVector<bool> visited(chunks.size(), false);
    for (int seed = 0; seed < chunks.size(); ++seed) {
        if (visited[seed]) {
            continue;
        }
        QList<int> component;
        QQueue<int> queue;
        queue.enqueue(seed);
        visited[seed] = true;
        while (!queue.isEmpty()) {
            const int idx = queue.dequeue();
            component.append(idx);
            for (const auto &station : chunks.at(idx).stations) {
                const QString key = cwStation::canonicalKey(station.name());
                if (key.isEmpty()) {
                    continue;
                }
                const auto it = stationToChunks.constFind(key);
                if (it == stationToChunks.constEnd()) {
                    continue;
                }
                for (int neighbour : it.value()) {
                    if (!visited[neighbour]) {
                        visited[neighbour] = true;
                        queue.enqueue(neighbour);
                    }
                }
            }
        }
        std::sort(component.begin(), component.end()); // preserve input order
        components.append(std::move(component));
    }
    return components;
}

struct LoopBreakResult {
    QList<cwSurveyChunkData> chunks;          // renamed copies
    QHash<QString, QString>  internalToPublic; // cavern-safe → user-facing
    QHash<QString, QString>  publicToOriginal; // user-facing → original
    QString                  canonicalAnchor;  // first station of first chunk
};

// cavern / survex only accept [A-Za-z0-9_-] in station names. Our
// user-facing "name#N" labels have a reserved '#', so we pass cavern an
// internal form "name__DUP_N" and translate back when unpacking positions.
// Private (leading underscore prefix) keeps the separator unlikely to
// collide with any user-chosen name.
QString internalDupName(const QString &base, int n)
{
    return QString("%1__DUP_%2").arg(base).arg(n);
}

QString publicDupName(const QString &base, int n)
{
    return QString("%1#%2").arg(base).arg(n);
}

// Per-component rename: walks chunks in input order. Stations already seen
// elsewhere in this component become anchor links unless a chunk brings ≥2
// duplicates — in which case all but the first-indexed duplicate are
// renamed to "Name#N" to prevent cavern from closing a loop. Counter is
// per-name and never resets across chunks, so a station colliding in
// multiple subsequent chunks yields "#1", "#2", … in discovery order.
LoopBreakResult renameDuplicates(const QList<cwSurveyChunkData> &allChunks,
                                 const QList<int> &componentChunkIndices)
{
    LoopBreakResult out;
    if (componentChunkIndices.isEmpty()) {
        return out;
    }

    out.chunks.reserve(componentChunkIndices.size());
    QHash<QString, int> seenCount;      // canonical name → placement count (within component)
    QHash<QString, int> renameCounter;  // canonical name → next "#N" suffix

    for (int chunkIdx : componentChunkIndices) {
        cwSurveyChunkData chunk = allChunks.at(chunkIdx);

        // Identify duplicate station indices in this chunk (stations whose
        // name is already in seenCount).
        QList<int> duplicateIndices;
        for (int i = 0; i < chunk.stations.size(); ++i) {
            const QString key = cwStation::canonicalKey(chunk.stations.at(i).name());
            if (key.isEmpty()) {
                continue;
            }
            if (seenCount.contains(key)) {
                duplicateIndices.append(i);
            }
        }

        if (duplicateIndices.size() >= 2) {
            // Keep first-indexed duplicate as anchor; rename the rest.
            for (int j = 1; j < duplicateIndices.size(); ++j) {
                const int idx = duplicateIndices.at(j);
                cwStation station = chunk.stations.at(idx);
                const QString originalName = station.name();
                const QString key = cwStation::canonicalKey(originalName);
                const int n = ++renameCounter[key];
                const QString internalName = internalDupName(originalName, n);
                const QString publicName   = publicDupName(originalName, n);
                station.setName(internalName);
                chunk.stations[idx] = station;
                out.internalToPublic.insert(cwStation::canonicalKey(internalName), publicName);
                out.publicToOriginal.insert(publicName, originalName);
                seenCount[cwStation::canonicalKey(internalName)] = 1;
            }
        }

        // Record canonical anchor from first chunk's first station.
        if (out.canonicalAnchor.isEmpty() && !chunk.stations.isEmpty()) {
            out.canonicalAnchor = chunk.stations.first().name();
        }

        // Multiple chunks meeting at a shared station do NOT trigger renames
        // on their own — only ≥2 duplicates in a single chunk do.
        for (const auto &station : chunk.stations) {
            const QString key = cwStation::canonicalKey(station.name());
            if (!key.isEmpty()) {
                seenCount[key]++;
            }
        }

        out.chunks.append(chunk);
    }

    return out;
}

cwLinePlotTask::Input buildLinePlotInput(const QString &tripName,
                                         const cwTripCalibrationData &calibration,
                                         const QList<cwSurveyChunkData> &chunks)
{
    cwTripCalibrationData stripped = calibration;
    // see module header — declination is applied downstream at scrap/3D time.
    stripped.setDeclinationManual(0.0);
    stripped.setAutoDeclination(false);

    cwTripData tripData;
    tripData.name = tripName;
    tripData.calibrations = stripped;
    tripData.chunks = chunks;

    cwCaveData caveData;
    caveData.name = QStringLiteral("sketch-trip"); // worker will re-encode
    // cwLinePlotTask hashes per-cave bookkeeping by cwCaveData::id (the
    // same UUID surfaces in the cavern station prefix). Synthetic input
    // built here doesn't have a stable cave identity, but the id needs to
    // be non-null so encodeCaveNames and the regionPointers cave (built
    // from the same `caveData` below) end up on the same UUID. Generate
    // once here and let the throwaway region adopt it via setData.
    caveData.id = QUuid::createUuid();
    caveData.trips.append(tripData);

    cwLinePlotTask::Input input;
    input.regionData.name = QStringLiteral("sketch-region");
    input.regionData.caves.append(caveData);

    // cwLinePlotTask's RegionDataPtrs needs a real cwCavingRegion so the
    // internal book-keeping indices line up. Construct a throwaway on the
    // worker thread; its lifetime ends with this call, and cwLinePlotTask
    // only dereferences the pointers through its own QMap keys.
    //
    // Live pointer: we can't store it here because RegionDataPtrs copies
    // by value. Instead the caller (buildAndRun below) owns the region for
    // the duration of the cwLinePlotTask run.
    return input;
}

// Runs cwLinePlotTask synchronously on the current (worker) thread and
// returns the first (and only) cave's result. On cavern failure / empty
// positions, returns default-constructed values — caller treats as skip.
cwLinePlotTask::LinePlotCaveData runLinePlot(cwLinePlotTask::Input input)
{
    // Throwaway region keeps RegionDataPtrs' Cave pointer stable for the
    // duration of the run. cwLinePlotTask does not emit signals from the
    // worker so creating QObjects here is safe.
    cwCavingRegion throwaway;
    throwaway.setData(input.regionData);
    input.regionPointers = cwLinePlotTask::RegionDataPtrs(&throwaway);

    auto future = cwLinePlotTask::run(std::move(input));
    future.waitForFinished(); // worker-thread block, not a QEventLoop spin
    if (future.isCanceled() || future.resultCount() == 0) {
        return cwLinePlotTask::LinePlotCaveData();
    }
    const auto result = future.result();
    const auto caves = result.caveData();
    if (caves.isEmpty()) {
        return cwLinePlotTask::LinePlotCaveData();
    }
    return caves.first();
}

} // namespace

namespace cwTripLinePlotTask {

Input buildInput(const cwTrip *trip)
{
    Input input;
    if (trip == nullptr) {
        return input;
    }
    const cwTripData d = trip->data();
    input.tripName    = d.name;
    input.calibration = d.calibrations;
    input.chunks      = d.chunks;
    return input;
}

QFuture<QList<TripComponent>> run(Input pipelineInput)
{
    return cwConcurrent::run([input = std::move(pipelineInput)]() {
        QList<TripComponent> results;
        if (input.chunks.isEmpty()) {
            return results;
        }

        const auto componentIndexLists = splitIntoComponents(input.chunks);

        for (const auto &componentIndices : componentIndexLists) {
            if (componentIndices.isEmpty()) {
                continue;
            }
            auto broken = renameDuplicates(input.chunks, componentIndices);
            auto lpInput = buildLinePlotInput(input.tripName,
                                              input.calibration,
                                              broken.chunks);
            auto caveResult = runLinePlot(std::move(lpInput));
            if (caveResult.stationPositions().isEmpty()) {
                // cavern refused this component (unparseable shots, etc.).
                // Skip — per the plan, we tolerate per-component failure
                // rather than failing the whole trip.
                continue;
            }

            TripComponent tc;
            tc.network   = caveResult.network();
            tc.positions = caveResult.stationPositions();

            if (!broken.internalToPublic.isEmpty()) {
                // Rename internal "foo__DUP_N" keys back to the user-facing
                // "foo#N" form in both network and position lookup.
                cwStationPositionLookup remapped;
                const auto srcMap = tc.positions.positions();
                for (auto it = srcMap.constBegin(); it != srcMap.constEnd(); ++it) {
                    QString key = it.key();
                    const auto mappedIt = broken.internalToPublic.constFind(key);
                    if (mappedIt != broken.internalToPublic.constEnd()) {
                        key = mappedIt.value();
                    }
                    remapped.setPosition(key, it.value());
                }
                tc.positions = std::move(remapped);

                cwSurveyNetwork renamedNetwork;
                const QStringList stations = tc.network.stations();
                for (const QString &from : stations) {
                    QString fromPublic = broken.internalToPublic.value(cwStation::canonicalKey(from), from);
                    for (const QString &to : tc.network.neighbors(from)) {
                        QString toPublic = broken.internalToPublic.value(cwStation::canonicalKey(to), to);
                        renamedNetwork.addShot(fromPublic, toPublic);
                    }
                }
                tc.network = std::move(renamedNetwork);
            }

            tc.renameRemap     = std::move(broken.publicToOriginal);
            tc.canonicalAnchor = std::move(broken.canonicalAnchor);
            results.append(std::move(tc));
        }

        std::sort(results.begin(), results.end(),
                  [](const TripComponent &a, const TripComponent &b) {
                      return a.network.stations().size() > b.network.stations().size();
                  });

        return results;
    });
}

} // namespace cwTripLinePlotTask
