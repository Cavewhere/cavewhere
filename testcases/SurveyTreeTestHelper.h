/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#pragma once

//Catch2 includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwEntranceModel.h"
#include "cwExternalCenterline.h"
#include "cwFixStation.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwShot.h"
#include "cwSolvedEntrance.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNetwork.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"

//Qt includes
#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QStringList>

//! The tree-building, on-disk and model-signal checks the survey-tree tests share.
namespace SurveyTreeTestHelper {

constexpr double kShotDistance = 10.0;
constexpr double kShotCompass = 45.0;
constexpr double kShotClino = -5.0;

//! Gives \a trip one shot, so it holds real survey data on disk.
inline void addShot(cwTrip* trip, const QString& fromStation, const QString& toStation)
{
    trip->addNewChunk();
    cwSurveyChunk* chunk = trip->chunk(trip->chunkCount() - 1);
    chunk->setData(cwSurveyChunk::StationNameRole, 0, fromStation);
    chunk->setData(cwSurveyChunk::StationNameRole, 1, toStation);
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, kShotDistance);
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, kShotCompass);
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, kShotClino);
}

//! Adds a node of \a kind named \a name under \a parent, the root when null.
inline cwCave* addNode(cwCavingRegion* region,
                       cwSurveyNode* parent,
                       cwSurveyNode::Kind kind,
                       const QString& name)
{
    auto node = qobject_cast<cwCave*>(region->addNode(parent, kind));
    REQUIRE(node != nullptr);
    node->setName(name);
    return node;
}

//! Adds a trip named \a name to \a node, carrying one shot between the
//! stations \a station names: "<station>1" to "<station>2".
inline cwTrip* addTrip(cwCave* node, const QString& name, const QString& station)
{
    node->addTrip();
    cwTrip* trip = node->trip(node->tripCount() - 1);
    REQUIRE(trip != nullptr);
    trip->setName(name);
    addShot(trip, station + QStringLiteral("1"), station + QStringLiteral("2"));
    return trip;
}

inline cwFixStation fixAt(const QString& stationName)
{
    cwFixStation fix;
    fix.setStationName(stationName);
    return fix;
}

inline cwEntrance entranceOn(const QString& stationName, const QString& label = QString())
{
    cwEntrance entrance;
    entrance.setStationName(stationName);
    entrance.setLabel(label);
    return entrance;
}

inline QStringList entranceNames(const cwEntranceModel* model)
{
    QStringList names;
    for (const cwEntrance& entrance : model->entrances()) {
        names.append(entrance.stationName());
    }
    return names;
}

//! \a count bare fixes a file lists, named <prefix>1, <prefix>2, ….
inline QList<cwAttachedFix> attachedFixes(const QString& prefix, int count)
{
    QList<cwAttachedFix> fixes;
    for(int i = 1; i <= count; ++i) {
        fixes.append({prefix + QString::number(i), QStringLiteral("0 0 0"), QString(),
                      QStringLiteral("attached.svx")});
    }
    return fixes;
}

//! Waits until every queued save has reached disk.
inline void flushSaves(cwRootData* rootData)
{
    rootData->project()->waitSaveToFinish();
    rootData->futureManagerModel()->waitForFinished();
    rootData->project()->waitSaveToFinish();
}

//! Saves the project as \a baseName.cwproj under \a parentDir and answers the
//! file it was written to.
inline QString saveProjectAs(cwRootData* rootData, const QDir& parentDir, const QString& baseName)
{
    auto project = rootData->project();
    const QString projectPath = parentDir.absoluteFilePath(baseName + QStringLiteral(".cwproj"));
    REQUIRE(project->saveAs(projectPath));
    flushSaves(rootData);
    return project->filename();
}

