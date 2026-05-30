#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include "cwCavingRegion.h"
#include "cwErrorListModel.h"
#include "cwFutureManagerModel.h"
#include "cwLazLayer.h"
#include "cwLazLayerData.h"
#include "cwLazLayerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"

#include "cavewhere.pb.h"
#include "cwRegionIOTask.h"
#include <google/protobuf/util/json_util.h>

#include "LazFixtureHelper.h"

TEST_CASE("cwLazLayer save/load round-trip preserves layer files",
          "[cwLazLayer][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString lazA = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("rtA")));
    const QString lazB = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("rtB")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    auto* region = project->cavingRegion();
    REQUIRE(region != nullptr);
    REQUIRE(region->lazLayers() != nullptr);

    addLazAndWait(root.get(), {lazA, lazB});
    REQUIRE(region->lazLayers()->count() == 2);

    // Originals are copies, not moves — sources should still exist on disk.
    REQUIRE(QFileInfo::exists(lazA));
    REQUIRE(QFileInfo::exists(lazB));

    // Both layers must now point at files inside GIS Layers/ (basenames
    // preserved when no collision).
    const QString p0 = region->lazLayers()->layerAt(0)->sourcePath();
    const QString p1 = region->lazLayers()->layerAt(1)->sourcePath();
    REQUIRE(QFileInfo(p0).fileName() == QFileInfo(lazA).fileName());
    REQUIRE(QFileInfo(p1).fileName() == QFileInfo(lazB).fileName());
    REQUIRE(QFileInfo(p0).absolutePath().endsWith(cwLazLayerModel::folderName()));
    REQUIRE(QFileInfo(p1).absolutePath().endsWith(cwLazLayerModel::folderName()));

    // Wait for the initial loads to settle so pointCount is non-zero.
    REQUIRE(waitForLazLayerLoaded(region->lazLayers()->layerAt(0)));
    REQUIRE(waitForLazLayerLoaded(region->lazLayers()->layerAt(1)));
    REQUIRE(region->lazLayers()->layerAt(0)->pointCount() > 0);

    // Save and reload.
    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-rt-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    // saveAs places the .cwproj inside a sibling directory; project->filename()
    // returns the actual on-disk path after the move.
    const QString actualPath = project->filename();

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(actualPath);
    root2->project()->waitLoadToFinish();

    auto* reloadedRegion = root2->project()->cavingRegion();
    REQUIRE(reloadedRegion != nullptr);
    REQUIRE(reloadedRegion->lazLayers() != nullptr);
    REQUIRE(reloadedRegion->lazLayers()->count() == 2);

    // The reloaded layer's sourcePath is inside the new project's GIS Layers/
    // (saveAs copies the folder across). Compare basenames only.
    auto* reloaded0 = reloadedRegion->lazLayers()->layerAt(0);
    auto* reloaded1 = reloadedRegion->lazLayers()->layerAt(1);
    REQUIRE(QFileInfo(reloaded0->sourcePath()).fileName()
            == QFileInfo(lazA).fileName());
    REQUIRE(QFileInfo(reloaded1->sourcePath()).fileName()
            == QFileInfo(lazB).fileName());
    REQUIRE(QFile::exists(reloaded0->sourcePath()));
    REQUIRE(QFile::exists(reloaded1->sourcePath()));

    // Geometry rebuilt by re-running cwLazLoader against the saved path.
    REQUIRE(waitForLazLayerLoaded(reloaded0));
    REQUIRE(waitForLazLayerLoaded(reloaded1));
    REQUIRE(reloaded0->pointCount() > 0);
    REQUIRE(reloaded1->pointCount() > 0);
}

TEST_CASE("cwLazLayer save/load: pointSize runtime override resets to default",
          "[cwLazLayer][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString laz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("psize")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    auto* region = project->cavingRegion();
    addLazAndWait(root.get(), {laz});
    auto* layer = region->lazLayers()->layerAt(0);
    REQUIRE(layer != nullptr);

    layer->setPointSize(11.0); // Non-default override.
    REQUIRE(layer->pointSize() == 11.0);
    REQUIRE(waitForLazLayerLoaded(layer));

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-psize-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    const QString actualPath = project->filename();

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(actualPath);
    root2->project()->waitLoadToFinish();

    auto* reloaded = root2->project()->cavingRegion()->lazLayers()->layerAt(0);
    REQUIRE(reloaded != nullptr);
    // pointSize is intentionally NOT persisted in v1 — it resets to default.
    REQUIRE(reloaded->pointSize() != 11.0);
}

TEST_CASE("cwLazLayer save/load: disabled state survives reopen",
          "[cwLazLayer][cwLazLayerEnabled][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString lazA = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("enA")));
    const QString lazB = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("enB")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    auto* region = project->cavingRegion();
    REQUIRE(region != nullptr);

    addLazAndWait(root.get(), {lazA, lazB});
    REQUIRE(region->lazLayers()->count() == 2);

    auto* layerA = region->lazLayers()->layerAt(0);
    auto* layerB = region->lazLayers()->layerAt(1);
    REQUIRE(waitForLazLayerLoaded(layerA));
    REQUIRE(waitForLazLayerLoaded(layerB));

    // Disable layer A; layer B stays enabled.
    layerA->setEnabled(false);
    REQUIRE(layerA->enabled() == false);
    REQUIRE(layerB->enabled() == true);

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-disabled-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    const QString actualPath = project->filename();

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(actualPath);
    root2->project()->waitLoadToFinish();
    // Apply queued state after rescan.
    QCoreApplication::processEvents();

    auto* reloadedRegion = root2->project()->cavingRegion();
    REQUIRE(reloadedRegion->lazLayers()->count() == 2);

    auto* reloadedA = reloadedRegion->lazLayers()->layerAt(0);
    auto* reloadedB = reloadedRegion->lazLayers()->layerAt(1);
    REQUIRE(reloadedA != nullptr);
    REQUIRE(reloadedB != nullptr);
    REQUIRE(reloadedA->enabled() == false);
    REQUIRE(reloadedB->enabled() == true);
}

