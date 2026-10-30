/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavernRunner.h"
#include "cwCavingRegion.h"
#include "cwCavingRegionData.h"
#include "cwExternalCenterlineAttach.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceModel.h"
#include "cwExternalSourceSettings.h"
#include "cwExternalUnitManifest.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwScopeLabels.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTemporaryDir>

// Std
#include <memory>

namespace {

const QString kCompassProject = QStringLiteral("compass_project");
const QString kProjectMak = QStringLiteral("PROJECT.MAK");

QString compassProjectMak()
{
    return fixturePath(kCompassProject + QStringLiteral("/") + kProjectMak);
}

std::unique_ptr<SavedProjectFixture> makeProject(const QString& projectFileBase)
{
    return makeSavedProject(projectFileBase,
                            QStringLiteral("NativeCave"),
                            QStringLiteral("NativeTrip"));
}

cwSurveyNode* regionRoot(SavedProjectFixture* fixture)
{
    return fixture->project->cavingRegion()->rootNode();
}

QString attachmentDirOf(SavedProjectFixture* fixture, const cwSurveyNode* root)
{
    return fixture->saveLoad()->externalCenterlineDir(qobject_cast<const cwCave*>(root)).absolutePath();
}

cwExternalSource sourceOf(SavedProjectFixture* fixture, const cwSurveyNode* root)
{
    const cwExternalSourceModel* sources = fixture->project->cavingRegion()->externalSources();
    const int row = sources->indexOf(root->sourceId());
    REQUIRE(row >= 0);
    return sources->sourceAt(row);
}

//! Copies compass_project into \a dir and returns the copy's .mak.
QString copyCompassProject(const QTemporaryDir& dir)
{
    const QDir fixtureDir(fixturePath(kCompassProject));
    for (const QString& name : fixtureDir.entryList(QDir::Files)) {
        REQUIRE(QFile::copy(fixtureDir.absoluteFilePath(name),
                            QDir(dir.path()).absoluteFilePath(name)));
    }
    return QDir(dir.path()).absoluteFilePath(kProjectMak);
}

void reload(SavedProjectFixture* fixture, cwSurveyNode* root)
{
    auto future = managerOf(fixture)->reloadFromSource(root);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("reload: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture);
}

void replace(SavedProjectFixture* fixture, cwSurveyNode* root, const QString& source)
{
    auto future = managerOf(fixture)->replaceSource(root, source);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("replace: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture);
}

} // namespace

TEST_CASE("Attaching compass_project builds a project root, one node per DAT, one trip per survey",
          "[Attach][ExternalSource]")
{
    auto fixture = makeProject(QStringLiteral("subtree-compass"));
    cwSurveyNode* root = attachSourceThroughManager(fixture.get(), regionRoot(fixture.get()),
                                                    compassProjectMak());
    drainPipelines(fixture.get());

    CHECK(root->parentNode() == regionRoot(fixture.get()));
    CHECK(root->name() == QStringLiteral("PROJECT"));
    CHECK(root->kind() == cwSurveyNode::Kind::CompassProject);
    CHECK(root->isSourceRoot());
    CHECK(root->isReadOnly());
    CHECK(root->externalCenterline().entryFile() == kProjectMak);
    CHECK(root->tripCount() == 0);

    // The absent DELTA.DAT is listed by the .mak and makes no node.
    CHECK(childNodeNames(root) == QStringList({QStringLiteral("Alpha"), QStringLiteral("Bravo"),
                                           QStringLiteral("Charlie")}));
    for (const cwSurveyNode* node : root->childNodes()) {
        INFO("node " << node->name().toStdString());
        CHECK(node->kind() == cwSurveyNode::Kind::CompassFile);
        CHECK(node->sourceId() == root->sourceId());
        CHECK(node->isReadOnly());
        CHECK(node->sourceRoot() == root);
    }

    const cwSurveyNode* alpha = childNodeNamed(root, QStringLiteral("Alpha"));
    CHECK(alpha->sourcePath() == QStringLiteral("ALPHA.DAT"));
    CHECK(tripNames(alpha) == QStringList({QStringLiteral("ALPHA1"), QStringLiteral("ALPHA2")}));
    const cwTrip* alpha1 = tripNamed(alpha, QStringLiteral("ALPHA1"));
    CHECK(alpha1->sourcePath() == QStringLiteral("ALPHA.DAT/ALPHA1"));
    CHECK(alpha1->date().date() == QDate(2020, 3, 14));
    CHECK(tripNamed(alpha, QStringLiteral("ALPHA2"))->date().date() == QDate(2020, 3, 15));
    CHECK(tripNames(childNodeNamed(root, QStringLiteral("Bravo"))) == QStringList({QStringLiteral("BRAVO1")}));
    CHECK(tripNamed(childNodeNamed(root, QStringLiteral("Charlie")), QStringLiteral("CHARLIE1"))->date().date()
          == QDate(2022, 5, 9));

    // The project records the source, and units.json sits beside the copy.
    const cwExternalSource source = sourceOf(fixture.get(), root);
    CHECK(source.rootNodeId() == root->id());

    const QString attachmentDir = attachmentDirOf(fixture.get(), root);
    CHECK(QFileInfo::exists(QDir(attachmentDir).absoluteFilePath(kProjectMak)));
    const auto manifest = cwExternalUnitManifest::read(
        QDir(attachmentDir).absoluteFilePath(cwExternalUnitManifest::fileName()));
    REQUIRE_FALSE(manifest.hasError());
    CHECK(manifest.value().size() == 9);

    CHECK(fixture->settings()->breadcrumbPath(root->id())
          == QFileInfo(compassProjectMak()).absoluteFilePath());
}

TEST_CASE("A survey file attaches under a Folder", "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("subtree-under-folder"));
    cwCave* folder = addChildNode(regionRoot(fixture.get()), QStringLiteral("Field seasons"),
                                  cwSurveyNode::Kind::Folder);
    fixture->project->waitSaveToFinish();

    cwSurveyNode* root = attachSourceThroughManager(
        fixture.get(), folder, compassProjectMak());
    drainPipelines(fixture.get());

    CHECK(root->parentNode() == folder);
    CHECK(childNodeNames(folder) == QStringList({QStringLiteral("PROJECT")}));
    CHECK(root->childNodeCount() == 3);
    CHECK(QFileInfo::exists(QDir(attachmentDirOf(fixture.get(), root)).absoluteFilePath(kProjectMak)));
}

TEST_CASE("Reload keeps a native trip under a derived node and adds a node for a new DAT",
          "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("subtree-reload-new-dat"));
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString mak = copyCompassProject(sourceDir);

    cwSurveyNode* root = attachSourceThroughManager(fixture.get(), regionRoot(fixture.get()), mak);
    drainPipelines(fixture.get());

    cwSurveyNode* alpha = childNodeNamed(root, QStringLiteral("Alpha"));
    const QUuid alphaId = alpha->id();
    const QUuid alpha1Id = tripNamed(alpha, QStringLiteral("ALPHA1"))->id();
    cwTrip* resurvey = addEmptyTrip(qobject_cast<cwCave*>(alpha), QStringLiteral("Alpha resurvey"));
    REQUIRE(resurvey->sourcePath().isEmpty());

    // ECHO.DAT joins CHARLIE at its C3.
    overwriteFile(QDir(sourceDir.path()).absoluteFilePath(QStringLiteral("ECHO.DAT")),
                  "Echo Cave\n"
                  "SURVEY NAME: ECHO1\n"
                  "SURVEY DATE: 6 1 2023\n"
                  "SURVEY TEAM: \n"
                  "Tester\n"
                  "DECLINATION:    0.00  FORMAT: DDDDLRUDLAaDdCT  CORRECTIONS:  0.00 0.00 0.00\n"
                  "\n"
                  "                FROM                   TO   LENGTH  BEARING      INC     LEFT       UP     DOWN    RIGHT\n"
                  "\n"
                  "                  C3                   E1    10.00     0.00     0.00     1.00     1.00     1.00     1.00\n"
                  "\f\n");
    overwriteFile(mak, fileContents(mak) + "#ECHO.DAT,C3;\n");

    reload(fixture.get(), root);

    CHECK(childNodeNamed(root, QStringLiteral("Alpha"))->id() == alphaId);
    CHECK(tripNamed(alpha, QStringLiteral("ALPHA1"))->id() == alpha1Id);
    CHECK(tripNamed(alpha, QStringLiteral("Alpha resurvey")) == resurvey);

    cwSurveyNode* echo = childNodeNamed(root, QStringLiteral("Echo"));
    REQUIRE(echo != nullptr);
    CHECK(echo->sourcePath() == QStringLiteral("ECHO.DAT"));
    CHECK(tripNames(echo) == QStringList({QStringLiteral("ECHO1")}));
    CHECK(tripNamed(echo, QStringLiteral("ECHO1"))->date().date() == QDate(2023, 6, 1));
    CHECK(root->childNodeCount() == 4);
}

TEST_CASE("A derived trip holding a note survives Reload and Replace",
          "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("subtree-note-survives"));
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString mak = copyCompassProject(sourceDir);

    cwSurveyNode* root = attachSourceThroughManager(fixture.get(), regionRoot(fixture.get()), mak);
    drainPipelines(fixture.get());

    cwSurveyNode* alpha = childNodeNamed(root, QStringLiteral("Alpha"));
    cwTrip* alpha1 = tripNamed(alpha, QStringLiteral("ALPHA1"));
    const QPointer<cwTrip> alpha1Ptr(alpha1);
    addNoteWithScrap(alpha1, QStringLiteral("A1"));

    reload(fixture.get(), root);
    REQUIRE_FALSE(alpha1Ptr.isNull());
    CHECK(alpha1->notes()->hasNotes());

    // Replace with a project that no longer lists ALPHA.DAT: ALPHA2 goes,
    // ALPHA1 holds a note and stays, and so does Alpha, now native.
    QTemporaryDir replacementDir;
    REQUIRE(replacementDir.isValid());
    const QString replacementMak = copyCompassProject(replacementDir);
    QString makText = QString::fromUtf8(fileContents(replacementMak));
    const qsizetype alphaLine = makText.indexOf(QStringLiteral("#ALPHA.DAT"));
    REQUIRE(alphaLine >= 0);
    makText.remove(alphaLine, makText.indexOf(QLatin1Char('\n'), alphaLine) - alphaLine + 1);
    overwriteFile(replacementMak, makText.toUtf8());

    replace(fixture.get(), root, replacementMak);
    REQUIRE_FALSE(alpha1Ptr.isNull());
    CHECK(alpha1->notes()->hasNotes());
    CHECK(alpha1->parentNode() == alpha);
    CHECK(tripNames(alpha) == QStringList({QStringLiteral("ALPHA1")}));
    CHECK_FALSE(alpha->isSourced());
    CHECK_FALSE(alpha->isReadOnly());
}

TEST_CASE("Deleting a source root takes its subtree, native trips, copies, and source record",
          "[Attach][ExternalSource]")
{
    auto fixture = makeProject(QStringLiteral("subtree-delete-root"));
    cwSurveyNode* root = attachSourceThroughManager(
        fixture.get(), regionRoot(fixture.get()),
        compassProjectMak());
    drainPipelines(fixture.get());

    const QPointer<cwTrip> resurvey(
        addEmptyTrip(qobject_cast<cwCave*>(childNodeNamed(root, QStringLiteral("Alpha"))),
                     QStringLiteral("Alpha resurvey")));
    fixture->project->waitSaveToFinish();
    const QString attachmentDir = attachmentDirOf(fixture.get(), root);
    REQUIRE(QDir(attachmentDir).exists());
    const QUuid rootId = root->id();

    auto future = managerOf(fixture.get())->deleteSourceRoot(root);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("delete: " << future.result().errorMessage().toStdString());
    CHECK_FALSE(future.result().hasError());
    drainPipelines(fixture.get());

    CHECK(childNodeNamed(regionRoot(fixture.get()), QStringLiteral("PROJECT")) == nullptr);
    for (const cwTrip* trip : regionRoot(fixture.get())->allTrips()) {
        CHECK(trip->name() != QStringLiteral("Alpha resurvey"));
    }
    CHECK_FALSE(QDir(attachmentDir).exists());
    CHECK(fixture->project->cavingRegion()->externalSources()->count() == 0);
    CHECK_FALSE(fixture->settings()->hasBreadcrumb(rootId));
    CHECK(regionRoot(fixture.get())->childNodes().contains(fixture->cave));
}

namespace {

//! Adds \a paths as the first source's excludedSourcePaths in the project
//! file, as a build that still had the derived hide saved them.
void writeStaleExclusions(const QString& projectFile, const QStringList& paths)
{
    QJsonObject project = QJsonDocument::fromJson(fileContents(projectFile)).object();
    QJsonArray sources = project.value(QStringLiteral("externalSources")).toArray();
    REQUIRE(sources.size() == 1);
    QJsonObject source = sources.at(0).toObject();
    source.insert(QStringLiteral("excludedSourcePaths"), QJsonArray::fromStringList(paths));
    sources.replace(0, source);
    project.insert(QStringLiteral("externalSources"), sources);
    overwriteFile(projectFile, QJsonDocument(project).toJson());
}

} // namespace

TEST_CASE("A project saved with exclusions loads and reloads with every derived row",
          "[Attach][SaveLoad]")
{
    auto fixture = makeProject(QStringLiteral("subtree-stale-exclusions"));
    cwSurveyNode* root = attachSourceThroughManager(fixture.get(), regionRoot(fixture.get()),
                                                    compassProjectMak());
    drainPipelines(fixture.get());

    // The tree an earlier build's hide left on disk: Bravo and ALPHA2 are
    // gone, and the source row names them.
    cwSurveyNode* alpha = childNodeNamed(root, QStringLiteral("Alpha"));
    alpha->removeTrip(alpha->indexOf(tripNamed(alpha, QStringLiteral("ALPHA2"))));
    root->removeNode(root->indexOfNode(childNodeNamed(root, QStringLiteral("Bravo"))));
    drainPipelines(fixture.get());

    const QUuid rootId = root->id();
    const QUuid sourceId = root->sourceId();
    const QString projectFile = fixture->project->filename();
    fixture->rootData.reset();
    fixture->cave = nullptr;
    fixture->trip = nullptr;
    writeStaleExclusions(projectFile, {QStringLiteral("BRAVO.DAT"), QStringLiteral("ALPHA.DAT/ALPHA2")});

    fixture->rootData = std::make_unique<cwRootData>();
    fixture->project = fixture->rootData->project();
    addTokenManager(fixture->project);
    fixture->project->loadOrConvert(projectFile);
    fixture->project->waitLoadToFinish();
    drainPipelines(fixture.get());

    CHECK(fixture->project->errorModel()->isEmpty());
    const cwExternalSourceModel* sources = fixture->project->cavingRegion()->externalSources();
    REQUIRE(sources->count() == 1);
    CHECK(sources->sourceAt(0) == cwExternalSource(sourceId, rootId));

    cwSurveyNode* loadedRoot = childNodeNamed(regionRoot(fixture.get()), QStringLiteral("PROJECT"));
    REQUIRE(loadedRoot != nullptr);
    REQUIRE(loadedRoot->isSourceRoot());
    CHECK(childNodeNames(loadedRoot) == QStringList({QStringLiteral("Alpha"), QStringLiteral("Charlie")}));
    cwSurveyNode* loadedAlpha = childNodeNamed(loadedRoot, QStringLiteral("Alpha"));
    REQUIRE(loadedAlpha != nullptr);
    CHECK(tripNames(loadedAlpha) == QStringList({QStringLiteral("ALPHA1")}));

    const QString source = compassProjectMak();
    fixture->settings()->setBreadcrumb(
        rootId, source, cwExternalSourceRecord::computeFingerprint({source}));
    REQUIRE(managerOf(fixture.get())->canReloadFromSource(loadedRoot));
    reload(fixture.get(), loadedRoot);

    // Reload builds every row the file lists; nothing it once hid is skipped.
    CHECK(loadedRoot->childNodeCount() == 3);
    CHECK(tripNames(loadedAlpha) == QStringList({QStringLiteral("ALPHA1"), QStringLiteral("ALPHA2")}));
    const cwSurveyNode* bravo = childNodeNamed(loadedRoot, QStringLiteral("Bravo"));
    REQUIRE(bravo != nullptr);
    CHECK(tripNames(bravo) == QStringList({QStringLiteral("BRAVO1")}));
}

TEST_CASE("Attaching walls_project nests its books and keeps the gray ones",
          "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("subtree-walls"));
    cwSurveyNode* root = attachSourceThroughManager(
        fixture.get(), regionRoot(fixture.get()), fixturePath(QStringLiteral("walls_project/KAUA.WPJ")));
    drainPipelines(fixture.get());

    CHECK(root->name() == QStringLiteral("KAUA"));
    CHECK(root->kind() == cwSurveyNode::Kind::WallsBook);
    CHECK(childNodeNames(root) == QStringList({QStringLiteral("Drawings"), QStringLiteral("Fixed Points"),
                                           QStringLiteral("Surveys")}));
    // The books with no survey data are nodes all the same, shown gray.
    CHECK(childNodeNamed(root, QStringLiteral("Drawings"))->childNodeCount() == 0);
    CHECK(childNodeNamed(root, QStringLiteral("Drawings"))->tripCount() == 0);

    const cwSurveyNode* surveys = childNodeNamed(root, QStringLiteral("Surveys"));
    CHECK(surveys->sourcePath() == QStringLiteral("SURVEYS"));
    const cwSurveyNode* dec2001 = childNodeNamed(surveys, QStringLiteral("Dec 2001 Surveys"));
    REQUIRE(dec2001 != nullptr);
    CHECK(dec2001->kind() == cwSurveyNode::Kind::WallsBook);
    CHECK(tripNames(dec2001) == QStringList({QStringLiteral("D Survey 1"), QStringLiteral("D Survey 2"),
                                             QStringLiteral("N Surveys")}));
    CHECK(tripNamed(dec2001, QStringLiteral("N Surveys"))->sourcePath()
          == QStringLiteral("SURVEYS/DEC2001/N01"));
    CHECK(tripNamed(dec2001, QStringLiteral("D Survey 1"))->date().date() == QDate(2001, 12, 1));
}

TEST_CASE("Attaching survex_master builds a file root with one block trip per cave",
          "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("subtree-survex-master"));
    cwSurveyNode* root = attachSourceThroughManager(
        fixture.get(), regionRoot(fixture.get()), fixturePath(QStringLiteral("survex_master/master.svx")));
    drainPipelines(fixture.get());

    CHECK(root->name() == QStringLiteral("master"));
    CHECK(root->kind() == cwSurveyNode::Kind::SurvexFile);
    CHECK(root->childNodeCount() == 0);
    // Each block holds shots and no nested block, so it is one trip whose
    // path is the block's.
    CHECK(tripNames(root) == QStringList({QStringLiteral("alpha"), QStringLiteral("bravo"),
                                          QStringLiteral("charlie")}));
    for (const cwTrip* trip : root->trips()) {
        INFO("block " << trip->name().toStdString());
        CHECK(trip->sourcePath() == trip->name());
    }
}

TEST_CASE("A lone Compass .dat attaches as one node holding its surveys",
          "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("subtree-lone-dat"));
    cwSurveyNode* root = attachSourceThroughManager(
        fixture.get(), regionRoot(fixture.get()), fixturePath(QStringLiteral("compass_simple.dat")));
    drainPipelines(fixture.get());

    CHECK(root->name() == QStringLiteral("compass_simple"));
    CHECK(root->kind() == cwSurveyNode::Kind::CompassFile);
    CHECK(root->isSourceRoot());
    CHECK(root->childNodeCount() == 0);
    REQUIRE(root->tripCount() == 1);
    CHECK(root->trip(0)->name() == QStringLiteral("A"));
    CHECK(root->trip(0)->sourcePath() == QStringLiteral("A"));
    CHECK(root->trip(0)->date().date() == QDate(2025, 1, 1));
}

TEST_CASE("Reloading an outer source root keeps a nested source root's derived trips",
          "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("subtree-nested-root"));
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString mak = copyCompassProject(sourceDir);

    cwSurveyNode* outer = attachSourceThroughManager(fixture.get(), regionRoot(fixture.get()), mak);
    drainPipelines(fixture.get());
    cwSurveyNode* alpha = childNodeNamed(outer, QStringLiteral("Alpha"));
    REQUIRE(alpha != nullptr);

    cwSurveyNode* inner = attachSourceThroughManager(
        fixture.get(), alpha, fixturePath(QStringLiteral("compass_simple.dat")));
    drainPipelines(fixture.get());
    REQUIRE(inner->parentNode() == alpha);
    REQUIRE(inner->tripCount() == 1);
    const QUuid innerTripId = inner->trip(0)->id();

    reload(fixture.get(), outer);

    REQUIRE(childNodeNamed(outer, QStringLiteral("Alpha")) == alpha);
    REQUIRE(childNodeNamed(alpha, inner->name()) == inner);
    REQUIRE(inner->tripCount() == 1);
    CHECK(inner->trip(0)->id() == innerTripId);
    CHECK(inner->trip(0)->sourcePath() == QStringLiteral("A"));
    CHECK(tripNames(alpha) == QStringList({QStringLiteral("ALPHA1"), QStringLiteral("ALPHA2")}));
}

TEST_CASE("A source root whose harvest fails keeps cavern's warnings off its row",
          "[Attach][HarvestErrorRow]")
{
    // A row error sits under the row on the Cavern page; a few lines is
    // all the page has room for beside the cavern log itself.
    constexpr int kMaxRowErrorLines = 5;

    auto fixture = makeProject(QStringLiteral("subtree-noisy-failure"));
    cwSurveyNode* root = attachSourceThroughManager(
        fixture.get(), regionRoot(fixture.get()),
        fixturePath(QStringLiteral("survex_noisy_failure/noisy_failure.svx")));
    drainPipelines(fixture.get());

    const cwAttachedCenterlinesModel* model = managerOf(fixture.get())->attachedCenterlinesModel();
    REQUIRE(tryWait(kAttachWaitMs, [&] { return !model->errorFor(root->id()).isEmpty(); }));

    const QString rowError = model->errorFor(root->id());
    INFO("row error:\n" << rowError.toStdString());
    CHECK_FALSE(rowError.contains(QStringLiteral("warning:")));
    CHECK(rowError.split(QLatin1Char('\n'), Qt::SkipEmptyParts).size() <= kMaxRowErrorLines);
}

TEST_CASE("firstErrorLine picks cavern's error out of a noisy log",
          "[Attach][HarvestErrorRow]")
{
    const QString log = QStringLiteral(
        "Survex 1.4.11\n"
        "\n"
        "cave/entry.svx:3:13: warning: Suspicious compass reading\n"
        " S2 S3 5.0 400 0\n"
        "            ^~~\n"
        "  cave/entry.svx:75: error: Couldn't open file \"not_on_disk.svx\"\n"
        " *include \"not_on_disk.svx\"\n"
        "error: Second error\n");

    CHECK(cwCavernRunner::firstErrorLine(log)
          == QStringLiteral("cave/entry.svx:75: error: Couldn't open file \"not_on_disk.svx\""));
    CHECK(cwCavernRunner::firstErrorLine(QStringLiteral("x\nerror: bare\n"))
          == QStringLiteral("error: bare"));
    CHECK(cwCavernRunner::firstErrorLine(QStringLiteral("\n  Export failed  \nmore\n"))
          == QStringLiteral("Export failed"));
    CHECK(cwCavernRunner::firstErrorLine(QString()).isEmpty());
}

namespace {

const QString kBlockRowsDir = QStringLiteral("survex_block_rows/");

//! \a node's subtree, pre-order, one line per row: "node <name> [<sourcePath>]"
//! or "trip <name> [<sourcePath>]", indented two spaces per level.
QStringList describeRows(const cwSurveyNode* node, int depth = 0)
{
    const QString indent(depth * 2, QLatin1Char(' '));
    QStringList lines({indent + QStringLiteral("node %1 [%2]").arg(node->name(), node->sourcePath())});
    for (const cwTrip* trip : node->trips()) {
        lines.append(indent + QStringLiteral("  trip %1 [%2]").arg(trip->name(), trip->sourcePath()));
    }
    for (const cwSurveyNode* child : node->childNodes()) {
        lines.append(describeRows(child, depth + 1));
    }
    return lines;
}

//! The names \a trip's solved stations go by, sorted.
QStringList solvedNames(const cwTrip* trip)
{
    QStringList names;
    for (const auto& station : trip->solvedStations()) {
        names.append(station.first);
    }
    names.sort();
    return names;
}

cwSurveyNode* attachBlockRows(SavedProjectFixture* fixture, const QString& entry)
{
    cwSurveyNode* root = attachSourceThroughManager(fixture, regionRoot(fixture),
                                                    fixturePath(kBlockRowsDir + entry));
    drainPipelines(fixture);
    return root;
}

//! The derived nodes under \a root that list \a station among their attached fixes.
QStringList nodesListingFix(const cwSurveyNode* root, const QString& station)
{
    QStringList names;
    for (const cwSurveyNode* node : nodeAndDescendants(root)) {
        if (attachedFixStations(node).contains(station)) {
            names.append(node->name());
        }
    }
    return names;
}

} // namespace

TEST_CASE("A leaf Survex block attaches as one trip, its stations named in the block",
          "[Attach][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("block-rows-leaf"));
    cwSurveyNode* root = attachBlockRows(fixture.get(), QStringLiteral("leaf.svx"));

    CHECK(describeRows(root) == QStringList({
        QStringLiteral("node leaf []"),
        QStringLiteral("  trip simmons_hall [simmons_hall]"),
    }));
    // The fix is the trip's, so no node lists it.
    CHECK(nodesListingFix(root, QStringLiteral("simmons_hall.st1")).isEmpty());

    const cwTrip* trip = tripNamed(root, QStringLiteral("simmons_hall"));
    REQUIRE(trip != nullptr);
    CHECK(solvedNames(trip)
          == QStringList({QStringLiteral("st1"), QStringLiteral("st2"), QStringLiteral("st3")}));
}

TEST_CASE("A Survex block with its own shots and a nested block attaches as a node with two trips",
          "[Attach][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("block-rows-mixed"));
    cwSurveyNode* root = attachBlockRows(fixture.get(), QStringLiteral("mixed.svx"));

    CHECK(describeRows(root) == QStringList({
        QStringLiteral("node mixed []"),
        QStringLiteral("  node cave [cave]"),
        QStringLiteral("    trip cave [cave/#own]"),
        QStringLiteral("    trip inner [cave/inner]"),
    }));

    cwSurveyNode* cave = childNodeNamed(root, QStringLiteral("cave"));
    REQUIRE(cave != nullptr);
    // cave's own fix stays on its node; inner's is inner's trip's.
    CHECK(attachedFixStations(cave) == QStringList({QStringLiteral("cave.c1")}));

    const cwTrip* inner = tripNamed(cave, QStringLiteral("inner"));
    REQUIRE(inner != nullptr);
    CHECK(solvedNames(inner) == QStringList({QStringLiteral("i1"), QStringLiteral("i2")}));

    // A native trip named like the block takes another scope label, live and
    // in the driver's pool alike.
    auto* caveNode = qobject_cast<cwCave*>(cave);
    REQUIRE(caveNode != nullptr);
    cwTrip* native = addEmptyTrip(caveNode, QStringLiteral("inner"));
    const QString liveLabel = tripScopeLabel(native);
    CHECK(liveLabel.compare(QStringLiteral("inner"), Qt::CaseInsensitive) != 0);
    const cwScopeLabels labels(fixture->project->cavingRegion()->data());
    CHECK(labels.tripLabels(cave->id()).value(native->id()) == liveLabel);
}

TEST_CASE("One Survex block name under two parent scopes attaches as two trips",
          "[Attach][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("block-rows-two-scopes"));
    cwSurveyNode* root = attachBlockRows(fixture.get(), QStringLiteral("twoscopes.svx"));

    CHECK(describeRows(root) == QStringList({
        QStringLiteral("node twoscopes []"),
        QStringLiteral("  node east [east]"),
        QStringLiteral("    trip higharch [east/higharch]"),
        QStringLiteral("  node west [west]"),
        QStringLiteral("    trip higharch [west/higharch]"),
    }));
}

TEST_CASE("A Survex block opened twice in one scope attaches as one trip holding every shot",
          "[Attach][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("block-rows-repeat"));
    cwSurveyNode* root = attachBlockRows(fixture.get(), QStringLiteral("repeat.svx"));

    CHECK(describeRows(root) == QStringList({
        QStringLiteral("node repeat []"),
        QStringLiteral("  trip higharch [higharch]"),
    }));
    const cwTrip* trip = tripNamed(root, QStringLiteral("higharch"));
    REQUIRE(trip != nullptr);
    CHECK(solvedNames(trip) == QStringList({QStringLiteral("h1"), QStringLiteral("h2"),
                                            QStringLiteral("h3"), QStringLiteral("h4")}));
}

TEST_CASE("A Survex block holding only an *include attaches as a node over the included block's trip",
          "[Attach][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("block-rows-include-only"));
    cwSurveyNode* root = attachBlockRows(fixture.get(), QStringLiteral("includeonly.svx"));

    CHECK(describeRows(root) == QStringList({
        QStringLiteral("node includeonly []"),
        QStringLiteral("  node higharch [higharch]"),
        QStringLiteral("    trip higharch [higharch/higharch]"),
    }));
    const cwSurveyNode* outer = childNodeNamed(root, QStringLiteral("higharch"));
    REQUIRE(outer != nullptr);
    const cwTrip* trip = tripNamed(outer, QStringLiteral("higharch"));
    REQUIRE(trip != nullptr);
    CHECK(solvedNames(trip) == QStringList({QStringLiteral("x1"), QStringLiteral("x2")}));
}

TEST_CASE("A Survex file included from two scopes attaches a trip in each",
          "[Attach][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("block-rows-shared"));
    cwSurveyNode* root = attachBlockRows(fixture.get(), QStringLiteral("shared_scopes.svx"));

    CHECK(describeRows(root) == QStringList({
        QStringLiteral("node shared_scopes []"),
        QStringLiteral("  trip one [one]"),
        QStringLiteral("  trip two [two]"),
    }));
    for (const QString& name : {QStringLiteral("one"), QStringLiteral("two")}) {
        INFO("trip " << name.toStdString());
        const cwTrip* trip = tripNamed(root, name);
        REQUIRE(trip != nullptr);
        CHECK(solvedNames(trip) == QStringList({QStringLiteral("s1"), QStringLiteral("s2")}));
    }
}
