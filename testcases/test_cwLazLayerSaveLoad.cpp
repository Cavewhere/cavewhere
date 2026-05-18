#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include "cwCavingRegion.h"
#include "cwFutureManagerModel.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwProject.h"
#include "cwRootData.h"

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
