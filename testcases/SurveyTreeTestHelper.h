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
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"

//Qt includes
#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QString>
#include <QStringList>

//! The tree-building and on-disk checks every survey-tree persistence test shares.
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

}
