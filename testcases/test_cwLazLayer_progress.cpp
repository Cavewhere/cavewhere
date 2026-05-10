#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
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