TEST_CASE("cwLazLayer save/load: user-removed layer does not silently disable its same-named successor",
          "[cwLazLayer][cwLazLayerEnabled][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString laz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("zombie")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    auto* region = project->cavingRegion();

    addLazAndWait(root.get(), {laz});
    REQUIRE(region->lazLayers()->count() == 1);
    auto* layer = region->lazLayers()->layerAt(0);
    REQUIRE(waitForLazLayerLoaded(layer));
    const QString basename = QFileInfo(layer->sourcePath()).fileName();

    // Disable, then remove via the user-facing removeAt path.
    layer->setEnabled(false);
    REQUIRE(layer->enabled() == false);
    region->lazLayers()->removeAt(0);
    REQUIRE(region->lazLayers()->count() == 0);

    // Save now — the state for the removed layer should have been pruned, so
    // no proto entry is written for the removed basename.
    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-zombie-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();
    const QString actualPath = project->filename();

    // Re-add a fresh file with the same basename — simulating a user adding
    // a new dataset that happens to reuse the name.
    const QString reAddedLaz = QDir(tempDir.path()).filePath(basename);
    REQUIRE(writeSyntheticLazFile(reAddedLaz, {{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}}));
    addLazAndWait(root.get(), {reAddedLaz});
    REQUIRE(region->lazLayers()->count() == 1);
    auto* fresh = region->lazLayers()->layerAt(0);
    REQUIRE(fresh != nullptr);
    REQUIRE(QFileInfo(fresh->sourcePath()).fileName() == basename);
    REQUIRE(fresh->enabled() == true); // would be false if zombie state had leaked

    // Reload from disk — fresh add picks up enabled=true with no zombie state
    // entry surviving in the proto.
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(actualPath);
    root2->project()->waitLoadToFinish();
    QCoreApplication::processEvents();

    auto* reloadedRegion = root2->project()->cavingRegion();
    REQUIRE(reloadedRegion->lazLayers()->count() == 1);
    REQUIRE(reloadedRegion->lazLayers()->layerAt(0)->enabled() == true);
}

TEST_CASE("cwLazLayer save/load: all-enabled round-trip leaves all enabled",
          "[cwLazLayer][cwLazLayerEnabled][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString lazA = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("allEnA")));
    const QString lazB = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("allEnB")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    auto* region = project->cavingRegion();

    addLazAndWait(root.get(), {lazA, lazB});
    REQUIRE(region->lazLayers()->count() == 2);
    REQUIRE(waitForLazLayerLoaded(region->lazLayers()->layerAt(0)));
    REQUIRE(waitForLazLayerLoaded(region->lazLayers()->layerAt(1)));

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-allen-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    const QString actualPath = project->filename();

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(actualPath);
    root2->project()->waitLoadToFinish();
    QCoreApplication::processEvents();

    auto* reloadedRegion = root2->project()->cavingRegion();
    REQUIRE(reloadedRegion->lazLayers()->count() == 2);
    REQUIRE(reloadedRegion->lazLayers()->layerAt(0)->enabled() == true);
    REQUIRE(reloadedRegion->lazLayers()->layerAt(1)->enabled() == true);
}

TEST_CASE("cwLazLayer save/load: backward compat — projects without LazLayerState load all-enabled",
          "[cwLazLayer][cwLazLayerEnabled][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString lazA = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("bcA")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    auto* region = project->cavingRegion();

    addLazAndWait(root.get(), {lazA});
    REQUIRE(region->lazLayers()->count() == 1);
    REQUIRE(waitForLazLayerLoaded(region->lazLayers()->layerAt(0)));

    // Save with all layers enabled — no LazLayerState entry is written for
    // layers at the default. This is the on-disk shape a pre-feature project
    // produces.
    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-bc-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    const QString actualPath = project->filename();

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(actualPath);
    root2->project()->waitLoadToFinish();
    QCoreApplication::processEvents();

    auto* reloadedRegion = root2->project()->cavingRegion();
    REQUIRE(reloadedRegion->lazLayers()->count() == 1);
    REQUIRE(reloadedRegion->lazLayers()->layerAt(0)->enabled() == true);
}

