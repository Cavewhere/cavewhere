#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "cwFutureManagerModel.h"
#include "cwFutureManagerToken.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwPointOctreeBuilder.h"
#include "cwPointOctreeManifest.h"

#include "LazFixtureHelper.h"

namespace {
QString octreeJobName(const QString& lazPath)
{
    return QStringLiteral("Building point cloud octree: %1").arg(QFileInfo(lazPath).baseName());
}
}

TEST_CASE("cwLazLayer: status transitions Idle → Loading → Loaded",
          "[cwLazLayer]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QVector<QVector3D> points;
    for (int i = 0; i < 200; ++i) {
        points.append(QVector3D(float(i), float(i) * 0.5f, float(i) * 0.25f));
    }
    const QString path = tempLazPath(tempDir, QStringLiteral("progress"));
    REQUIRE(writeSyntheticLazFile(path, points));

    cwLazLayer layer;
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Idle);

    QSignalSpy statusSpy(&layer, &cwLazLayer::loadStatusChanged);
    QSignalSpy pointCountSpy(&layer, &cwLazLayer::pointCountChanged);
    QSignalSpy octreeSpy(&layer, &cwLazLayer::octreeChanged);

    layer.setSourcePath(path);
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loading);

    REQUIRE(waitForLazLayerLoaded(&layer));
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);
    REQUIRE(layer.pointCount() == points.size());
    REQUIRE(layer.errorMessage().isEmpty());

    REQUIRE(statusSpy.size() >= 2); // Idle→Loading and Loading→Loaded
    REQUIRE(pointCountSpy.size() >= 1);
    REQUIRE(octreeSpy.size() >= 1); // the renderer is told what to draw

    // What the layer publishes is the octree, and every number it reports is
    // the manifest's.
    const cwPointOctreeSource& source = layer.octree();
    REQUIRE_FALSE(source.isNull());
    REQUIRE(source.lazPath == path);
    REQUIRE(source.manifest->pointCount == points.size());
    REQUIRE(source.manifest->fingerprint == source.fingerprint);
    REQUIRE(layer.bboxMin() == source.manifest->bboxMin);
    REQUIRE(layer.bboxMax() == source.manifest->bboxMax);
    REQUIRE(layer.meanSpacingXY() == source.manifest->meanSpacingXY);
    REQUIRE(layer.meanSpacingXY() > 0.0f);

    // The build put the octree in the cache the layer named, so the manifest
    // reads back from there.
    REQUIRE(source.cacheRootPath() == QDir(tempDir.path()).absolutePath());
}

TEST_CASE("cwLazLayer: the octree build is registered as a named job",
          "[cwLazLayer]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QVector<QVector3D> points = {
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f },
        { 2.0f, 2.0f, 2.0f }
    };
    const QString path = tempLazPath(tempDir, QStringLiteral("manager"));
    REQUIRE(writeSyntheticLazFile(path, points));

    cwFutureManagerModel manager;
    cwFutureManagerToken token(&manager);

    cwLazLayer layer;
    layer.setFutureManagerToken(token);
    layer.setCacheRootPath(tempDir.path());

    LazJobRecorder jobs(&manager);
    layer.setSourcePath(path);

    REQUIRE(jobs.waitForName(octreeJobName(path)));

    REQUIRE(waitForLazLayerLoaded(&layer));
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);
}

TEST_CASE("cwLazLayer: a second layer on a built file publishes without a job",
          "[cwLazLayer]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QVector<QVector3D> points;
    for (int i = 0; i < 200; ++i) {
        points.append(QVector3D(float(i), float(i) * 0.5f, float(i) * 0.25f));
    }
    const QString path = tempLazPath(tempDir, QStringLiteral("cache-hit"));
    REQUIRE(writeSyntheticLazFile(path, points));

    cwFutureManagerModel manager;
    cwFutureManagerToken token(&manager);

    cwLazLayer first;
    first.setFutureManagerToken(token);
    first.setCacheRootPath(tempDir.path());
    first.setSourcePath(path);
    REQUIRE(waitForLazLayerLoaded(&first));
    REQUIRE(first.loadStatus() == cwLazLayer::LoadStatus::Loaded);

    // The cache now holds this file's octree, so the second layer reads the
    // manifest and publishes it — no build, and so nothing to report.
    LazJobRecorder jobs(&manager);

    cwLazLayer second;
    second.setFutureManagerToken(token);
    second.setCacheRootPath(tempDir.path());
    second.setSourcePath(path);

    REQUIRE(waitForLazLayerLoaded(&second));
    REQUIRE(second.loadStatus() == cwLazLayer::LoadStatus::Loaded);
    REQUIRE(jobs.names().isEmpty());
    REQUIRE(manager.count() == 0);

    REQUIRE(second.octree() == first.octree());
    REQUIRE(second.octree().manifest->pointCount == first.octree().manifest->pointCount);
    REQUIRE(second.pointCount() == points.size());
}

