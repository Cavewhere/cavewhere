#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "cavewhere.pb.h"
#include "cwFutureManagerModel.h"
#include "cwFutureManagerToken.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"

#include "LazFixtureHelper.h"

TEST_CASE("cwLazLayerModel: addLayer / removeAt / count", "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazLayerModel model;
    QSignalSpy countSpy(&model, &cwLazLayerModel::countChanged);
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);

    REQUIRE(model.rowCount() == 0);
    REQUIRE(model.count() == 0);

    const QString pathA = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("a")));
    auto* layerA = model.addLayer(pathA);
    REQUIRE(layerA != nullptr);
    REQUIRE(model.count() == 1);
    REQUIRE(insertSpy.size() == 1);
    REQUIRE(countSpy.size() >= 1);

    const QString pathB = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("b")));
    model.addLayer(pathB);
    REQUIRE(model.count() == 2);

    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.removeAt(0);
    REQUIRE(model.count() == 1);
    REQUIRE(removeSpy.size() == 1);
    REQUIRE(model.layerAt(0)->sourcePath() == pathB);

    // Out-of-range remove is a no-op.
    model.removeAt(99);
    REQUIRE(model.count() == 1);
    model.removeAt(-1);
    REQUIRE(model.count() == 1);
}

TEST_CASE("cwLazLayerModel: data() roles round-trip", "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazLayerModel model;
    const QString path = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("roles")));
    auto* layer = model.addLayer(path);
    REQUIRE(layer != nullptr);

    const QModelIndex idx = model.index(0, 0);
    REQUIRE(model.data(idx, cwLazLayerModel::SourcePathRole).toString() == path);
    REQUIRE(model.data(idx, cwLazLayerModel::NameRole).toString() == layer->name());

    layer->setPointSize(7.5);
    REQUIRE(model.data(idx, cwLazLayerModel::PointSizeRole).toDouble() == 7.5);
}

TEST_CASE("cwLazLayerModel: dataChanged emitted on layer property change",
          "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazLayerModel model;
    const QString path = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("changed")));
    auto* layer = model.addLayer(path);
    REQUIRE(layer != nullptr);

    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

    layer->setPointSize(3.0);
    REQUIRE(dataChangedSpy.size() >= 1);
}

TEST_CASE("cwLazLayerModel: writeTo / readFrom proto round-trip",
          "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString pathA = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("rt-a")));
    const QString pathB = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("rt-b")));

    cwLazLayerModel src;
    src.addLayer(pathA);
    src.addLayer(pathB);
    REQUIRE(src.count() == 2);

    CavewhereProto::ProjectMetadata metadata;
    src.writeTo(&metadata);
    REQUIRE(metadata.lazlayers_size() == 2);

    cwLazLayerModel dst;
    dst.readFrom(metadata);
    REQUIRE(dst.count() == 2);
    REQUIRE(dst.layerAt(0)->sourcePath() == pathA);
    REQUIRE(dst.layerAt(1)->sourcePath() == pathB);
}

TEST_CASE("cwLazLayerModel: missing file → loadStatus == Error",
          "[cwLazLayerModel]") {
    cwLazLayerModel model;
    const QString missingPath = QStringLiteral("/nonexistent-laz-%1.laz")
                                    .arg(QCoreApplication::applicationPid());
    auto* layer = model.addLayer(missingPath);
    REQUIRE(layer != nullptr);
    REQUIRE(waitForLazLayerLoaded(layer));
    REQUIRE(layer->loadStatus() == cwLazLayer::LoadStatus::Error);
    REQUIRE(layer->sourcePath() == missingPath);
    REQUIRE(model.count() == 1);
}
