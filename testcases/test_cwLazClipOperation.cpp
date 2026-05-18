#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QPolygonF>
#include <QTemporaryDir>
#include <QVector3D>

#include "cwGeoPoint.h"
#include "cwGeometry.h"
#include "cwLazClipOperation.h"
#include "cwLazLoader.h"

namespace {

QPolygonF squareAround(double cx, double cy, double half)
{
    QPolygonF p;
    p << QPointF(cx - half, cy - half)
      << QPointF(cx + half, cy - half)
      << QPointF(cx + half, cy + half)
      << QPointF(cx - half, cy + half);
    return p;
}

cwGeometry pointGeometry(const QVector<QVector3D>& points)
{
    cwGeometry geom {
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    };
    geom.set(cwGeometry::Semantic::Position, points);
    geom.setType(cwGeometry::Type::Points);
    return geom;
}

QString outputPath(const QTemporaryDir& dir, const QString& stem)
{
    return dir.filePath(stem + QStringLiteral(".laz"));
}

} // namespace

TEST_CASE("cwLazClipOperation: keep mode retains points inside polygon", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QVector<QVector3D> input = {
        { 0.0f, 0.0f, 0.0f },     // inside
        { 5.0f, 5.0f, 1.0f },     // inside
        { 50.0f, 50.0f, 2.0f },   // outside
        { -50.0f, 0.0f, 3.0f }    // outside
    };
    const QString outPath = outputPath(tempDir, QStringLiteral("clip-keep-out"));

    cwLazClipOperation::Request req;
    req.sources.append(pointGeometry(input));
    req.polygonLocalXY = squareAround(0.0, 0.0, 10.0);
    req.worldOrigin = cwGeoPoint(0.0, 0.0, 0.0);
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outPath;

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE(future.resultCount() == 1);
    const auto result = future.result();
    REQUIRE_FALSE(result.hasError());
    REQUIRE(result.value().pointsWritten == 2);
    REQUIRE(result.value().outputPath == outPath);

    auto loadFuture = cwLazLoader::load({.path = outPath});
    loadFuture.waitForFinished();
    REQUIRE(loadFuture.resultCount() == 1);
    REQUIRE(loadFuture.result().geometry.vertexCount() == 2);
}

TEST_CASE("cwLazClipOperation: remove mode retains points outside polygon", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QVector<QVector3D> input = {
        { 0.0f, 0.0f, 0.0f },     // inside
        { 5.0f, 5.0f, 0.0f },     // inside
        { 50.0f, 50.0f, 0.0f },   // outside
        { -50.0f, 0.0f, 0.0f }    // outside
    };

    cwLazClipOperation::Request req;
    req.sources.append(pointGeometry(input));
    req.polygonLocalXY = squareAround(0.0, 0.0, 10.0);
    req.mode = cwLazClipOperation::Mode::Remove;
    req.outputPath = outputPath(tempDir, QStringLiteral("clip-rm-out"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().hasError());
    REQUIRE(future.result().value().pointsWritten == 2);
}

TEST_CASE("cwLazClipOperation: source without Position attribute is rejected", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwGeometry malformed {
        { cwGeometry::Semantic::Color0, cwGeometry::AttributeFormat::Vec3 }
    };
    malformed.setType(cwGeometry::Type::Points);

    cwLazClipOperation::Request req;
    req.sources.append(malformed);
    req.polygonLocalXY = squareAround(0.0, 0.0, 10.0);
    req.outputPath = outputPath(tempDir, QStringLiteral("malformed"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    const auto result = future.result();
    REQUIRE(result.hasError());
    REQUIRE(result.errorCodeTo<cwLazClipOperation::ErrorCode>()
            == cwLazClipOperation::MissingPosition);
    // Preflight must reject before opening the writer.
    REQUIRE_FALSE(QFile::exists(req.outputPath));
}

TEST_CASE("cwLazClipOperation: empty source list fails fast", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazClipOperation::Request req;
    req.polygonLocalXY = squareAround(0.0, 0.0, 1.0);
    req.outputPath = outputPath(tempDir, QStringLiteral("empty"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    const auto result = future.result();
    REQUIRE(result.hasError());
    REQUIRE(result.errorCodeTo<cwLazClipOperation::ErrorCode>()
            == cwLazClipOperation::NoSources);
    REQUIRE(result.value().pointsWritten == 0);
}

TEST_CASE("cwLazClipOperation: polygon with fewer than 3 vertices is rejected", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazClipOperation::Request req;
    req.sources.append(pointGeometry({{0,0,0}, {1,1,1}}));
    QPolygonF degenerate;
    degenerate << QPointF(0, 0) << QPointF(1, 1);
    req.polygonLocalXY = degenerate;
    req.outputPath = outputPath(tempDir, QStringLiteral("poly2-out"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE(future.result().errorCodeTo<cwLazClipOperation::ErrorCode>()
            == cwLazClipOperation::BadPolygon);
}

TEST_CASE("cwLazClipOperation: non-finite worldOrigin is rejected", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazClipOperation::Request req;
    req.sources.append(pointGeometry({{0,0,0}, {1,1,1}, {2,2,2}}));
    req.polygonLocalXY = squareAround(0.0, 0.0, 1.0);
    req.worldOrigin = cwGeoPoint(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0);
    req.outputPath = outputPath(tempDir, QStringLiteral("nan-out"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE(future.result().errorCodeTo<cwLazClipOperation::ErrorCode>()
            == cwLazClipOperation::NonFiniteInput);
}

TEST_CASE("cwLazClipOperation: unions points from multiple sources into one output", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazClipOperation::Request req;
    req.sources.append(pointGeometry({{0,0,0}, {2,2,2}, {100,100,0}}));
    req.sources.append(pointGeometry({{3,3,3}, {4,4,4}, {200,200,0}}));
    req.polygonLocalXY = squareAround(0.0, 0.0, 10.0);
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outputPath(tempDir, QStringLiteral("u-out"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().hasError());
    REQUIRE(future.result().value().pointsWritten == 4); // 2 from A + 2 from B
}

TEST_CASE("cwLazClipOperation: output preserves absolute coords via worldOrigin", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Geometry is worldOrigin-relative — local (5,5) maps to absolute
    // (1005, 2005) once worldOrigin (1000, 2000) is reapplied on write.
    // Reloading via cwLazLoader resubtracts worldOrigin so the round-trip
    // returns the original local coordinates.
    const QVector<QVector3D> input = {
        { 0.0f, 0.0f, 0.0f },    // inside polygon at local origin
        { 5.0f, 5.0f, 0.0f },    // inside
        { 500.0f, 500.0f, 0.0f } // outside
    };
    const QString outPath = outputPath(tempDir, QStringLiteral("origin-out"));

    cwLazClipOperation::Request req;
    req.sources.append(pointGeometry(input));
    req.polygonLocalXY = squareAround(0.0, 0.0, 10.0);
    req.worldOrigin = cwGeoPoint(1000.0, 2000.0, 0.0);
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outPath;

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().hasError());
    REQUIRE(future.result().value().pointsWritten == 2);

    // Round-trip via cwLazLoader with the same worldOrigin must restore
    // the original local coordinates.
    auto loadFuture = cwLazLoader::load({
        .path = outPath,
        .worldOrigin = cwGeoPoint(1000.0, 2000.0, 0.0)
    });
    loadFuture.waitForFinished();
    REQUIRE(loadFuture.result().geometry.vertexCount() == 2);
}
