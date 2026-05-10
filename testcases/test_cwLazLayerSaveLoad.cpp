#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "cwCavingRegion.h"
#include "cwFutureManagerModel.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwProject.h"
#include "cwRootData.h"

#include "LazFixtureHelper.h"

TEST_CASE("cwLazLayer save/load round-trip preserves sourcePath",
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

    region->lazLayers()->addLayer(lazA);
    region->lazLayers()->addLayer(lazB);
    REQUIRE(region->lazLayers()->count() == 2);

    // Wait for the initial loads to settle so vertexCount is non-zero.
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
    REQUIRE(reloadedRegion->lazLayers()->layerAt(0)->sourcePath() == lazA);
    REQUIRE(reloadedRegion->lazLayers()->layerAt(1)->sourcePath() == lazB);

    // Geometry rebuilt by re-running cwLazLoader against the saved path.
    REQUIRE(waitForLazLayerLoaded(reloadedRegion->lazLayers()->layerAt(0)));
    REQUIRE(waitForLazLayerLoaded(reloadedRegion->lazLayers()->layerAt(1)));
    REQUIRE(reloadedRegion->lazLayers()->layerAt(0)->pointCount() > 0);
    REQUIRE(reloadedRegion->lazLayers()->layerAt(1)->pointCount() > 0);
}

TEST_CASE("cwLazLayer save/load: pointSize runtime override resets to default",
          "[cwLazLayer][cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString laz = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("psize")));

    auto root = std::make_unique<cwRootData>();
    auto* project = root->project();
    auto* region = project->cavingRegion();
    auto* layer = region->lazLayers()->addLayer(laz);

    layer->setPointSize(11.0); // Non-default override.
    REQUIRE(layer->pointSize() == 11.0);
    REQUIRE(waitForLazLayerLoaded(layer));

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-psize-%1.cwproj")
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
    auto* layer = project->cavingRegion()->lazLayers()->addLayer(laz);
    REQUIRE(waitForLazLayerLoaded(layer));

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("laz-missing-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();
    const QString actualPath = project->filename();

    // Delete the source file before reload so the row reappears as Error.
    REQUIRE(QFile::remove(laz));

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(actualPath);
    root2->project()->waitLoadToFinish();

    auto* reloaded = root2->project()->cavingRegion()->lazLayers()->layerAt(0);
    REQUIRE(reloaded != nullptr);
    REQUIRE(reloaded->sourcePath() == laz);
    REQUIRE(reloaded->loadStatus() == cwLazLayer::LoadStatus::Error);
}