//! Every file below \a dir, as paths relative to it, git's own files left out —
//! they are not part of the project's layout.
inline QStringList relativeFiles(const QDir& dir)
{
    QStringList files;
    QDirIterator it(dir.absolutePath(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString relative = dir.relativeFilePath(it.next());
        if (relative.startsWith(QStringLiteral(".git"))) {
            continue;
        }
        files.append(relative);
    }
    files.sort();
    return files;
}

inline QByteArray readFile(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

//! Stands in for a solve of \a node's subtree: writes \a positions, named as
//! the node names its stations, into the region's lookup in place of whatever
//! the subtree held there. The node must hang in a region.
inline void setScopedPositions(cwSurveyNode* node, const cwStationPositionLookup& positions)
{
    cwCavingRegion* region = node->parentRegion();
    REQUIRE(region != nullptr);
    const QString prefix = cwStation::canonicalKey(node->scopePrefix());
    REQUIRE((node->isRoot() || !prefix.isEmpty()));

    cwStationPositionLookup merged;
    const QMap<QString, QVector3D> existing = region->stationPositions().positions();
    for (auto it = existing.constBegin(); it != existing.constEnd(); ++it) {
        if (!it.key().startsWith(prefix)) {
            merged.setPosition(it.key(), it.value());
        }
    }
    const QMap<QString, QVector3D> scoped = positions.positions();
    for (auto it = scoped.constBegin(); it != scoped.constEnd(); ++it) {
        merged.setPosition(prefix + it.key(), it.value());
    }
    region->setStationPositions(merged);
}

//! The same, for the region's network.
inline void setScopedNetwork(cwSurveyNode* node, const cwSurveyNetwork& network)
{
    cwCavingRegion* region = node->parentRegion();
    REQUIRE(region != nullptr);
    const QString prefix = cwStation::canonicalKey(node->scopePrefix());
    REQUIRE((node->isRoot() || !prefix.isEmpty()));

    cwSurveyNetwork merged;
    const cwSurveyNetwork existing = region->network();
    for (const QString& station : existing.stations()) {
        if (station.startsWith(prefix)) {
            continue;
        }
        for (const QString& neighbor : existing.neighbors(station)) {
            if (!neighbor.startsWith(prefix)) {
                merged.addShot(station, neighbor);
            }
        }
        if (existing.hasPosition(station)) {
            merged.setPosition(station, existing.position(station));
        }
    }
    for (const QString& station : network.stations()) {
        for (const QString& neighbor : network.neighbors(station)) {
            merged.addShot(prefix + station, prefix + neighbor);
        }
        if (network.hasPosition(station)) {
            merged.setPosition(prefix + station, network.position(station));
        }
    }
    region->setNetwork(merged);
}

//! Stands in for a solve that flags \a entrances on stations \a node owns:
//! replaces the node's solved entrances in the region and keeps every other
//! node's. The node must hang in a region.
inline void setSolvedEntrances(cwSurveyNode* node, const QList<cwSolvedEntrance>& entrances)
{
    cwCavingRegion* region = node->parentRegion();
    REQUIRE(region != nullptr);
    QHash<QUuid, QList<cwSolvedEntrance>> merged = region->solvedEntrances();
    if (entrances.isEmpty()) {
        merged.remove(node->id());
    } else {
        merged.insert(node->id(), entrances);
    }
    region->setSolvedEntrances(merged);
}

//! Installs a network on \a node that names \a stations, so fixes on them
//! pass the reference check and a test can isolate the other fix checks.
inline void surveyStations(cwSurveyNode* node, const QStringList& stations)
{
    const QString hub = QStringLiteral("hub");
    cwSurveyNetwork network;
    for (const QString& station : stations) {
        network.addShot(hub, station);
    }
    setScopedNetwork(node, network);
}

//! Adds a trip to \a node whose one chunk chains through \a stations, as a
//! survey looks before its first solve.
inline cwTrip* addTripWithStations(cwSurveyNode* node, const QStringList& stations)
{
    auto* trip = new cwTrip();
    node->addTrip(trip);
    auto* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    cwStation from;
    from.setName(QStringLiteral("hub"));
    for (const QString& name : stations) {
        cwStation to;
        to.setName(name);
        chunk->appendShot(from, to, cwShot());
        from = to;
    }
    return trip;
}

//! The dataChanged emissions in \a spy whose rows cover \a rowIndex.
inline int changesCovering(const QSignalSpy& spy, const QModelIndex& rowIndex)
{
    int count = 0;
    for (const QList<QVariant>& arguments : spy) {
        const QModelIndex topLeft = arguments.at(0).value<QModelIndex>();
        const QModelIndex bottomRight = arguments.at(1).value<QModelIndex>();
        if (topLeft.parent() == rowIndex.parent()
            && topLeft.row() <= rowIndex.row()
            && bottomRight.row() >= rowIndex.row()) {
            ++count;
        }
    }
    return count;
}

}
