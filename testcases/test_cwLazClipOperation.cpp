#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include <QCoreApplication>
#include <QFile>
#include <QList>
#include <QMatrix4x4>
#include <QTemporaryDir>
#include <QVector3D>

#include "cwGeoPoint.h"
#include "cwLazClipOperation.h"
#include "cwLazLoader.h"

#include "LazFixtureHelper.h"

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

QString outputPath(const QTemporaryDir& dir, const QString& stem)
{
    return dir.filePath(stem + QStringLiteral(".laz"));
}

// Streaming sources live on disk in absolute (output-CS) coords. The clip's
// eye-XY test is run in worldOrigin-relative space, so a source built from
// worldOrigin-relative points must add worldOrigin back when writing the file.
cwLazClipSource sourceFromRelative(QTemporaryDir& dir, const QString& tag,
                                   const QVector<QVector3D>& relPoints,
                                   const cwGeoPoint& worldOrigin = cwGeoPoint(0, 0, 0))
{
    QVector<QVector3D> absPoints;
    absPoints.reserve(relPoints.size());
    for (const QVector3D& p : relPoints) {
        absPoints.append(QVector3D(float(double(p.x()) + worldOrigin.x),
                                   float(double(p.y()) + worldOrigin.y),
                                   float(double(p.z()) + worldOrigin.z)));
    }
    const QString path = tempLazPath(dir, tag);
    REQUIRE(writeSyntheticLazFile(path, absPoints));
    return { path, QString() };
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
    req.sources.append(sourceFromRelative(tempDir, QStringLiteral("clip-keep-in"), input));
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
    req.sources.append(sourceFromRelative(tempDir, QStringLiteral("clip-rm-in"), input));
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
    req.mode = cwLazClipOperation::Mode::Remove;
    req.outputPath = outputPath(tempDir, QStringLiteral("clip-rm-out"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().hasError());
    REQUIRE(future.result().value().pointsWritten == 2);
}

TEST_CASE("cwLazClipOperation: missing source file is rejected", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazClipOperation::Request req;
    req.sources.append({ tempDir.filePath(QStringLiteral("does-not-exist.laz")), QString() });
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
    req.outputPath = outputPath(tempDir, QStringLiteral("missing-out"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    const auto result = future.result();
    REQUIRE(result.hasError());
    REQUIRE(result.errorCodeTo<cwLazClipOperation::ErrorCode>()
            == cwLazClipOperation::SourceOpenFailed);
    // Probe fails before the writer opens — no partial output left behind.
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
    req.sources.append(sourceFromRelative(tempDir, QStringLiteral("poly2-in"), {{0,0,0}, {1,1,1}}));
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
    req.sources.append(sourceFromRelative(tempDir, QStringLiteral("nan-in"), {{0,0,0}, {1,1,1}, {2,2,2}}));
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
    req.sources.append(sourceFromRelative(tempDir, QStringLiteral("u-a"), {{0,0,0}, {2,2,2}, {100,100,0}}));
    req.sources.append(sourceFromRelative(tempDir, QStringLiteral("u-b"), {{3,3,3}, {4,4,4}, {200,200,0}}));
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

    // Source on disk holds absolute coords (local + worldOrigin). The clip
    // re-anchors the output on the same worldOrigin, so reloading via
    // cwLazLoader with that origin returns the original local coordinates.
    const cwGeoPoint worldOrigin(1000.0, 2000.0, 0.0);
    const QVector<QVector3D> input = {
        { 0.0f, 0.0f, 0.0f },    // inside polygon at local origin
        { 5.0f, 5.0f, 0.0f },    // inside
        { 500.0f, 500.0f, 0.0f } // outside
    };
    const QString outPath = outputPath(tempDir, QStringLiteral("origin-out"));

    cwLazClipOperation::Request req;
    req.sources.append(sourceFromRelative(tempDir, QStringLiteral("origin-in"), input, worldOrigin));
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
    req.worldOrigin = worldOrigin;
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outPath;

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().hasError());
    REQUIRE(future.result().value().pointsWritten == 2);

    auto loadFuture = cwLazLoader::load({
        .path = outPath,
        .worldOrigin = worldOrigin
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
    req.sources.append(sourceFromRelative(tempDir, QStringLiteral("profile-in"), input));
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
    req.sources.append(sourceFromRelative(tempDir, QStringLiteral("topdown-in"), input));
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
    reqA.sources.append(sourceFromRelative(tempDir, QStringLiteral("pan-in"), input));
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
    req.sources.append(sourceFromRelative(tempDir, QStringLiteral("nan-view-in"), {{0,0,0}, {1,1,1}, {2,2,2}}));
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 5.0);
    req.viewMatrix = nanView;
    req.outputPath = outputPath(tempDir, QStringLiteral("nan-view"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE(future.result().errorCodeTo<cwLazClipOperation::ErrorCode>()
            == cwLazClipOperation::NonFiniteInput);
}

// Streaming from disk gives the worker the full LAS record, so a kept point
// carries its intensity / classification / RGB / GPS time / source ID into the
// output unchanged — only XYZ is re-encoded.
TEST_CASE("cwLazClipOperation: kept points preserve their source attributes",
          "[cwLazClipOperation]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QVector<LazAttributePoint> input;
    LazAttributePoint a;
    a.position = QVector3D(1.0f, 1.0f, 0.0f); // inside
    a.intensity = 1234;
    a.classification = 2; // ground
    a.red = 100; a.green = 200; a.blue = 300;
    a.gpsTime = 86400.5;
    a.pointSourceId = 7;
    input.append(a);

    LazAttributePoint b;
    b.position = QVector3D(-2.0f, 3.0f, 0.0f); // inside
    b.intensity = 4321;
    b.classification = 5; // high vegetation
    b.red = 4000; b.green = 5000; b.blue = 6000;
    b.gpsTime = 90000.25;
    b.pointSourceId = 9;
    input.append(b);

    LazAttributePoint outside;
    outside.position = QVector3D(500.0f, 500.0f, 0.0f); // outside
    outside.classification = 1;
    input.append(outside);

    const QString srcPath = tempLazPath(tempDir, QStringLiteral("attr-in"));
    REQUIRE(writeAttributedLazFile(srcPath, input, 3));

    const QString outPath = outputPath(tempDir, QStringLiteral("attr-out"));
    cwLazClipOperation::Request req;
    req.sources.append({ srcPath, QString() });
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outPath;

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().hasError());
    REQUIRE(future.result().value().pointsWritten == 2);

    const LazFileContents out = readLazFile(outPath);
    CHECK(out.pointDataFormat == 3); // GPS time + RGB carried over
    const QVector<LazAttributePoint>& kept = out.points;

    // The output header bounding box must bound the kept points (a at (1,1,0),
    // b at (-2,3,0)), not be left at the all-zero default.
    CHECK(out.headerBboxMin.x() == Catch::Approx(-2.0).margin(0.001));
    CHECK(out.headerBboxMin.y() == Catch::Approx(1.0).margin(0.001));
    CHECK(out.headerBboxMax.x() == Catch::Approx(1.0).margin(0.001));
    CHECK(out.headerBboxMax.y() == Catch::Approx(3.0).margin(0.001));

    REQUIRE(kept.size() == 2);
    // Kept points keep their input order under keep mode.
    CHECK(kept[0].intensity == 1234);
    CHECK(kept[0].classification == 2);
    CHECK(kept[0].red == 100);
    CHECK(kept[0].green == 200);
    CHECK(kept[0].blue == 300);
    CHECK(kept[0].gpsTime == Catch::Approx(86400.5));
    CHECK(kept[0].pointSourceId == 7);
    CHECK(kept[0].position.x() == Catch::Approx(1.0).margin(0.001));

    CHECK(kept[1].intensity == 4321);
    CHECK(kept[1].classification == 5);
    CHECK(kept[1].red == 4000);
    CHECK(kept[1].gpsTime == Catch::Approx(90000.25));
    CHECK(kept[1].pointSourceId == 9);
}

// Point formats 6–10 are LAS 1.4 only. The output header must be stamped 1.4
// so the 64-bit extended point count is written — a 1.2 header drops it, and
// every reader (cwLazLoader, CloudCompare) then sees an empty cloud even though
// the point records are on disk.
TEST_CASE("cwLazClipOperation: extended (1.4) format output records its point count",
          "[cwLazClipOperation]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QVector<LazAttributePoint> input;
    LazAttributePoint a;
    a.position = QVector3D(1.0f, 1.0f, 0.0f); // inside
    a.classification = 2;
    a.gpsTime = 86400.5;
    input.append(a);

    LazAttributePoint b;
    b.position = QVector3D(-2.0f, 3.0f, 0.0f); // inside
    b.classification = 5;
    b.gpsTime = 90000.25;
    input.append(b);

    LazAttributePoint outside;
    outside.position = QVector3D(500.0f, 500.0f, 0.0f); // outside
    input.append(outside);

    const QString srcPath = tempLazPath(tempDir, QStringLiteral("ext-in"));
    REQUIRE(writeAttributedLazFile(srcPath, input, 6));

    const QString outPath = outputPath(tempDir, QStringLiteral("ext-out"));
    cwLazClipOperation::Request req;
    req.sources.append({ srcPath, QString() });
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outPath;

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().hasError());
    REQUIRE(future.result().value().pointsWritten == 2);

    const LazFileContents out = readLazFile(outPath);
    CHECK(out.pointDataFormat == 6);
    CHECK(out.points.size() == 2);

    // The real regression: cwLazLoader reads the header point count. A 1.2
    // header would report 0 here despite the records existing on disk.
    auto loadFuture = cwLazLoader::load({.path = outPath});
    loadFuture.waitForFinished();
    REQUIRE(loadFuture.resultCount() == 1);
    CHECK(loadFuture.result().geometry.vertexCount() == 2);
}

// Mixed source formats merge into the richest format (here 3): the format-0
// source upconverts (its missing attributes default to 0) and the format-3
// source's attributes survive.
TEST_CASE("cwLazClipOperation: mixed source formats merge into the richest format",
          "[cwLazClipOperation]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Format-3 source with RGB, one point inside.
    LazAttributePoint rich;
    rich.position = QVector3D(-1.0f, -1.0f, 0.0f);
    rich.red = 1111; rich.green = 2222; rich.blue = 3333;
    rich.classification = 2;
    const QString richPath = tempLazPath(tempDir, QStringLiteral("mix-rich"));
    REQUIRE(writeAttributedLazFile(richPath, { rich }, 3));

    // Format-0 source (XYZ only), one point inside.
    const QString plainPath = tempLazPath(tempDir, QStringLiteral("mix-plain"));
    REQUIRE(writeSyntheticLazFile(plainPath, { QVector3D(1.0f, 1.0f, 0.0f) }));

    const QString outPath = outputPath(tempDir, QStringLiteral("mix-out"));
    cwLazClipOperation::Request req;
    // Rich source first so the format-0 point is written AFTER a format-3
    // point: without clearing the merged point's optional fields it would
    // inherit the rich point's RGB.
    req.sources.append({ richPath, QString() });
    req.sources.append({ plainPath, QString() });
    req.polygonWorldXYZ = squareInWorldXY(0.0, 0.0, 10.0);
    req.viewMatrix = topDownView();
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outPath;

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().hasError());
    REQUIRE(future.result().value().pointsWritten == 2);

    const LazFileContents out = readLazFile(outPath);
    CHECK(out.pointDataFormat == 3);
    REQUIRE(out.points.size() == 2);

    // Identify the two points by position (the merge re-encodes XYZ but not
    // far enough to move them): the rich point keeps its RGB, the plain point
    // must come out with RGB 0 — not the rich point's bled-over color.
    auto pointNear = [&](float x, float y) -> LazAttributePoint {
        for (const LazAttributePoint& p : out.points) {
            if (std::abs(p.position.x() - x) < 0.01f
                && std::abs(p.position.y() - y) < 0.01f) {
                return p;
            }
        }
        FAIL("expected a clipped point near (" << x << ", " << y << ")");
        return {};
    };

    const LazAttributePoint richOut = pointNear(-1.0f, -1.0f);
    CHECK(richOut.red == 1111);
    CHECK(richOut.green == 2222);
    CHECK(richOut.blue == 3333);

    const LazAttributePoint plainOut = pointNear(1.0f, 1.0f);
    CHECK(plainOut.red == 0);
    CHECK(plainOut.green == 0);
    CHECK(plainOut.blue == 0);
}
