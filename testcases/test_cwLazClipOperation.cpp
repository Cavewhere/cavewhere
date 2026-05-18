#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QPolygonF>
#include <QTemporaryDir>

#include "cwGeoPoint.h"
#include "cwLazClipOperation.h"
#include "cwLazLoader.h"

#include "LazFixtureHelper.h"

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
    const QString sourcePath = tempLazPath(tempDir, QStringLiteral("clip-keep-in"));
    REQUIRE(writeSyntheticLazFile(sourcePath, input));
    const QString outPath = tempLazPath(tempDir, QStringLiteral("clip-keep-out"));

    cwLazClipOperation::Request req;
    req.sources.append({ sourcePath, QString() });
    req.polygonLocalXY = squareAround(0.0, 0.0, 10.0);
    req.worldOrigin = cwGeoPoint(0.0, 0.0, 0.0);
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outPath;

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE(future.resultCount() == 1);
    const auto result = future.result();
    REQUIRE(result.success);
    REQUIRE(result.pointsWritten == 2);
    REQUIRE(result.outputPath == outPath);

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
    const QString sourcePath = tempLazPath(tempDir, QStringLiteral("clip-rm-in"));
    REQUIRE(writeSyntheticLazFile(sourcePath, input));
    const QString outPath = tempLazPath(tempDir, QStringLiteral("clip-rm-out"));

    cwLazClipOperation::Request req;
    req.sources.append({ sourcePath, QString() });
    req.polygonLocalXY = squareAround(0.0, 0.0, 10.0);
    req.mode = cwLazClipOperation::Mode::Remove;
    req.outputPath = outPath;

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE(future.result().success);
    REQUIRE(future.result().pointsWritten == 2);
}

TEST_CASE("cwLazClipOperation: empty source list fails fast", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazClipOperation::Request req;
    req.polygonLocalXY = squareAround(0.0, 0.0, 1.0);
    req.outputPath = tempLazPath(tempDir, QStringLiteral("empty"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().success);
    REQUIRE_FALSE(future.result().errorMessage.isEmpty());
    REQUIRE(future.result().pointsWritten == 0);
}

TEST_CASE("cwLazClipOperation: polygon with fewer than 3 vertices is rejected", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString sourcePath = tempLazPath(tempDir, QStringLiteral("poly2-in"));
    REQUIRE(writeSyntheticLazFile(sourcePath, {{0,0,0}, {1,1,1}}));

    cwLazClipOperation::Request req;
    req.sources.append({ sourcePath, QString() });
    QPolygonF degenerate;
    degenerate << QPointF(0, 0) << QPointF(1, 1);
    req.polygonLocalXY = degenerate;
    req.outputPath = tempLazPath(tempDir, QStringLiteral("poly2-out"));

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE_FALSE(future.result().success);
}

TEST_CASE("cwLazClipOperation: unions points from multiple sources into one output", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString srcA = tempLazPath(tempDir, QStringLiteral("u-a"));
    const QString srcB = tempLazPath(tempDir, QStringLiteral("u-b"));
    REQUIRE(writeSyntheticLazFile(srcA, {{0,0,0}, {2,2,2}, {100,100,0}}));
    REQUIRE(writeSyntheticLazFile(srcB, {{3,3,3}, {4,4,4}, {200,200,0}}));

    const QString outPath = tempLazPath(tempDir, QStringLiteral("u-out"));
    cwLazClipOperation::Request req;
    req.sources.append({ srcA, QString() });
    req.sources.append({ srcB, QString() });
    req.polygonLocalXY = squareAround(0.0, 0.0, 10.0);
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outPath;

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE(future.result().success);
    REQUIRE(future.result().pointsWritten == 4); // 2 from A + 2 from B
}

TEST_CASE("cwLazClipOperation: polygon test uses worldOrigin-relative coords", "[cwLazClipOperation]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Source-CS absolute coords; only the first two land inside a small
    // polygon at the LOCAL origin once worldOrigin (1000,2000) is applied.
    const QVector<QVector3D> input = {
        { 1000.0f, 2000.0f, 0.0f },    // local (0,0)
        { 1005.0f, 2005.0f, 0.0f },    // local (5,5)
        { 1500.0f, 2500.0f, 0.0f }     // local (500,500)
    };
    const QString sourcePath = tempLazPath(tempDir, QStringLiteral("origin-in"));
    REQUIRE(writeSyntheticLazFile(sourcePath, input));
    const QString outPath = tempLazPath(tempDir, QStringLiteral("origin-out"));

    cwLazClipOperation::Request req;
    req.sources.append({ sourcePath, QString() });
    req.polygonLocalXY = squareAround(0.0, 0.0, 10.0);
    req.worldOrigin = cwGeoPoint(1000.0, 2000.0, 0.0);
    req.mode = cwLazClipOperation::Mode::Keep;
    req.outputPath = outPath;

    auto future = cwLazClipOperation::run(req);
    future.waitForFinished();
    REQUIRE(future.result().success);
    REQUIRE(future.result().pointsWritten == 2);
}
