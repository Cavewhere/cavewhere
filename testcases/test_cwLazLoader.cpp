#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "cwGeoPoint.h"
#include "cwGeometry.h"
#include "cwLazLoader.h"

#include "LazFixtureHelper.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("cwLazLoader: empty source CS short-circuits and applies worldOrigin", "[cwLazLoader]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QVector<QVector3D> input = {
        { 100.0f, 200.0f, 50.0f },
        { 110.0f, 210.0f, 55.0f },
        { 120.0f, 220.0f, 60.0f },
        { 130.0f, 230.0f, 65.0f }
    };

    const QString path = tempLazPath(tempDir, QStringLiteral("identity"));
    REQUIRE(writeSyntheticLazFile(path, input));

    const cwGeoPoint origin(100.0, 200.0, 50.0);
    auto future = cwLazLoader::load({.path = path, .worldOrigin = origin});
    future.waitForFinished();
    REQUIRE(future.resultCount() == 1);
    cwLazLoadResult result = future.result();

    REQUIRE(result.geometry.type() == cwGeometry::Type::Points);
    REQUIRE(result.geometry.attribute(cwGeometry::Semantic::Position) != nullptr);
    REQUIRE(result.geometry.vertexCount() == input.size());

    QVector<QVector3D> out =
        result.geometry.values<QVector3D>(cwGeometry::Semantic::Position);
    REQUIRE(out.size() == input.size());

    // Each point should have worldOrigin subtracted.
    REQUIRE_THAT(out[0].x(), WithinAbs(0.0f, 1e-3f));
    REQUIRE_THAT(out[0].y(), WithinAbs(0.0f, 1e-3f));
    REQUIRE_THAT(out[0].z(), WithinAbs(0.0f, 1e-3f));
    REQUIRE_THAT(out[1].x(), WithinAbs(10.0f, 1e-3f));
    REQUIRE_THAT(out[1].y(), WithinAbs(10.0f, 1e-3f));
    REQUIRE_THAT(out[1].z(), WithinAbs(5.0f, 1e-3f));

    REQUIRE(result.sourceCS.isEmpty());
}

TEST_CASE("cwLazLoader: bbox tracks min/max in worldOrigin space", "[cwLazLoader]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QVector<QVector3D> input = {
        { 0.0f, 0.0f, 0.0f },
        { 100.0f, 0.0f, -10.0f },
        { 50.0f, 80.0f, 20.0f },
        { -5.0f, 25.0f, 5.0f }
    };
    const QString path = tempLazPath(tempDir, QStringLiteral("bbox"));
    REQUIRE(writeSyntheticLazFile(path, input));

    auto future = cwLazLoader::load({.path = path});
    future.waitForFinished();
    REQUIRE(future.resultCount() == 1);
    cwLazLoadResult result = future.result();

    REQUIRE_THAT(result.bboxMin.x(), WithinAbs(-5.0f, 1e-3f));
    REQUIRE_THAT(result.bboxMin.y(), WithinAbs(0.0f, 1e-3f));
    REQUIRE_THAT(result.bboxMin.z(), WithinAbs(-10.0f, 1e-3f));
    REQUIRE_THAT(result.bboxMax.x(), WithinAbs(100.0f, 1e-3f));
    REQUIRE_THAT(result.bboxMax.y(), WithinAbs(80.0f, 1e-3f));
    REQUIRE_THAT(result.bboxMax.z(), WithinAbs(20.0f, 1e-3f));
}

TEST_CASE("cwLazLoader: maxPoints caps the read count", "[cwLazLoader]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QVector<QVector3D> input;
    for (int i = 0; i < 50; ++i) {
        input.append(QVector3D(float(i), float(i) * 2.0f, float(i) * 0.5f));
    }
    const QString path = tempLazPath(tempDir, QStringLiteral("max"));
    REQUIRE(writeSyntheticLazFile(path, input));

    auto future = cwLazLoader::load({.path = path, .maxPoints = 10});
    future.waitForFinished();
    REQUIRE(future.resultCount() == 1);
    cwLazLoadResult result = future.result();
    REQUIRE(result.geometry.vertexCount() == 10);
}

TEST_CASE("cwLazLoader: missing file returns empty result", "[cwLazLoader]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("does-not-exist-%1.laz")
                                              .arg(QCoreApplication::applicationPid()));
    auto future = cwLazLoader::load({.path = path});
    future.waitForFinished();
    REQUIRE(future.resultCount() == 1);
    cwLazLoadResult result = future.result();
    REQUIRE(result.geometry.vertexCount() == 0);
}
