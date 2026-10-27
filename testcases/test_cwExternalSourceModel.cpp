/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "LoadProjectHelper.h"
#include "SurveyTreeSyncFixture.h"
#include "SurveyTreeTestHelper.h"
#include "cwCavingRegionMergeApplier.h"
#include "cwErrorListModel.h"
#include "cwExternalSourceModel.h"
#include "cwRegionIOTask.h"

//Qt includes
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>

using namespace SurveyTreeTestHelper;
using SurveyTreeSyncFixture::childNamed;

namespace {

QJsonObject readJson(const QString& path)
{
    const QJsonDocument document = QJsonDocument::fromJson(readFile(path));
    REQUIRE(document.isObject());
    return document.object();
}

int fileVersionOf(const QString& path)
{
    return readJson(path).value(QStringLiteral("fileVersion")).toObject()
            .value(QStringLiteral("version")).toInt(-1);
}

//! The ids of the sources the project file at \a path holds, in file order.
QList<QUuid> sourceIdsOnDisk(const QString& path)
{
    QList<QUuid> ids;
    const QJsonArray sources = readJson(path).value(QStringLiteral("externalSources")).toArray();
    for (const QJsonValue& source : sources) {
        ids.append(QUuid(source.toObject().value(QStringLiteral("id")).toString()));
    }
    return ids;
}

std::unique_ptr<cwRootData> loadProject(const QString& projectFile)
{
    auto rootData = std::make_unique<cwRootData>();
    addTokenManager(rootData->project());
    rootData->project()->loadOrConvert(projectFile);
    rootData->project()->waitLoadToFinish();
    return rootData;
}

void syncProject(cwRootData* rootData)
{
    auto* project = rootData->project();
    project->waitSaveToFinish();
    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);
}

} // namespace

TEST_CASE("cwExternalSourceModel holds one row per source", "[ExternalSource]")
{
    cwExternalSourceModel model;
    QSignalSpy countSpy(&model, &cwExternalSourceModel::countChanged);
    QSignalSpy dataSpy(&model, &cwExternalSourceModel::dataChanged);

    const cwExternalSource first(QUuid::createUuid(), QUuid::createUuid());
    const cwExternalSource second(QUuid::createUuid(), QUuid::createUuid());
    model.appendSource(first);
    model.appendSource(second);

    CHECK(model.rowCount() == 2);
    CHECK(model.count() == 2);
    CHECK(countSpy.count() == 2);
    CHECK(model.sources() == QList<cwExternalSource>({first, second}));

    const QModelIndex firstIndex = model.index(0);
    CHECK(model.data(firstIndex, cwExternalSourceModel::IdRole).toUuid() == first.id());
    CHECK(model.data(firstIndex, cwExternalSourceModel::RootNodeIdRole).toUuid() == first.rootNodeId());
    CHECK(model.data(firstIndex, cwExternalSourceModel::SourceRole).value<cwExternalSource>() == first);

    CHECK(model.indexOf(second.id()) == 1);
    CHECK(model.indexOf(QUuid::createUuid()) == -1);
    CHECK(model.sourceAt(2) == cwExternalSource());

    const cwExternalSource moved(second.id(), QUuid::createUuid());
    model.replaceAt(1, moved);
    CHECK(dataSpy.count() == 1);
    CHECK(model.sourceAt(1).rootNodeId() == moved.rootNodeId());

    //Replacing a row with what it already holds changes nothing.
    model.replaceAt(1, moved);
    CHECK(dataSpy.count() == 1);

    model.removeAt(0);
    CHECK(model.count() == 1);
    CHECK(countSpy.count() == 3);
    CHECK(model.sourceAt(0) == moved);
}

TEST_CASE("The caving region carries its external sources through its data", "[ExternalSource]")
{
    const cwExternalSource source(QUuid::createUuid(), QUuid::createUuid());

    cwCavingRegion region;
    region.externalSources()->appendSource(source);
    const cwCavingRegionData data = region.data();
    CHECK(data.externalSources == QList<cwExternalSource>({source}));

    cwCavingRegion copy;
    copy.setData(data);
    CHECK(copy.externalSources()->sources() == QList<cwExternalSource>({source}));
}

