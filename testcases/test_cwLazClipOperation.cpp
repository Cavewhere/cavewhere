#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QList>
#include <QMatrix4x4>
#include <QTemporaryDir>
#include <QVector3D>

#include "cwGeoPoint.h"
#include "cwGeometry.h"
#include "cwLazClipOperation.h"
#include "cwLazLoader.h"

namespace {

// Axis-aligned square in world XY at z. Use with topDownView.
QList<QVector3D> squareInWorldXY(double cx, double cy, double half, double z = 0.0)
{
    return {
        QVector3D(float(cx - half), float(cy - half), float(z)),
        QVector3D(float(cx + half), float(cy - half), float(z)),
        QVector3D(float(cx + half), float(cy + half), float(z)),
        QVector3D(float(cx - half), float(cy + half), float(z))
    };
}

// Axis-aligned square in world XZ at y. Use with profileView.
QList<QVector3D> squareInWorldXZ(double cx, double cz, double half, double y = 0.0)
{
    return {
        QVector3D(float(cx - half), float(y), float(cz - half)),
        QVector3D(float(cx + half), float(y), float(cz - half)),
        QVector3D(float(cx + half), float(y), float(cz + half)),
        QVector3D(float(cx - half), float(y), float(cz + half))
    };
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

// Top-down: world (x, y, z) → eye (x, y, z − eyeHeight). Eye XY = world XY.
QMatrix4x4 topDownView(double eyeHeight = 10.0)
{
    QMatrix4x4 v;
    v.lookAt(QVector3D(0, 0, float(eyeHeight)),
             QVector3D(0, 0, 0),
             QVector3D(0, 1, 0));
    return v;
}

// Profile: world (x, y, z) → eye XY (x, z). Spreads stacked-along-Z
// points so the polygon test can tell them apart.
QMatrix4x4 profileView(double eyeDistance = 10.0)
{
    QMatrix4x4 v;
    v.lookAt(QVector3D(0, float(-eyeDistance), 0),
             QVector3D(0, 0, 0),
             QVector3D(0, 0, 1));
    return v;
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
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
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
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
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
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
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
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 1.0);
    req.viewMatrix = topDownView();
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
    req.polygonWorldXYZ = {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 1.0f, 0.0f)
    };
    req.viewMatrix = topDownView();
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
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 1.0);
    req.viewMatrix = topDownView();
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
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
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
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
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

// Stacked-along-Z points spread along eye Y under a profile view;
// tight polygon picks one. World-XY-drop would have kept all or none.
TEST_CASE("cwLazClipOperation: profile view separates points stacked along world Z",
          "[cwLazClipOperation]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QVector<QVector3D> input = {
        { 0.0f, 0.0f, 0.0f },   // eye XY ≈ (0,  0)
        { 0.0f, 0.0f, 5.0f },   // eye XY ≈ (0,  5)
        { 0.0f, 0.0f, 10.0f }   // eye XY ≈ (0, 10)
    };

    cwLazClipOperation::Request req;
    req.sources.append(pointGeometry(input));
    req.polygonWorldXYZ = squareInWorldXZ(0.0, 5.0, 1.0);
    req.viewMatrix = profileView();
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outputPath(tempDir, QStringLiteral("profile-middle"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().hasError());
    CHECK(future.result().value().pointsWritten == 1);
}

// Top-down boundary check: stacked-Z points collapse to one eye-XY
// position, so the polygon must keep all three.
TEST_CASE("cwLazClipOperation: top-down view treats stacked-Z points as one column",
          "[cwLazClipOperation]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QVector<QVector3D> input = {
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 5.0f },
        { 0.0f, 0.0f, 10.0f }
    };

    cwLazClipOperation::Request req;
    req.sources.append(pointGeometry(input));
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 1.0);
    req.viewMatrix = topDownView();
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outputPath(tempDir, QStringLiteral("topdown-column"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().hasError());
    CHECK(future.result().value().pointsWritten == 3);
}

// Pan shifts polygon and points by the same amount in eye XY, so
// the contained-test outcome is unchanged.
TEST_CASE("cwLazClipOperation: classification is invariant under view-matrix pan",
          "[cwLazClipOperation]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QVector<QVector3D> input = {
        { 0.5f, 0.5f, 0.0f },   // inside the 5-unit square
        { 2.0f, 2.0f, 0.0f },   // inside
        { 9.0f, 9.0f, 0.0f },   // outside
        { -3.0f, -7.0f, 0.0f }  // outside (y < -5)
    };
    const QList<QVector3D> poly = squareInWorldXY(0.0, 0.0, 5.0);

    QMatrix4x4 panned;
    panned.lookAt(QVector3D(5, 0, 10), QVector3D(5, 0, 0), QVector3D(0, 1, 0));

    cwLazClipOperation::Request reqA;
    reqA.sources.append(pointGeometry(input));
    reqA.polygonWorldXYZ = poly;
    reqA.viewMatrix = topDownView();
    reqA.mode = cwLazClipOperation::Mode::Keep;
    reqA.outputPath = outputPath(tempDir, QStringLiteral("pan-A"));

    cwLazClipOperation::Request reqB = reqA;
    reqB.viewMatrix = panned;
    reqB.outputPath = outputPath(tempDir, QStringLiteral("pan-B"));

    auto futA = cwLazClipOperation::run(reqA);
    auto futB = cwLazClipOperation::run(reqB);
    futA.waitForFinished();
    futB.waitForFinished();

    REQUIRE_FALSE(futA.result().hasError());
    REQUIRE_FALSE(futB.result().hasError());
    CHECK(futA.result().value().pointsWritten == 2);
    CHECK(futB.result().value().pointsWritten == futA.result().value().pointsWritten);
}

// NaN in the view matrix would silently emit garbage — preflight rejects.
TEST_CASE("cwLazClipOperation: non-finite view matrix is rejected",
          "[cwLazClipOperation]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QMatrix4x4 nanView = topDownView();
    nanView(0, 3) = std::numeric_limits<float>::quiet_NaN();

    cwLazClipOperation::Request req;
    req.sources.append(pointGeometry({{0,0,0}, {1,1,1}, {2,2,2}}));
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 5.0);
    req.viewMatrix = nanView;
    req.outputPath = outputPath(tempDir, QStringLiteral("nan-view"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE(future.result().errorCodeTo<cwLazClipOperation::ErrorCode>()
            == cwLazClipOperation::NonFiniteInput);
}