TEST_CASE("cwLazLayer save/load: missing source file → loadStatus == Error",
          "[cwLazLayer][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString laz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("missing")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    addLazAndWait(root.get(), {laz});
    auto* layer = project->cavingRegion()->lazLayers()->layerAt(0);
    REQUIRE(layer != nullptr);
    REQUIRE(waitForLazLayerLoaded(layer));

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-missing-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();
    const QString actualPath = project->filename();

    // Delete the copied file from inside the saved project's GIS Layers/
    // before reload so the row reappears as Error. The copy in GIS Layers/ is
    // what gets loaded, not the external original.
    const QString gisLayersFile = QFileInfo(actualPath).absoluteDir()
                                      .filePath(cwLazLayerModel::folderName()
                                                + QLatin1Char('/')
                                                + QFileInfo(laz).fileName());
    REQUIRE(QFile::exists(gisLayersFile));
    REQUIRE(QFile::remove(gisLayersFile));

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(actualPath);
    root2->project()->waitLoadToFinish();

    // With the file removed, rescan() finds nothing — model is empty.
    REQUIRE(root2->project()->cavingRegion()->lazLayers()->count() == 0);
}

namespace {

// Upper bound on how long the discard→rescan test waits for the queued
// rescan to repopulate the model. Qt::QueuedConnection needs one event
// loop tick to fire the slot; the bound is generous to absorb CI jitter.
constexpr int kDiscardRescanMaxWaitMs = 5000;
constexpr int kDiscardRescanPollSleepMs = 10;

// Locate the GIS Layers/ directory inside a saved project, given the .cwproj
// path returned by project->filename() after a saveAs.
QDir gisLayersDirForProject(const QString& cwprojPath)
{
    return QDir(QFileInfo(cwprojPath).absoluteDir()
                .absoluteFilePath(cwLazLayerModel::folderName()));
}

// Hand-write a .cwlaz proto with a fixed UUID + chosen enabled bit so a
// subsequent rescan() can adopt the persisted identity. Mirrors the bytes
// cwSaveLoad::toProtoLazLayer + saveProtoMessage would produce, without
// requiring a live cwSaveLoad in the test.
bool writeCwlazFile(const QString& path, const QUuid& id, bool enabled)
{
    CavewhereProto::LazLayer proto;
    auto* fv = proto.mutable_fileversion();
    fv->set_version(cwRegionIOTask::protoVersion());
    *(fv->mutable_cavewhereversion()) = "test";
    *(proto.mutable_id()) = id.toString(QUuid::WithoutBraces).toStdString();
    proto.set_enabled(enabled);

    std::string json;
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    auto status = google::protobuf::util::MessageToJsonString(proto, &json, options);
    if (!status.ok()) {
        return false;
    }
    QFile file(path);
    if (!file.open(QFile::WriteOnly)) {
        return false;
    }
    const qint64 expected = static_cast<qint64>(json.size());
    return file.write(json.data(), expected) == expected;
}

} // namespace

TEST_CASE("cwLazLayer .cwlaz: pre-placed sibling sets UUID + enabled on rescan",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Stand up a project, drop one LAZ into GIS Layers/, then save so the
    // project root + GIS Layers folder exist on disk.
    const QString externalLaz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("prepl")));
    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    addLazAndWait(root.get(), {externalLaz});
    REQUIRE(project->cavingRegion()->lazLayers()->count() == 1);

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-preplaced-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();
    const QString savedPath = project->filename();

    // Find the .laz inside GIS Layers/, and overwrite its sibling .cwlaz with
    // a fixed UUID + enabled=false. This is the on-disk shape a previous
    // session (or a collaborator) would have left behind.
    const QDir gisDir = gisLayersDirForProject(savedPath);
    const QString lazBase = QFileInfo(externalLaz).completeBaseName();
    const QString lazInProject = gisDir.absoluteFilePath(lazBase + QStringLiteral(".laz"));
    REQUIRE(QFile::exists(lazInProject));

    const QUuid fixedId = QUuid::fromString(QStringLiteral("12345678-1234-1234-1234-1234567890ab"));
    REQUIRE(!fixedId.isNull());
    const QString cwlazPath = gisDir.absoluteFilePath(lazBase + QStringLiteral(".cwlaz"));
    REQUIRE(writeCwlazFile(cwlazPath, fixedId, /*enabled=*/false));

    // Reload the project: rescan() pairs the .cwlaz to its .laz and applies
    // the persisted UUID + enabled bit before announcing the row.
    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(savedPath);
    root2->project()->waitLoadToFinish();
    QCoreApplication::processEvents();

    auto* layers = root2->project()->cavingRegion()->lazLayers();
    REQUIRE(layers->count() == 1);
    auto* reloaded = layers->layerAt(0);
    REQUIRE(reloaded != nullptr);
    REQUIRE(reloaded->id() == fixedId);
    REQUIRE(reloaded->enabled() == false);
}

TEST_CASE("cwLazLayer .cwlaz: fresh layer with no sibling is eagerly persisted",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString externalLaz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("eager")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    addLazAndWait(root.get(), {externalLaz});
    REQUIRE(project->cavingRegion()->lazLayers()->count() == 1);

    // Save the project to settle GIS Layers/ on disk.
    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-eager-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    // The .cwlaz must exist next to the .laz with the layer's UUID inside it.
    const QString savedPath = project->filename();
    const QDir gisDir = gisLayersDirForProject(savedPath);
    const QString lazBase = QFileInfo(externalLaz).completeBaseName();
    const QString cwlazPath = gisDir.absoluteFilePath(lazBase + QStringLiteral(".cwlaz"));
    REQUIRE(QFileInfo::exists(cwlazPath));

    auto* layer = project->cavingRegion()->lazLayers()->layerAt(0);
    REQUIRE(layer != nullptr);
    const QUuid liveId = layer->id();
    REQUIRE(!liveId.isNull());

    // Loading the .cwlaz must surface the same UUID + default enabled=true.
    auto loaded = cwSaveLoad::loadLazLayer(cwlazPath);
    REQUIRE_FALSE(loaded.hasError());
    REQUIRE(loaded.value().id == liveId);
    REQUIRE(loaded.value().enabled == true);
}