TEST_CASE("mergeExternalSources merges sources by id three ways", "[ExternalSource]")
{
    const cwExternalSource base(QUuid::createUuid(), QUuid::createUuid());

    SECTION("Without a base, every source of both sides is kept, theirs first")
    {
        const cwExternalSource ourOther(QUuid::createUuid(), QUuid::createUuid());
        const cwExternalSource theirOther(QUuid::createUuid(), QUuid::createUuid());
        const auto merged = cwCavingRegionMergeApplier::mergeExternalSources(
            {base, ourOther},
            {theirOther, base},
            std::nullopt);
        CHECK(merged == QList<cwExternalSource>({theirOther, base, ourOther}));
    }

    SECTION("A source one side deleted stays deleted, and a source one side added stays")
    {
        const cwExternalSource added(QUuid::createUuid(), QUuid::createUuid());
        const auto merged = cwCavingRegionMergeApplier::mergeExternalSources(
            {},
            {base, added},
            QList<cwExternalSource>({base}));
        CHECK(merged == QList<cwExternalSource>({added}));
    }
}

TEST_CASE("External sources save, restamp the project, and load", "[ExternalSource]")
{
    auto rootData = std::make_unique<cwRootData>();
    auto* region = rootData->project()->cavingRegion();
    cwCave* cave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, QStringLiteral("Plain cave"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("sources"));
    const QDir dataRoot = rootData->project()->dataRootDir();
    const QString caveFile = dataRoot.absoluteFilePath(QStringLiteral("Plain cave/Plain cave.cwcave"));
    CHECK(fileVersionOf(projectFile) < cwRegionIOTask::protoVersion());

    //A source is what an older build would drop, so the first one restamps
    //every file of the project, not only the project file that holds it.
    const cwExternalSource source(QUuid::createUuid(), cave->id());
    region->externalSources()->appendSource(source);
    flushSaves(rootData.get());
    CHECK(fileVersionOf(projectFile) == cwRegionIOTask::protoVersion());
    CHECK(fileVersionOf(caveFile) == cwRegionIOTask::protoVersion());
    CHECK(sourceIdsOnDisk(projectFile) == QList<QUuid>({source.id()}));

    auto loaded = loadProject(projectFile);
    CHECK(loaded->project()->errorModel()->isEmpty());
    CHECK(loaded->project()->cavingRegion()->externalSources()->sources()
          == QList<cwExternalSource>({source}));
}

TEST_CASE("Two clones that each add a different source keep both after a sync",
          "[ExternalSource][sync]")
{
    using namespace SurveyTreeSyncFixture;

    auto clones = makeTwoClones(true);
    auto* authorRegion = clones->authorProject()->cavingRegion();
    auto* peerRegion = clones->peerProject()->cavingRegion();

    //The source reaches both clones before they diverge, so the merge base holds it.
    cwCave* sideCave = childNamed(folderOf(authorRegion), kSideCaveName);
    REQUIRE(sideCave != nullptr);
    const cwExternalSource source(QUuid::createUuid(), sideCave->id());
    authorRegion->externalSources()->appendSource(source);
    syncProject(clones->authorRootData.get());
    syncProject(clones->peerRootData.get());
    REQUIRE(peerRegion->externalSources()->sources() == QList<cwExternalSource>({source}));

    cwCave* siblingCave = childNamed(folderOf(authorRegion), kSiblingCaveName);
    REQUIRE(siblingCave != nullptr);
    const cwExternalSource peerSource(QUuid::createUuid(), siblingCave->id());
    peerRegion->externalSources()->appendSource(peerSource);
    pushPeer(clones.get());

    const cwExternalSource authorSource(QUuid::createUuid(), folderOf(authorRegion)->id());
    authorRegion->externalSources()->appendSource(authorSource);
    syncAuthor(clones.get());

    //The peer pushed first, so its source leads and the author's follows.
    const QList<cwExternalSource> expected({source, peerSource, authorSource});
    const QList<QUuid> expectedIds({source.id(), peerSource.id(), authorSource.id()});
    CHECK(authorRegion->externalSources()->sources() == expected);
    CHECK(sourceIdsOnDisk(clones->authorProject()->filename()) == expectedIds);

    //The merged sources were pushed, so the peer pulls both.
    syncProject(clones->peerRootData.get());
    CHECK(peerRegion->externalSources()->sources() == expected);
    CHECK(sourceIdsOnDisk(clones->peerProject()->filename()) == expectedIds);
}
