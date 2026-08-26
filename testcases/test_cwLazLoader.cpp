#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QThread>

#include "cwGeoPoint.h"
#include "cwGeometry.h"
#include "cwLazLoader.h"

#include "LazFixtureHelper.h"

#include <LASlib/lasreader.hpp>

using Catch::Matchers::WithinAbs;

namespace {
// GeoTIFF key ids stored in a LAS file's VLR 34735.
constexpr quint16 kGeographicTypeGeoKey = 2048;
constexpr quint16 kProjectedCSTypeGeoKey = 3072;
constexpr quint16 kUserDefinedGeoCode = 32767;
// A key holds its value inline when it points at no other tag, and an inline
// key holds exactly one value.
constexpr quint16 kInlineTiffTagLocation = 0;
constexpr quint16 kInlineValueCount = 1;
// NAD83(CSRS) / UTM zone 10N — the CRS on the BC lidar tiles that only carry
// GeoKeys.
constexpr quint16 kNad83CsrsUtm10N = 3157;
constexpr quint16 kNad83Geographic = 4269;

const QByteArray kSampleWkt = QByteArrayLiteral("GEOGCS[\"WGS 84\"]");

LASvlr_key_entry inlineGeoKey(quint16 keyId, quint16 code)
{
    LASvlr_key_entry entry;
    entry.key_id = keyId;
    entry.tiff_tag_location = kInlineTiffTagLocation;
    entry.count = kInlineValueCount;
    entry.value_offset = code;
    return entry;
}

// kMinPointsPerWorker in cwLazLoader.cpp is 256 * 1024. To force multi-worker
// mode we need at least 2 * that, with the actual worker count capped by
// QThread::idealThreadCount() - 1. 600k is comfortably above the threshold and
// keeps synthesis quick (~100ms).
constexpr int kMultiWorkerPointCount = 600 * 1024;
} // namespace

TEST_CASE("cwLazLoader: empty source CS short-circuits to the file's own coordinates", "[cwLazLoader]") {
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

    auto future = cwLazLoader::load({.path = path});
    future.waitForFinished();
    REQUIRE(future.resultCount() == 1);
    cwLazLoadResult result = future.result();

    REQUIRE(result.geometry.type() == cwGeometry::Type::Points);
    REQUIRE(result.geometry.attribute(cwGeometry::Semantic::Position) != nullptr);
    REQUIRE(result.geometry.vertexCount() == input.size());

    QVector<QVector3D> out =
        result.geometry.values<QVector3D>(cwGeometry::Semantic::Position);
    REQUIRE(out.size() == input.size());

    // With no destination frame to transform into, the points arrive exactly
    // as the file holds them.
    REQUIRE_THAT(out[0].x(), WithinAbs(100.0f, 1e-3f));
    REQUIRE_THAT(out[0].y(), WithinAbs(200.0f, 1e-3f));
    REQUIRE_THAT(out[0].z(), WithinAbs(50.0f, 1e-3f));
    REQUIRE_THAT(out[1].x(), WithinAbs(110.0f, 1e-3f));
    REQUIRE_THAT(out[1].y(), WithinAbs(210.0f, 1e-3f));
    REQUIRE_THAT(out[1].z(), WithinAbs(55.0f, 1e-3f));

    REQUIRE(result.sourceCS.isEmpty());
}

TEST_CASE("cwLazLoader: bbox tracks min/max of the loaded points", "[cwLazLoader]") {
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

TEST_CASE("cwLazLoader: GeoTIFF GeoKeys name the source CS when the WKT VLR is absent",
          "[cwLazLoader][geokeys]") {
    SECTION("ProjectedCSTypeGeoKey resolves to an EPSG code") {
        LASheader header;
        const LASvlr_key_entry keys[] = { inlineGeoKey(kProjectedCSTypeGeoKey, kNad83CsrsUtm10N) };
        REQUIRE(header.set_geo_keys(1, keys));

        REQUIRE(cwLazLoader::resolveSourceCS(QString(), header)
                == QStringLiteral("EPSG:3157"));
    }

    SECTION("GeographicTypeGeoKey is the fallback when no projected key is present") {
        LASheader header;
        const LASvlr_key_entry keys[] = { inlineGeoKey(kGeographicTypeGeoKey, kNad83Geographic) };
        REQUIRE(header.set_geo_keys(1, keys));

        REQUIRE(cwLazLoader::resolveSourceCS(QString(), header)
                == QStringLiteral("EPSG:4269"));
    }

    SECTION("A projected key wins over a geographic key in the same file") {
        LASheader header;
        const LASvlr_key_entry keys[] = {
            inlineGeoKey(kGeographicTypeGeoKey, kNad83Geographic),
            inlineGeoKey(kProjectedCSTypeGeoKey, kNad83CsrsUtm10N)
        };
        REQUIRE(header.set_geo_keys(2, keys));

        REQUIRE(cwLazLoader::resolveSourceCS(QString(), header)
                == QStringLiteral("EPSG:3157"));
    }

    SECTION("A user-defined code names no EPSG CRS, so the source CS stays empty") {
        LASheader header;
        const LASvlr_key_entry keys[] = { inlineGeoKey(kProjectedCSTypeGeoKey, kUserDefinedGeoCode) };
        REQUIRE(header.set_geo_keys(1, keys));

        REQUIRE(cwLazLoader::resolveSourceCS(QString(), header).isEmpty());
    }

    SECTION("A header with no CRS at all stays empty") {
        LASheader header;
        REQUIRE(cwLazLoader::resolveSourceCS(QString(), header).isEmpty());
    }

    SECTION("The OGC WKT VLR wins over the GeoKeys") {
        LASheader header;
        const LASvlr_key_entry keys[] = { inlineGeoKey(kProjectedCSTypeGeoKey, kNad83CsrsUtm10N) };
        REQUIRE(header.set_geo_keys(1, keys));
        header.set_geo_ogc_wkt(kSampleWkt.size(), kSampleWkt.constData());

        REQUIRE(cwLazLoader::resolveSourceCS(QString(), header)
                == QString::fromLatin1(kSampleWkt));
    }

    SECTION("An explicit override wins over both") {
        LASheader header;
        const LASvlr_key_entry keys[] = { inlineGeoKey(kProjectedCSTypeGeoKey, kNad83CsrsUtm10N) };
        REQUIRE(header.set_geo_keys(1, keys));
        header.set_geo_ogc_wkt(kSampleWkt.size(), kSampleWkt.constData());

        REQUIRE(cwLazLoader::resolveSourceCS(QStringLiteral("EPSG:26910"), header)
                == QStringLiteral("EPSG:26910"));
    }
}
