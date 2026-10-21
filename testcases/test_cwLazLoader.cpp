#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "cwGeoPoint.h"
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

constexpr double kProbeTolerance = 1e-3;

LASvlr_key_entry inlineGeoKey(quint16 keyId, quint16 code)
{
    LASvlr_key_entry entry;
    entry.key_id = keyId;
    entry.tiff_tag_location = kInlineTiffTagLocation;
    entry.count = kInlineValueCount;
    entry.value_offset = code;
    return entry;
}
} // namespace

TEST_CASE("cwLazLoader: the probe reads the header bbox in the file's own CS",
          "[cwLazLoader]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QVector<QVector3D> input = {
        { 0.0f, 0.0f, 0.0f },
        { 100.0f, 0.0f, -10.0f },
        { 50.0f, 80.0f, 20.0f },
        { -5.0f, 25.0f, 5.0f }
    };
    const QString path = tempLazPath(tempDir, QStringLiteral("probe-bbox"));
    REQUIRE(writeSyntheticLazFile(path, input));

    const cwLazLoader::ProbeResult probe = cwLazLoader::probeHeader(path);
    REQUIRE(probe.valid);
    CHECK(probe.sourceCS.isEmpty());
    CHECK_THAT(probe.bboxMin.x, WithinAbs(-5.0, kProbeTolerance));
    CHECK_THAT(probe.bboxMin.y, WithinAbs(0.0, kProbeTolerance));
    CHECK_THAT(probe.bboxMin.z, WithinAbs(-10.0, kProbeTolerance));
    CHECK_THAT(probe.bboxMax.x, WithinAbs(100.0, kProbeTolerance));
    CHECK_THAT(probe.bboxMax.y, WithinAbs(80.0, kProbeTolerance));
    CHECK_THAT(probe.bboxMax.z, WithinAbs(20.0, kProbeTolerance));
}

TEST_CASE("cwLazLoader: the probe reports the LAZ's embedded CRS",
          "[cwLazLoader]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString wkt = utmZoneWkt(10, -123);
    const QVector<QVector3D> input = { { 500000.0f, 4000000.0f, 100.0f } };
    const QString path = tempLazPath(tempDir, QStringLiteral("probe-crs"));
    REQUIRE(writeSyntheticLazFile(path, input, wkt));

    const cwLazLoader::ProbeResult probe = cwLazLoader::probeHeader(path);
    REQUIRE(probe.valid);
    CHECK(probe.sourceCS == wkt);
}

TEST_CASE("cwLazLoader: probing a missing file reports it as invalid",
          "[cwLazLoader]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("does-not-exist-%1.laz")
                                              .arg(QCoreApplication::applicationPid()));

    const cwLazLoader::ProbeResult probe = cwLazLoader::probeHeader(path);
    CHECK_FALSE(probe.valid);
    CHECK(probe.sourceCS.isEmpty());
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
