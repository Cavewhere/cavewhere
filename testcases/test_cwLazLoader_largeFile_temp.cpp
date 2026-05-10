// TEMP REPRODUCTION TEST — delete once the underlying crash is fixed.
//
// User reported a crash opening a real-world ~1.8GB LAZ file:
//   /Users/cave/Downloads/bc_092f031_3_4_2_xyes_8_utm10_20240204_20240204.laz
// This test calls cwLazLoader::load() with that exact path and waits for the
// future to settle. If the file is missing on a CI/peer machine the test is
// skipped, so the suite stays green for everyone except the user reproducing
// the bug.

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QFileInfo>

#include "cwGeoPoint.h"
#include "cwLazLoader.h"

namespace {
constexpr const char* kRealLargeLazPath =
    "/Users/cave/Downloads/bc_092f031_3_4_2_xyes_8_utm10_20240204_20240204.laz";
}

TEST_CASE("cwLazLoader: real 1.8GB UTM10 LAZ file does not crash",
          "[cwLazLoader][largefile]")
{
    const QFileInfo info(QString::fromLatin1(kRealLargeLazPath));
    if (!info.exists()) {
        SKIP("Reproduction file not present on this machine: "
             << kRealLargeLazPath);
    }

    // No CS override, no globalCS, no worldOrigin — exercise the same path
    // that triggers when a user adds the file to a fresh project.
    cwLazLoader::Request req;
    req.path = QString::fromLatin1(kRealLargeLazPath);
    // Cap so this test can run quickly even without the crash.
    // Set to -1 to exercise the full file (matches the crashing path).
    req.maxPoints = -1;

    auto future = cwLazLoader::load(req);
    future.waitForFinished();

    REQUIRE(future.resultCount() == 1);
    const auto result = future.result();
    INFO("vertexCount=" << result.geometry.vertexCount()
         << " sourceCS=" << result.sourceCS.toStdString());
    REQUIRE(result.geometry.vertexCount() > 0);
}
