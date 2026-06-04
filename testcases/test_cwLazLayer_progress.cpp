#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "cwFutureManagerModel.h"
#include "cwFutureManagerToken.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"

#include "LazFixtureHelper.h"

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

    layer.setSourcePath(path);
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loading);

    REQUIRE(waitForLazLayerLoaded(&layer));
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);
    REQUIRE(layer.pointCount() == points.size());
    REQUIRE(layer.errorMessage().isEmpty());

    REQUIRE(statusSpy.size() >= 2); // Idle→Loading and Loading→Loaded
    REQUIRE(pointCountSpy.size() >= 1);
}

TEST_CASE("cwLazLayer: load future is registered with cwFutureManagerModel",
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

    QSignalSpy rowsInsertedSpy(&manager, &QAbstractItemModel::rowsInserted);
    layer.setSourcePath(path);

    REQUIRE(rowsInsertedSpy.size() >= 1);

    REQUIRE(waitForLazLayerLoaded(&layer));
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);
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