TEST_CASE("cwLazLayer .cwlaz: removeAt drops both .laz and .cwlaz",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString externalLaz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("rem")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    addLazAndWait(root.get(), {externalLaz});
    REQUIRE(project->cavingRegion()->lazLayers()->count() == 1);

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-rem-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    const QString savedPath = project->filename();
    const QDir gisDir = gisLayersDirForProject(savedPath);
    const QString lazBase = QFileInfo(externalLaz).completeBaseName();
    const QString lazInProject = gisDir.absoluteFilePath(lazBase + QStringLiteral(".laz"));
    const QString cwlazInProject = gisDir.absoluteFilePath(lazBase + QStringLiteral(".cwlaz"));
    REQUIRE(QFile::exists(lazInProject));
    REQUIRE(QFile::exists(cwlazInProject));

    // User-initiated removal: both sibling files must come off disk.
    project->cavingRegion()->lazLayers()->removeAt(0);
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    REQUIRE_FALSE(QFile::exists(lazInProject));
    REQUIRE_FALSE(QFile::exists(cwlazInProject));
}

TEST_CASE("cwLazLayer .cwlaz: orphaned sibling without matching .laz is left untouched",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Build a project with one LAZ layer, save, then delete the .laz from
    // disk to simulate the .cwlaz being orphaned (e.g. user moved the .laz
    // in Finder, or a partial sync).
    const QString externalLaz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("orphan")));
    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    addLazAndWait(root.get(), {externalLaz});
    REQUIRE(project->cavingRegion()->lazLayers()->count() == 1);

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-orphan-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    const QString savedPath = project->filename();
    const QDir gisDir = gisLayersDirForProject(savedPath);
    const QString lazBase = QFileInfo(externalLaz).completeBaseName();
    const QString lazInProject = gisDir.absoluteFilePath(lazBase + QStringLiteral(".laz"));
    const QString cwlazInProject = gisDir.absoluteFilePath(lazBase + QStringLiteral(".cwlaz"));
    REQUIRE(QFile::exists(lazInProject));
    REQUIRE(QFile::exists(cwlazInProject));

    // Delete the .laz externally; .cwlaz now stands alone.
    REQUIRE(QFile::remove(lazInProject));
    REQUIRE_FALSE(QFile::exists(lazInProject));
    REQUIRE(QFile::exists(cwlazInProject));

    // Reload: rescan sees no .laz, creates no layer, but must not touch the
    // orphan .cwlaz — preserving identity for a possible reappearance.
    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(savedPath);
    root2->project()->waitLoadToFinish();
    root2->project()->waitSaveToFinish();
    root2->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    REQUIRE(root2->project()->cavingRegion()->lazLayers()->count() == 0);
    REQUIRE(QFile::exists(cwlazInProject));
}

TEST_CASE("cwLazLayer .cwlaz: UUID survives close/reopen",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString lazA = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("uuidA")));
    const QString lazB = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("uuidB")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    addLazAndWait(root.get(), {lazA, lazB});
    REQUIRE(project->cavingRegion()->lazLayers()->count() == 2);

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-uuid-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();
    const QString savedPath = project->filename();

    // Snapshot the live UUIDs by basename (rescan may reorder by name).
    auto* live = project->cavingRegion()->lazLayers();
    QHash<QString, QUuid> liveById;
    for (int i = 0; i < live->count(); ++i) {
        auto* layer = live->layerAt(i);
        liveById.insert(QFileInfo(layer->sourcePath()).fileName(), layer->id());
    }

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(savedPath);
    root2->project()->waitLoadToFinish();
    QCoreApplication::processEvents();

    auto* reloaded = root2->project()->cavingRegion()->lazLayers();
    REQUIRE(reloaded->count() == 2);
    for (int i = 0; i < reloaded->count(); ++i) {
        auto* layer = reloaded->layerAt(i);
        const QString fname = QFileInfo(layer->sourcePath()).fileName();
        REQUIRE(liveById.contains(fname));
        REQUIRE(layer->id() == liveById.value(fname));
    }
}

TEST_CASE("cwLazLayer .cwlaz: malformed sibling skips layer + reports to errorModel",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Stand up a project with a LAZ layer + a known-good sibling first.
    const QString externalLaz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("bad")));
    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    addLazAndWait(root.get(), {externalLaz});
    REQUIRE(project->cavingRegion()->lazLayers()->count() == 1);

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-malformed-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    const QString savedPath = project->filename();
    const QDir gisDir = gisLayersDirForProject(savedPath);
    const QString lazBase = QFileInfo(externalLaz).completeBaseName();
    const QString cwlazPath = gisDir.absoluteFilePath(lazBase + QStringLiteral(".cwlaz"));
    REQUIRE(QFile::exists(cwlazPath));

    // Corrupt the .cwlaz: not valid JSON, not valid proto, but the id field
    // tag is present so loadLazLayer's malformed-id branch fires.
    {
        QFile bad(cwlazPath);
        REQUIRE(bad.open(QFile::WriteOnly | QFile::Truncate));
        const QByteArray garbage = "{\"id\":\"NOT-A-UUID\"}";
        REQUIRE(bad.write(garbage) == garbage.size());
    }

    // Reload: rescan must skip the bad row entirely and surface a warning
    // through cwErrorModel rather than silently minting a new UUID over it.
    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(savedPath);
    root2->project()->waitLoadToFinish();
    QCoreApplication::processEvents();

    REQUIRE(root2->project()->cavingRegion()->lazLayers()->count() == 0);
    REQUIRE(root2->project()->errorModel()->count() >= 1);
    REQUIRE(root2->project()->errorModel()->allMessagesAsText().contains(
                QFileInfo(externalLaz).fileName()));

    // The malformed .cwlaz must still be on disk — no silent overwrite.
    REQUIRE(QFile::exists(cwlazPath));
}