TEST_CASE("cwLazLayer: a layer destroyed mid-build writes no manifest",
          "[cwLazLayer]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Big enough that the build is still running when the layer goes away.
    QVector<QVector3D> points;
    constexpr int kPointCount = 200000;
    points.reserve(kPointCount);
    for (int i = 0; i < kPointCount; ++i) {
        points.append(QVector3D(float(i % 997), float(i % 991), float(i % 983)));
    }
    const QString path = tempLazPath(tempDir, QStringLiteral("cancel-mid-build"));
    REQUIRE(writeSyntheticLazFile(path, points));

    cwFutureManagerModel manager;
    cwFutureManagerToken token(&manager);

    auto* layer = new cwLazLayer();
    layer->setFutureManagerToken(token);
    layer->setCacheRootPath(tempDir.path());

    LazJobRecorder jobs(&manager);
    layer->setSourcePath(path);

    REQUIRE(jobs.waitForName(octreeJobName(path)));
    REQUIRE(manager.count() == 1);

    delete layer;

    // Give the cancelled build every chance to finish writing: it must not.
    constexpr int kSettleMs = 2000;
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < kSettleMs) {
        spinEventLoopSlice();
    }

    // A cancelled build takes its job row with it, which is what tells this
    // assertion apart from a build that simply failed.
    REQUIRE(manager.count() == 0);

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .sourceCSOverride = QString(),
        .frameCS = QString(),
        .cacheRootPath = tempDir.path()
    };
    REQUIRE_FALSE(cwPointOctreeBuilder::cachedManifest(request).has_value());
}

TEST_CASE("cwLazLayer: a build superseded by a cache hit never publishes its octree",
          "[cwLazLayer]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString embeddedCS = utmZoneWkt(13, -105);
    const QString overrideCS = utmZoneWkt(14, -99);

    // Big enough that the build outlasts the cache probe that supersedes it.
    QVector<QVector3D> points;
    constexpr int kPointCount = 600000;
    points.reserve(kPointCount);
    for (int i = 0; i < kPointCount; ++i) {
        points.append(QVector3D(500000.0f + float(i % 997),
                                4000000.0f + float(i % 991),
                                float(i % 983)));
    }
    const QString path = tempLazPath(tempDir, QStringLiteral("superseded-by-hit"));
    REQUIRE(writeSyntheticLazFile(path, points, embeddedCS));

    constexpr int kBuildTimeoutMs = 60000;
    const QString cacheRoot = QDir(tempDir.path()).absolutePath();

    // Put the overridden CS's octree in the cache, so the reload the override
    // triggers below hits it and publishes without a build of its own.
    {
        cwLazLayer warm;
        warm.setCacheRootPath(cacheRoot);
        warm.setSourceCSOverride(overrideCS);
        warm.setSourcePath(path);
        REQUIRE(waitForLazLayerLoaded(&warm, kBuildTimeoutMs));
        REQUIRE(warm.loadStatus() == cwLazLayer::LoadStatus::Loaded);
    }

    const cwPointOctreeBuilder::Request overridden {
        .path = path,
        .sourceCSOverride = overrideCS,
        .frameCS = QString(),
        .cacheRootPath = cacheRoot
    };
    const QString overriddenFingerprint = cwPointOctreeBuilder::fingerprintFor(overridden);
    REQUIRE_FALSE(overriddenFingerprint.isEmpty());

    cwFutureManagerModel manager;
    cwFutureManagerToken token(&manager);

    cwLazLayer layer;
    layer.setFutureManagerToken(token);
    layer.setCacheRootPath(cacheRoot);

    LazJobRecorder jobs(&manager);
    layer.setSourcePath(path);
    REQUIRE(jobs.waitForName(octreeJobName(path)));

    // The build of the file's own CS is now running. Overriding the CS asks for
    // a different octree, and the cache already has it — so the layer reaches
    // Loaded while the superseded build is still going.
    layer.setSourceCSOverride(overrideCS);
    REQUIRE(waitForLazLayerLoaded(&layer, kBuildTimeoutMs));
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);
    REQUIRE(layer.octree().fingerprint == overriddenFingerprint);

    // Let the superseded build run to completion. Publishing it now would swap
    // the octree under a status that never changes, so the renderer — which
    // hears only loadStatusChanged — would keep drawing the wrong one.
    constexpr int kSettleMs = 6000;
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < kSettleMs) {
        spinEventLoopSlice();
    }

    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);
    REQUIRE(layer.octree().fingerprint == overriddenFingerprint);
}

