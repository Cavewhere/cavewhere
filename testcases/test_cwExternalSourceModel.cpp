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

const QString kAlpha2 = QStringLiteral("ALPHA.DAT/ALPHA2");
const QString kBravo = QStringLiteral("BRAVO.DAT");
const QString kCharlie = QStringLiteral("CHARLIE.DAT");

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

//! The exclusions the project file at \a path holds for its first source.
QStringList excludedPathsOnDisk(const QString& path)
{
    const QJsonArray sources = readJson(path).value(QStringLiteral("externalSources")).toArray();
    REQUIRE(sources.size() == 1);
    QStringList paths;
    const QJsonArray excluded = sources.at(0).toObject().value(QStringLiteral("excludedSourcePaths")).toArray();
    for (const QJsonValue& value : excluded) {
        paths.append(value.toString());
    }
    return paths;
}

//! Copies \a source to \a destination; the fixture carries no git repository.
void copyDirectory(const QDir& source, const QDir& destination)
{
    REQUIRE(QDir().mkpath(destination.absolutePath()));
    const QFileInfoList entries = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        const QString destinationPath = destination.absoluteFilePath(entry.fileName());
        if (entry.isDir()) {
            copyDirectory(QDir(entry.absoluteFilePath()), QDir(destinationPath));
        } else {
            REQUIRE(QFile::copy(entry.absoluteFilePath(), destinationPath));
        }
    }
}

cwTrip* tripNamed(const cwCave* node, const QString& name)
{
    for (int i = 0; i < node->tripCount(); i++) {
        if (node->trip(i)->name() == name) {
            return node->trip(i);
        }
    }
    return nullptr;
}

std::unique_ptr<cwRootData> loadProject(const QString& projectFile)
{
    auto rootData = std::make_unique<cwRootData>();
    addTokenManager(rootData->project());
    rootData->project()->loadOrConvert(projectFile);
    rootData->project()->waitLoadToFinish();
    return rootData;
}

//! Hides \a path on the region's one source, as a hide verb would.
void hide(cwCavingRegion* region, const QString& path)
{
    cwExternalSourceModel* sources = region->externalSources();
    REQUIRE(sources->count() == 1);
    cwExternalSource source = sources->sourceAt(0);
    source.setExcludedSourcePaths(source.excludedSourcePaths() + QStringList{path});
    sources->replaceAt(0, source);
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

    const cwExternalSource first(QUuid::createUuid(), QUuid::createUuid(), {kBravo});
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
    CHECK(model.data(firstIndex, cwExternalSourceModel::ExcludedSourcePathsRole).toStringList()
          == QStringList({kBravo}));
    CHECK(model.data(firstIndex, cwExternalSourceModel::SourceRole).value<cwExternalSource>() == first);

    CHECK(model.indexOf(second.id()) == 1);
    CHECK(model.indexOf(QUuid::createUuid()) == -1);
    CHECK(model.sourceAt(2) == cwExternalSource());

    cwExternalSource hidden = second;
    hidden.setExcludedSourcePaths({kAlpha2});
    model.replaceAt(1, hidden);
    CHECK(dataSpy.count() == 1);
    CHECK(model.sourceAt(1).excludedSourcePaths() == QStringList({kAlpha2}));

    //Replacing a row with what it already holds changes nothing.
    model.replaceAt(1, hidden);
    CHECK(dataSpy.count() == 1);

    model.removeAt(0);
    CHECK(model.count() == 1);
    CHECK(countSpy.count() == 3);
    CHECK(model.sourceAt(0) == hidden);
}

TEST_CASE("The caving region carries its external sources through its data", "[ExternalSource]")
{
    const cwExternalSource source(QUuid::createUuid(), QUuid::createUuid(), {kAlpha2, kBravo});

    cwCavingRegion region;
    region.externalSources()->appendSource(source);
    const cwCavingRegionData data = region.data();
    CHECK(data.externalSources == QList<cwExternalSource>({source}));

    cwCavingRegion copy;
    copy.setData(data);
    CHECK(copy.externalSources()->sources() == QList<cwExternalSource>({source}));
}