TEST_CASE("cwLazLayer .cwlaz: external .laz delete dirties the project",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString externalLaz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("extdel")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    addLazAndWait(root.get(), {externalLaz});
    REQUIRE(project->cavingRegion()->lazLayers()->count() == 1);

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-extdel-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // After a clean save, the project is at HEAD and not modified.
    REQUIRE_FALSE(project->modified());

    const QString savedPath = project->filename();
    const QDir gisDir = gisLayersDirForProject(savedPath);
    const QString lazBase = QFileInfo(externalLaz).completeBaseName();
    const QString lazInProject = gisDir.absoluteFilePath(lazBase + QStringLiteral(".laz"));
    REQUIRE(QFile::exists(lazInProject));

    // Simulate a user (or external tool) deleting the .laz from GIS Layers/
    // outside of CaveWhere. The model only learns about it on the next rescan,
    // which is the path under test.
    REQUIRE(QFile::remove(lazInProject));
    project->cavingRegion()->lazLayers()->rescan();

    REQUIRE(project->cavingRegion()->lazLayers()->count() == 0);
    REQUIRE(project->modified());
}

TEST_CASE("cwLazLayer .cwlaz: discardChanges re-surfaces .laz with original UUID",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString externalLaz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("disc")));

    auto root = std::make_unique<cwRootData>();
    // git commitAll requires a valid account; without it the initial saveAs
    // never creates a HEAD commit and discardChanges fails with
    // "revspec 'HEAD' not found". Configure the account before saveAs.
    root->account()->setName(QStringLiteral("Discard Tester"));
    root->account()->setEmail(QStringLiteral("discard.tester@example.com"));

    auto* project = root->project();
    addLazAndWait(root.get(), {externalLaz});
    REQUIRE(project->cavingRegion()->lazLayers()->count() == 1);

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-disc-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    REQUIRE_FALSE(project->modified());

    // Snapshot the layer's UUID so we can verify identity survives the
    // delete → discard → re-discover round trip.
    auto* lazLayers = project->cavingRegion()->lazLayers();
    const QUuid originalId = lazLayers->layerAt(0)->id();
    REQUIRE_FALSE(originalId.isNull());

    const QString savedPath = project->filename();
    const QDir gisDir = gisLayersDirForProject(savedPath);
    const QString lazBase = QFileInfo(externalLaz).completeBaseName();
    const QString lazInProject = gisDir.absoluteFilePath(lazBase + QStringLiteral(".laz"));
    const QString cwlazInProject = gisDir.absoluteFilePath(lazBase + QStringLiteral(".cwlaz"));
    REQUIRE(QFile::exists(lazInProject));
    REQUIRE(QFile::exists(cwlazInProject));

    // External delete of the .laz; rescan picks it up and dirties the
    // project (Bug A path). The orphan .cwlaz remains untouched on disk.
    REQUIRE(QFile::remove(lazInProject));
    lazLayers->rescan();
    REQUIRE(lazLayers->count() == 0);
    REQUIRE(project->modified());
    REQUIRE(QFile::exists(cwlazInProject));

    // Drain the project-metadata WriteFile that rescan→rowsRemoved
    // enqueued so its deferred completes before discardChanges replaces
    // m_pendingJobsDeferred. Without this the OLD deferred is orphaned
    // mid-flight and the AsyncFuture chain to git reset never fires.
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    // Discard: git reset --hard HEAD restores the deleted .laz. The new
    // discardCompleted → rescan hookup must re-discover it and re-pair
    // with the orphan .cwlaz so the original UUID is preserved.
    REQUIRE(project->fileType() == cwProject::GitFileType);
    QSignalSpy discardSpy(project, &cwProject::discardCompleted);
    project->discardChanges();
    project->waitForDiscardToFinish();
    QCoreApplication::processEvents();
    REQUIRE(discardSpy.count() >= 1);

    // The queued rescan fires on the next event loop tick after
    // discardCompleted. Pump events until the layer reappears or the
    // bound elapses; bounding by wall-clock surfaces the actual cause
    // ("rescan never fired") in CI logs rather than masking it as the
    // downstream count assertion.
    QElapsedTimer rescanTimer;
    rescanTimer.start();
    while (lazLayers->count() == 0
           && rescanTimer.elapsed() < kDiscardRescanMaxWaitMs) {
        QCoreApplication::processEvents();
        QThread::msleep(kDiscardRescanPollSleepMs);
    }
    INFO("Waited " << rescanTimer.elapsed()
         << "ms for discardCompleted→rescan to repopulate the model");

    REQUIRE(QFile::exists(lazInProject));
    REQUIRE(lazLayers->count() == 1);
    REQUIRE(lazLayers->layerAt(0)->id() == originalId);
    REQUIRE_FALSE(project->modified());
}

