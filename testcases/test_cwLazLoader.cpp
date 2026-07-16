#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QThread>

#include "cwGeoPoint.h"
#include "cwGeometry.h"
#include "cwLazLoader.h"

#include "LazFixtureHelper.h"

using Catch::Matchers::WithinAbs;

namespace {
// kMinPointsPerWorker in cwLazLoader.cpp is 256 * 1024. To force multi-worker
// mode we need at least 2 * that, with the actual worker count capped by
// QThread::idealThreadCount() - 1. 600k is comfortably above the threshold and
// keeps synthesis quick (~100ms).
constexpr int kMultiWorkerPointCount = 600 * 1024;
} // namespace

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

// Reproduces the cancel-mid-multi-worker crash at cwLazLoader.cpp:388 where
// mapFuture.results() can return fewer items than ranges if cancellation
// prevents some mapped tasks from running. The original workflow:
//   1. Add a multi-million-point USGS LAZ.
//   2. Disable the layer mid-load (or via .cwlaz on re-rescan).
//   3. cwLazLayer::setEnabled(false) cancels m_loadRestarter.future().
//   4. cwLazLoader's polling loop calls mapFuture.cancel(); pending workers
//      never run; mapFuture.results().size() < ranges.size().
//   5. The for (i = 0; i < ranges.size(); ++i) loop then indexes
//      workerResults[i] past the end → Q_ASSERT abort.
TEST_CASE("cwLazLoader: cancel during multi-worker load does not crash",
          "[cwLazLoader][cancel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QVector<QVector3D> points;
    points.reserve(kMultiWorkerPointCount);
    for (int i = 0; i < kMultiWorkerPointCount; ++i) {
        points.append(QVector3D(float(i % 1000),
                                float((i / 1000) % 1000),
                                float(i & 0xff) * 0.1f));
    }
    const QString path = tempLazPath(tempDir, QStringLiteral("cancel-race"));
    REQUIRE(writeSyntheticLazFile(path, points));

    // Cancel as soon as the future exists. The race we want to lose: cancel
    // arrives at the polling loop before all mapped workers have started, so
    // mapFuture.results() comes back shorter than ranges.
    auto future = cwLazLoader::load({.path = path, .maxPoints = -1});
    future.cancel();
    future.waitForFinished();

    // No assertion on result shape — the contract under cancel is "don't
    // crash"; cwLazLayer's observer drops the result anyway when m_enabled
    // is false. This test passes iff cwLazLoader returned without aborting.
    SUCCEED();
}