TEST_CASE("mergeExternalSources merges sources by id and exclusions three ways", "[ExternalSource]")
{
    const QUuid id = QUuid::createUuid();
    const QUuid rootId = QUuid::createUuid();
    const cwExternalSource base(id, rootId, {kCharlie});

    SECTION("Each side hides a different item, and both stay hidden")
    {
        const auto merged = cwCavingRegionMergeApplier::mergeExternalSources(
            {cwExternalSource(id, rootId, {kCharlie, kAlpha2})},
            {cwExternalSource(id, rootId, {kCharlie, kBravo})},
            QList<cwExternalSource>({base}));
        CHECK(merged == QList<cwExternalSource>({cwExternalSource(id, rootId, {kCharlie, kBravo, kAlpha2})}));
    }

    SECTION("An item one side restored comes back")
    {
        const auto merged = cwCavingRegionMergeApplier::mergeExternalSources(
            {cwExternalSource(id, rootId, {kCharlie})},
            {cwExternalSource(id, rootId, {})},
            QList<cwExternalSource>({base}));
        CHECK(merged == QList<cwExternalSource>({cwExternalSource(id, rootId, {})}));
    }

    SECTION("Without a base, every source and exclusion of both sides is kept")
    {
        const cwExternalSource ourOther(QUuid::createUuid(), QUuid::createUuid());
        const cwExternalSource theirOther(QUuid::createUuid(), QUuid::createUuid());
        const auto merged = cwCavingRegionMergeApplier::mergeExternalSources(
            {cwExternalSource(id, rootId, {kAlpha2}), ourOther},
            {theirOther, cwExternalSource(id, rootId, {kBravo})},
            std::nullopt);
        CHECK(merged == QList<cwExternalSource>({theirOther,
                                                 cwExternalSource(id, rootId, {kBravo, kAlpha2}),
                                                 ourOther}));
    }

    SECTION("A source one side deleted stays deleted, and a source one side added stays")
    {
        const cwExternalSource added(QUuid::createUuid(), QUuid::createUuid(), {kBravo});
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
    const cwExternalSource source(QUuid::createUuid(), cave->id(), {kAlpha2});
    region->externalSources()->appendSource(source);
    flushSaves(rootData.get());
    CHECK(fileVersionOf(projectFile) == cwRegionIOTask::protoVersion());
    CHECK(fileVersionOf(caveFile) == cwRegionIOTask::protoVersion());
    CHECK(excludedPathsOnDisk(projectFile) == QStringList({kAlpha2}));

    hide(region, kBravo);
    flushSaves(rootData.get());
    CHECK(excludedPathsOnDisk(projectFile) == QStringList({kAlpha2, kBravo}));

    auto loaded = loadProject(projectFile);
    CHECK(loaded->project()->errorModel()->isEmpty());
    CHECK(loaded->project()->cavingRegion()->externalSources()->sources()
          == QList<cwExternalSource>({cwExternalSource(source.id(), cave->id(), {kAlpha2, kBravo})}));
}

TEST_CASE("The sourced_excluded fixture loads with its exclusions", "[ExternalSource]")
{
    const QString fixturePath = testcasesDatasetSourcePath(QStringLiteral("survey-tree/sourced_excluded"));
    REQUIRE(QDir(fixturePath).exists());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir workingRoot(QDir(tempDir.path()).absoluteFilePath(QStringLiteral("sourced_excluded")));
    copyDirectory(QDir(fixturePath), workingRoot);

    auto rootData = loadProject(workingRoot.absoluteFilePath(QStringLiteral("sourced_excluded.cwproj")));
    CHECK(rootData->project()->errorModel()->isEmpty());
    auto* region = rootData->project()->cavingRegion();

    cwCave* folder = childNamed(region->rootNode(), QStringLiteral("Field seasons"));
    REQUIRE(folder != nullptr);
    CHECK(folder->kind() == cwSurveyNode::Kind::Folder);
    CHECK_FALSE(folder->isSourced());

    cwCave* root = childNamed(folder, QStringLiteral("PROJECT"));
    REQUIRE(root != nullptr);
    CHECK(root->kind() == cwSurveyNode::Kind::CompassProject);
    CHECK(root->isSourceRoot());
    CHECK(root->isReadOnly());
    CHECK(root->externalCenterline().entryFile() == QStringLiteral("PROJECT.MAK"));
    CHECK(root->childNodes().size() == 2);
    CHECK(root->tripCount() == 0);

    cwCave* alpha = childNamed(root, QStringLiteral("Alpha"));
    REQUIRE(alpha != nullptr);
    CHECK(alpha->kind() == cwSurveyNode::Kind::CompassFile);
    CHECK(alpha->sourceId() == root->sourceId());
    CHECK(alpha->sourcePath() == QStringLiteral("ALPHA.DAT"));
    CHECK(alpha->sourceRoot() == root);
    REQUIRE(alpha->tripCount() == 2);
    cwTrip* alpha1 = tripNamed(alpha, QStringLiteral("ALPHA1"));
    REQUIRE(alpha1 != nullptr);
    CHECK(alpha1->sourcePath() == QStringLiteral("ALPHA.DAT/ALPHA1"));
    cwTrip* nativeTrip = tripNamed(alpha, QStringLiteral("Alpha resurvey"));
    REQUIRE(nativeTrip != nullptr);
    CHECK(nativeTrip->sourcePath().isEmpty());

    cwCave* charlie = childNamed(root, QStringLiteral("Charlie"));
    REQUIRE(charlie != nullptr);
    CHECK(charlie->sourcePath() == kCharlie);
    REQUIRE(charlie->tripCount() == 1);
    CHECK(charlie->trip(0)->sourcePath() == QStringLiteral("CHARLIE.DAT/CHARLIE1"));

    //The excluded unit and node have no records; the source row names them.
    CHECK(childNamed(root, QStringLiteral("Bravo")) == nullptr);
    CHECK(tripNamed(alpha, QStringLiteral("ALPHA2")) == nullptr);
    REQUIRE(region->externalSources()->count() == 1);
    const cwExternalSource source = region->externalSources()->sourceAt(0);
    CHECK(source.id() == root->sourceId());
    CHECK(source.rootNodeId() == root->id());
    CHECK(source.excludedSourcePaths() == QStringList({kAlpha2, kBravo}));
}

TEST_CASE("Two clones that each hide a different item keep both after a sync",
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

    hide(peerRegion, kBravo);
    pushPeer(clones.get());

    hide(authorRegion, kAlpha2);
    syncAuthor(clones.get());

    //The peer pushed first, so its exclusion leads and the author's follows.
    const QList<cwExternalSource> expected({cwExternalSource(source.id(), sideCave->id(), {kBravo, kAlpha2})});
    CHECK(authorRegion->externalSources()->sources() == expected);
    CHECK(excludedPathsOnDisk(clones->authorProject()->filename()) == QStringList({kBravo, kAlpha2}));

    //The merged exclusions were pushed, so the peer pulls both.
    syncProject(clones->peerRootData.get());
    CHECK(peerRegion->externalSources()->sources() == expected);
    CHECK(excludedPathsOnDisk(clones->peerProject()->filename()) == QStringList({kBravo, kAlpha2}));
}