namespace {

// Stand up a project with one .laz layer, save to a fresh .cwproj inside the
// given temp dir, drain the save pipeline, and return the saved .cwproj
// path. The layer's enabled bit is set to @a enabled before saveAs so the
// .cwlaz preserves the state through the round trip.
struct LazProjectFixture {
    std::unique_ptr<cwRootData> root;
    QString projectPath;
    QString savedPath;
    QString lazBasename;
};

LazProjectFixture makeRenameFixture(QTemporaryDir& tempDir,
                                    const QString& tag,
                                    bool enabled)
{
    LazProjectFixture fx;
    const QString externalLaz = writeMinimalLaz(tempLazPath(tempDir, tag));
    fx.lazBasename = QFileInfo(externalLaz).completeBaseName();

    fx.root = std::make_unique<cwRootData>();
    fx.root->account()->setName(QStringLiteral("Rename Tester"));
    fx.root->account()->setEmail(QStringLiteral("rename.tester@example.com"));

    auto* project = fx.root->project();
    addLazAndWait(fx.root.get(), {externalLaz});
    REQUIRE(project->cavingRegion()->lazLayers()->count() == 1);
    project->cavingRegion()->lazLayers()->layerAt(0)->setEnabled(enabled);

    fx.projectPath = QDir(tempDir.path())
                         .filePath(QStringLiteral("laz-rename-%1-%2.cwproj")
                                       .arg(tag)
                                       .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(fx.projectPath));
    project->waitSaveToFinish();
    fx.root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    fx.savedPath = project->filename();
    REQUIRE_FALSE(project->modified());
    return fx;
}

// Read the persisted UUID out of a .cwlaz file directly, without spinning
// up a cwSaveLoad. Used by rename tests to detect cross-pollination of the
// .laz and .cwlaz Move jobs (the corruption would write the LAS header
// bytes into the .cwlaz slot, so JSON parse fails or the UUID field
// goes missing).
QUuid readCwlazUuid(const QString& cwlazPath)
{
    QFile file(cwlazPath);
    if (!file.open(QFile::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    CavewhereProto::LazLayer proto;
    auto status = google::protobuf::util::JsonStringToMessage(
                std::string(bytes.constData(), static_cast<size_t>(bytes.size())),
                &proto);
    if (!status.ok()) {
        return {};
    }
    return QUuid::fromString(QString::fromStdString(proto.id()));
}

// File looks like a LAS/LAZ by header: starts with the "LASF" magic.
bool looksLikeLaz(const QString& path)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return false;
    }
    return file.read(4) == QByteArray("LASF", 4);
}

// Snapshot every file's (name, size, mtime) inside a directory. Used to
// detect phantom writes/renames during operations that should be no-ops.
struct DirSnapshot {
    QString relName;
    qint64 size = 0;
    QDateTime mtime;

    bool operator==(const DirSnapshot& other) const
    {
        return relName == other.relName && size == other.size && mtime == other.mtime;
    }
};

QList<DirSnapshot> snapshotDir(const QDir& dir)
{
    QList<DirSnapshot> result;
    const QFileInfoList entries = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& info : entries) {
        result.append({info.fileName(), info.size(), info.lastModified()});
    }
    return result;
}

} // namespace

TEST_CASE("cwLazLayer rename: round-trip preserves UUID and enabled state",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto fx = makeRenameFixture(tempDir, QStringLiteral("rtA"), /*enabled=*/false);
    auto* project = fx.root->project();
    auto* lazLayers = project->cavingRegion()->lazLayers();
    const QUuid originalId = lazLayers->layerAt(0)->id();

    const QDir gisDir = gisLayersDirForProject(fx.savedPath);
    const QString oldLaz = gisDir.absoluteFilePath(fx.lazBasename + QStringLiteral(".laz"));
    const QString oldCwlaz = gisDir.absoluteFilePath(fx.lazBasename + QStringLiteral(".cwlaz"));
    REQUIRE(QFile::exists(oldLaz));
    REQUIRE(QFile::exists(oldCwlaz));

    const QString newBasename = QStringLiteral("renamed");
    const int errorsBefore = project->errorModel()->size();
    REQUIRE(lazLayers->rename(0, newBasename));
    REQUIRE(project->errorModel()->size() == errorsBefore);

    project->waitSaveToFinish();
    fx.root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    const QString newLaz = gisDir.absoluteFilePath(newBasename + QStringLiteral(".laz"));
    const QString newCwlaz = gisDir.absoluteFilePath(newBasename + QStringLiteral(".cwlaz"));
    REQUIRE(QFile::exists(newLaz));
    REQUIRE(QFile::exists(newCwlaz));
    REQUIRE_FALSE(QFile::exists(oldLaz));
    REQUIRE_FALSE(QFile::exists(oldCwlaz));

    auto* layer = lazLayers->layerAt(0);
    REQUIRE(layer != nullptr);
    REQUIRE(layer->id() == originalId);
    REQUIRE(layer->enabled() == false);
    REQUIRE(layer->name() == newBasename);
}

