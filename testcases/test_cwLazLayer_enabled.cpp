#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "cwLazLayer.h"

#include "LazFixtureHelper.h"

TEST_CASE("cwLazLayer::enabled defaults to true",
          "[cwLazLayer][cwLazLayerEnabled]") {
    cwLazLayer layer;
    REQUIRE(layer.enabled() == true);
}

TEST_CASE("cwLazLayer::setEnabled emits enabledChanged on change; nothing on no-op",
          "[cwLazLayer][cwLazLayerEnabled]") {
    cwLazLayer layer;
    QSignalSpy enabledSpy(&layer, &cwLazLayer::enabledChanged);

    // No-op: layer is already enabled.
    layer.setEnabled(true);
    REQUIRE(enabledSpy.size() == 0);

    // Real change.
    layer.setEnabled(false);
    REQUIRE(enabledSpy.size() == 1);
    REQUIRE(layer.enabled() == false);

    // Re-disable is a no-op.
    layer.setEnabled(false);
    REQUIRE(enabledSpy.size() == 1);

    // Re-enable.
    layer.setEnabled(true);
    REQUIRE(enabledSpy.size() == 2);
    REQUIRE(layer.enabled() == true);
}

TEST_CASE("cwLazLayer::setEnabled(false) on a loaded layer drops geometry and resets to Idle",
          "[cwLazLayer][cwLazLayerEnabled]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QVector<QVector3D> points;
    for (int i = 0; i < 50; ++i) {
        points.append(QVector3D(float(i), float(i) * 0.5f, float(i) * 0.25f));
    }
    const QString path = tempLazPath(tempDir, QStringLiteral("disable-loaded"));
    REQUIRE(writeSyntheticLazFile(path, points));

    cwLazLayer layer;
    layer.setSourcePath(path);
    REQUIRE(waitForLazLayerLoaded(&layer));
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);
    REQUIRE(layer.pointCount() == points.size());

    QSignalSpy statusSpy(&layer, &cwLazLayer::loadStatusChanged);
    QSignalSpy pointCountSpy(&layer, &cwLazLayer::pointCountChanged);

    layer.setEnabled(false);

    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Idle);
    REQUIRE(layer.pointCount() == 0);
    REQUIRE(layer.errorMessage().isEmpty());
    REQUIRE(statusSpy.size() >= 1); // Loaded → Idle
    REQUIRE(pointCountSpy.size() >= 1); // points dropped
}

TEST_CASE("cwLazLayer::setEnabled(true) on a disabled layer with a sourcePath reloads",
          "[cwLazLayer][cwLazLayerEnabled]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("re-enable")));

    cwLazLayer layer;
    layer.setEnabled(false);
    REQUIRE(layer.enabled() == false);

    // setSourcePath on a disabled layer records the path but does not load.
    layer.setSourcePath(path);
    REQUIRE(layer.sourcePath() == path);
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Idle);

    layer.setEnabled(true);
    // Re-enable kicks off a fresh async load.
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loading);
    REQUIRE(waitForLazLayerLoaded(&layer));
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);
    REQUIRE(layer.pointCount() > 0);
}

TEST_CASE("cwLazLayer: setSourcePath on a disabled layer captures fingerprint, stays Idle",
          "[cwLazLayer][cwLazLayerEnabled]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("disabled-setpath")));

    cwLazLayer layer;
    layer.setEnabled(false);
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Idle);

    layer.setSourcePath(path);

    REQUIRE(layer.sourcePath() == path);
    // Fingerprint must be recorded so cwLazLayerModel::rescan can detect
    // in-place overwrites later.
    const QFileInfo info(path);
    REQUIRE(layer.sourceSize() == info.size());
    REQUIRE(layer.sourceMtime() == info.lastModified());
    // But no async load fired.
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Idle);
    REQUIRE(layer.pointCount() == 0);

    // Sanity: pump the event loop briefly to confirm no transition leaks
    // through asynchronously.
    QCoreApplication::processEvents();
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Idle);
}