TEST_CASE("cwLazLayer: setCacheRootPath mid-build rebuilds under the new root",
          "[cwLazLayer]") {
    QTemporaryDir firstRoot;
    QTemporaryDir secondRoot;
    REQUIRE(firstRoot.isValid());
    REQUIRE(secondRoot.isValid());

    // Big enough that the build is still running when the root moves.
    QVector<QVector3D> points;
    constexpr int kPointCount = 200000;
    points.reserve(kPointCount);
    for (int i = 0; i < kPointCount; ++i) {
        points.append(QVector3D(float(i % 997), float(i % 991), float(i % 983)));
    }
    const QString path = tempLazPath(firstRoot, QStringLiteral("root-move"));
    REQUIRE(writeSyntheticLazFile(path, points));

    cwFutureManagerModel manager;
    cwFutureManagerToken token(&manager);

    cwLazLayer layer;
    layer.setFutureManagerToken(token);
    layer.setCacheRootPath(firstRoot.path());

    LazJobRecorder jobs(&manager);
    layer.setSourcePath(path);
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loading);

    // Wait for the job so the root really does move under a running build,
    // rather than under a probe that has not asked for one yet.
    REQUIRE(jobs.waitForName(octreeJobName(path)));
    layer.setCacheRootPath(secondRoot.path());

    // Two builds of 200 k points can run back to back here, so the wait is
    // longer than the default.
    constexpr int kBuildTimeoutMs = 30000;
    REQUIRE(waitForLazLayerLoaded(&layer, kBuildTimeoutMs));
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);

    // The octree the layer publishes is the one the newest request asked for,
    // and it really is in the new root.
    const QString secondProjectRoot = QDir(secondRoot.path()).absolutePath();
    REQUIRE(layer.octree().cacheRootPath() == secondProjectRoot);

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .sourceCSOverride = QString(),
        .frameCS = QString(),
        .cacheRootPath = secondProjectRoot
    };
    const std::optional<cwPointOctreeManifest> cached =
            cwPointOctreeBuilder::cachedManifest(request);
    REQUIRE(cached.has_value());
    REQUIRE(cached->fingerprint == layer.octree().fingerprint);
}

// cwLazLayerModel::rescan uses the (sourceSize, sourceMtime) fingerprint to
// decide whether an existing layer can be reused after a directory scan.
// That contract has to hold even when setSourcePath is called twice with
// the same path between scans (e.g. a future caller doing an explicit
// "reload from the same source" path). Without this, an in-place overwrite
// silently keeps the stale fingerprint and the model would skip the reload.
TEST_CASE("cwLazLayer: setSourcePath refreshes the fingerprint when the file is overwritten",
          "[cwLazLayer]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("fingerprint"));
    REQUIRE(!writeMinimalLaz(path).isEmpty());

    cwLazLayer layer;
    layer.setSourcePath(path);
    const qint64 originalSize = layer.sourceSize();
    const QDateTime originalMtime = layer.sourceMtime();
    REQUIRE(originalSize > 0);
    REQUIRE(originalMtime.isValid());

    // Overwrite with a clearly larger payload so the new file has a
    // distinct size — protects the assertion against 1-second mtime
    // resolution on coarse-grained filesystems.
    QVector<QVector3D> manyPoints;
    manyPoints.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        manyPoints.append(QVector3D(float(i), float(i) * 2.0f, float(i) * 3.0f));
    }
    REQUIRE(writeSyntheticLazFile(path, manyPoints));

    // Same path, different content — setSourcePath must refresh the
    // cached fingerprint so callers can detect the change.
    layer.setSourcePath(path);
    REQUIRE(layer.sourceSize() != originalSize);
    REQUIRE(layer.sourceSize() == QFileInfo(path).size());
}

TEST_CASE("cwLazLayer: missing file transitions to Error", "[cwLazLayer]") {
    cwLazLayer layer;
    const QString missing = QStringLiteral("/nonexistent-progress-%1.laz")
                                .arg(QCoreApplication::applicationPid());
    layer.setSourcePath(missing);
    REQUIRE(waitForLazLayerLoaded(&layer));
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Error);
    REQUIRE(!layer.errorMessage().isEmpty());
    REQUIRE(layer.pointCount() == 0);
}