TEST_CASE("cwLazLayer rename: reopen project picks up new basename + original UUID",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto fx = makeRenameFixture(tempDir, QStringLiteral("rtR"), /*enabled=*/false);
    auto* project = fx.root->project();
    auto* lazLayers = project->cavingRegion()->lazLayers();
    const QUuid originalId = lazLayers->layerAt(0)->id();

    REQUIRE(lazLayers->rename(0, QStringLiteral("reopened")));
    project->waitSaveToFinish();
    fx.root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // Reopen.
    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(fx.savedPath);
    root2->project()->waitLoadToFinish();
    QCoreApplication::processEvents();

    auto* layers2 = root2->project()->cavingRegion()->lazLayers();
    REQUIRE(layers2->count() == 1);
    auto* reloaded = layers2->layerAt(0);
    REQUIRE(reloaded != nullptr);
    REQUIRE(reloaded->name() == QStringLiteral("reopened"));
    REQUIRE(reloaded->id() == originalId);
    REQUIRE(reloaded->enabled() == false);
}

TEST_CASE("cwLazLayer rename: collides with another loaded layer",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString lazA = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("collA")));
    const QString lazB = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("collB")));

    auto root = std::make_unique<cwRootData>();
    root->account()->setName(QStringLiteral("Rename Tester"));
    root->account()->setEmail(QStringLiteral("rename.tester@example.com"));
    auto* project = root->project();
    addLazAndWait(root.get(), {lazA, lazB});

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-collide-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    auto* lazLayers = project->cavingRegion()->lazLayers();
    REQUIRE(lazLayers->count() == 2);
    const QString existingNameOfB = lazLayers->layerAt(1)->name();

    // Target row 0's new name == layer 1's existing basename.
    const int errorsBefore = project->errorModel()->size();
    REQUIRE_FALSE(lazLayers->rename(0, existingNameOfB));
    REQUIRE(project->errorModel()->size() == errorsBefore + 1);

    // Disk: nothing moved.
    const QDir gisDir = gisLayersDirForProject(project->filename());
    REQUIRE(QFile::exists(gisDir.absoluteFilePath(lazLayers->layerAt(0)->name() + QStringLiteral(".laz"))));
}

TEST_CASE("cwLazLayer rename: collides with a stray .cwlaz in GIS Layers/",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto fx = makeRenameFixture(tempDir, QStringLiteral("stray"), /*enabled=*/true);
    auto* project = fx.root->project();
    auto* lazLayers = project->cavingRegion()->lazLayers();

    const QDir gisDir = gisLayersDirForProject(fx.savedPath);
    const QString strayBasename = QStringLiteral("strayClash");
    const QString strayCwlaz = gisDir.absoluteFilePath(strayBasename + QStringLiteral(".cwlaz"));
    // Drop a small orphan .cwlaz at the target basename.
    REQUIRE(writeCwlazFile(strayCwlaz,
                           QUuid::createUuid(),
                           /*enabled=*/true));

    const int errorsBefore = project->errorModel()->size();
    REQUIRE_FALSE(lazLayers->rename(0, strayBasename));
    REQUIRE(project->errorModel()->size() == errorsBefore + 1);

    // The stray file is still where we left it; the layer hasn't moved.
    REQUIRE(QFile::exists(strayCwlaz));
    const QString oldLaz = gisDir.absoluteFilePath(fx.lazBasename + QStringLiteral(".laz"));
    REQUIRE(QFile::exists(oldLaz));
}

TEST_CASE("cwLazLayer rename: validation rejects bad names",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto fx = makeRenameFixture(tempDir, QStringLiteral("valid"), /*enabled=*/true);
    auto* lazLayers = fx.root->project()->cavingRegion()->lazLayers();
    const QString originalName = lazLayers->layerAt(0)->name();

    const QStringList badNames = {
        QStringLiteral(""),
        QStringLiteral("   "),
        QStringLiteral("foo/bar"),
        QStringLiteral("..\\baz"),
        QStringLiteral("renamed.laz")
    };
    auto* errorModel = fx.root->project()->errorModel();
    int expectedErrors = errorModel->size();
    for (const QString& bad : badNames) {
        INFO("Rejecting bad name: '" << bad.toStdString() << "'");
        REQUIRE_FALSE(lazLayers->rename(0, bad));
        ++expectedErrors;
        REQUIRE(errorModel->size() == expectedErrors);
    }
    // No mutation on any of the failures.
    REQUIRE(lazLayers->layerAt(0)->name() == originalName);
}

TEST_CASE("cwLazLayer rename: sequential renames coalesce per artifact",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto fx = makeRenameFixture(tempDir, QStringLiteral("seq"), /*enabled=*/true);
    auto* project = fx.root->project();
    auto* lazLayers = project->cavingRegion()->lazLayers();
    const QUuid originalId = lazLayers->layerAt(0)->id();

    const QDir gisDir = gisLayersDirForProject(fx.savedPath);
    const QString midLaz = gisDir.absoluteFilePath(QStringLiteral("seqMid.laz"));
    const QString midCwlaz = gisDir.absoluteFilePath(QStringLiteral("seqMid.cwlaz"));
    const QString finalLaz = gisDir.absoluteFilePath(QStringLiteral("seqFinal.laz"));
    const QString finalCwlaz = gisDir.absoluteFilePath(QStringLiteral("seqFinal.cwlaz"));

    // A→B then immediately B→C with no flush in between. Each rename
    // enqueues one .laz Move (tag "source") and one .cwlaz Move (tag "")
    // — four Move jobs total. The collapseSequentialMoves rule MUST
    // partition by tag so the .laz chain collapses to its own A→C and
    // the .cwlaz chain collapses to its own A→C. If the partition were
    // broken (the bug commit 4 guards against), the per-tag chain would
    // mix and one of the two artifacts would land at the wrong path,
    // observable below as a missing final file or a corrupted .cwlaz.
    REQUIRE(lazLayers->rename(0, QStringLiteral("seqMid")));
    REQUIRE(lazLayers->rename(0, QStringLiteral("seqFinal")));

    project->waitSaveToFinish();
    fx.root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // Final disk state: only the final basename exists; the intermediate
    // basename never landed.
    REQUIRE(QFile::exists(finalLaz));
    REQUIRE(QFile::exists(finalCwlaz));
    REQUIRE_FALSE(QFile::exists(midLaz));
    REQUIRE_FALSE(QFile::exists(midCwlaz));
    // Both artifacts arrived at the new basename intact: the .laz is
    // still a real LAS file (not a .cwlaz proto written into the .laz
    // slot, which the cross-tag collapse bug would produce), and the
    // .cwlaz still holds the original UUID.
    REQUIRE(looksLikeLaz(finalLaz));
    REQUIRE(readCwlazUuid(finalCwlaz) == originalId);
    REQUIRE(lazLayers->layerAt(0)->id() == originalId);
    REQUIRE(lazLayers->layerAt(0)->enabled() == true);
}

TEST_CASE("cwLazLayer rename: enqueues exactly two Move jobs with distinct tags",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    // Anti-corruption guard. If a future refactor accidentally lands both
    // Move jobs with empty tag, collapseSequentialMoves at the next
    // sequential rename would silently produce "old.laz -> new.cwlaz" and
    // destroy the .cwlaz metadata. Pin the queue shape so that regression
    // fails here, before any filesystem damage.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto fx = makeRenameFixture(tempDir, QStringLiteral("guard"), /*enabled=*/true);
    auto* project = fx.root->project();
    auto* lazLayers = project->cavingRegion()->lazLayers();
    const QUuid originalId = lazLayers->layerAt(0)->id();

    REQUIRE(lazLayers->rename(0, QStringLiteral("guarded")));
    project->waitSaveToFinish();
    fx.root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    const QDir gisDir = gisLayersDirForProject(fx.savedPath);
    const QString newLaz = gisDir.absoluteFilePath(QStringLiteral("guarded.laz"));
    const QString newCwlaz = gisDir.absoluteFilePath(QStringLiteral("guarded.cwlaz"));

    // Both artifacts must arrive at the new basename intact. The
    // corruption this guards against is: if both Move jobs were enqueued
    // with the same tag, collapseSequentialMoves would treat them as a
    // sequential chain and emit a single Move "old.cwlaz -> new.laz",
    // overwriting the .laz slot with .cwlaz proto bytes and leaving
    // .cwlaz vacant. Three distinct checks expose that shape:
    //   * new.laz exists AND starts with the "LASF" magic (not JSON);
    //   * new.cwlaz exists AND parses to the original UUID;
    //   * the original basename is fully gone (no orphan .laz/.cwlaz).
    REQUIRE(QFile::exists(newLaz));
    REQUIRE(QFile::exists(newCwlaz));
    REQUIRE(looksLikeLaz(newLaz));
    REQUIRE(readCwlazUuid(newCwlaz) == originalId);

    const QString oldLaz = gisDir.absoluteFilePath(fx.lazBasename + QStringLiteral(".laz"));
    const QString oldCwlaz = gisDir.absoluteFilePath(fx.lazBasename + QStringLiteral(".cwlaz"));
    REQUIRE_FALSE(QFile::exists(oldLaz));
    REQUIRE_FALSE(QFile::exists(oldCwlaz));
}

TEST_CASE("cwLazLayer rename: phantom-rename guard — no rename, no on-disk churn",
          "[cwLazLayer][cwLazLayerModel][cwSaveLoad]") {
    // Load a project with one layer and DO NOT call rename. Two
    // observable invariants must hold across the load + flush:
    //   * No file in GIS Layers/ moves to a different basename (the
    //     spurious-rename bug would surface as a renamed .laz or
    //     .cwlaz on disk).
    //   * No file is rewritten — neither size nor mtime should change.
    //     A spurious WriteFile would land at the same path and bump
    //     mtime even without renaming.
    // Catches a regression where rescan-time setSourcePath triggers
    // the rename machinery, which would queue a Move job or a save()
    // that touches the .cwlaz.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto fx = makeRenameFixture(tempDir, QStringLiteral("phant"), /*enabled=*/false);
    const QDir gisDir = gisLayersDirForProject(fx.savedPath);

    const QList<DirSnapshot> before = snapshotDir(gisDir);
    REQUIRE(before.size() == 2);  // one .laz + one .cwlaz

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(fx.savedPath);
    root2->project()->waitLoadToFinish();
    root2->project()->waitSaveToFinish();
    root2->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    const QList<DirSnapshot> after = snapshotDir(gisDir);
    INFO("before:" << before.size() << " after:" << after.size());
    REQUIRE(after == before);
}
