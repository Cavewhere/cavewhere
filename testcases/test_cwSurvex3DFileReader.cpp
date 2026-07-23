//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwSurvex3DFileReader.h"
#include "cwCavernRunner.h"
#include "cwCavernNaming.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QFileInfo>
#include <QTemporaryDir>

TEST_CASE("cwSurvex3DFileReader should return empty lookup for missing file", "[cwSurvex3DFileReader]") {
    cwSurvex3DFileReader reader;
    auto lookup = reader.readStationPositions("/nonexistent/file.3d");
    CHECK(lookup.isEmpty());
}

TEST_CASE("cwSurvex3DFileReader should read station positions from .3d file", "[cwSurvex3DFileReader]") {
    // Run cavern to produce a .3d file from the test dataset
    QString survexDataFile = testcasesDatasetPath("test_cwSurvexport/data.svx");
    REQUIRE(QFile::exists(survexDataFile));

    const QString output3dPath = survexDataFile + QStringLiteral(".3d");
    auto cavernResult = cwCavernRunner::run(survexDataFile, output3dPath);

    REQUIRE_FALSE(cavernResult.hasError());
    REQUIRE(QFileInfo(cavernResult.value().output3dPath).exists());

    // Read station positions directly from the .3d file
    cwSurvex3DFileReader reader;
    cwStationPositionLookup lookup = reader.readStationPositions(cavernResult.value().output3dPath);

    CHECK(!lookup.isEmpty());

    // The test dataset has 6 stations: 26, 26a-26e
    // Expected positions match the existing cwSurvexportCSVTask test dataset
    auto positions = lookup.positions();
    CHECK(positions.size() == 6);

    // Verify known station positions (station 26 is fixed at origin)
    CHECK(lookup.hasPosition("26"));
    QVector3D pos26 = lookup.position("26");
    CHECK(pos26.x() == Catch::Approx(0.0).margin(0.01));
    CHECK(pos26.y() == Catch::Approx(0.0).margin(0.01));
    CHECK(pos26.z() == Catch::Approx(0.0).margin(0.01));

    // Station 26a should be nearby with non-zero coordinates
    CHECK(lookup.hasPosition("26a"));
    QVector3D pos26a = lookup.position("26a");
    CHECK(pos26a.x() == Catch::Approx(0.93).margin(0.01));
    CHECK(pos26a.y() == Catch::Approx(-5.40).margin(0.01));
    CHECK(pos26a.z() == Catch::Approx(2.32).margin(0.01));
}

TEST_CASE("cwSurvex3DFileReader should normalize a non-dot survex separator", "[cwSurvex3DFileReader]") {
    // Regression: a "*alias station - .." (as external TopoDroid files carry)
    // makes '.' a name character, so cavern picks ':' as the label separator.
    // cwCavernNaming and every decode consumer assume '.', so the reader must
    // normalize the emitted labels back to '.'. Without normalization the
    // labels arrive as "cave_<hex>:trip_<hex>:holberg1:1" and stationRegex()
    // fails to match, dropping every station from the solve.
    // Read from the source tree (not copyToTempFolder) so the sibling
    // separator_walls.srv the *include pulls in resolves. cavern only reads the
    // .svx; the .3d goes to a private temp dir below, so this stays safe to run
    // as concurrent processes.
    QString survexDataFile = testcasesDatasetSourcePath("test_cwSurvex3DFileReader/separator_alias.svx");
    REQUIRE(QFile::exists(survexDataFile));

    // Write the .3d into a private temp dir so concurrent test processes don't
    // clobber a shared output next to the source dataset.
    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());
    const QString output3dPath = workDir.filePath(QStringLiteral("separator_alias.3d"));

    auto cavernResult = cwCavernRunner::run(survexDataFile, output3dPath);
    REQUIRE_FALSE(cavernResult.hasError());
    REQUIRE(QFileInfo(cavernResult.value().output3dPath).exists());

    cwSurvex3DFileReader reader;
    cwStationPositionLookup lookup = reader.readStationPositions(cavernResult.value().output3dPath);

    CHECK(!lookup.isEmpty());

    // No label may keep the raw ':' separator, and every cave-prefixed label
    // must decode through the cavern-naming grammar. (The Walls include emits
    // its own top-level stations without a cave_ prefix; they only need to be
    // separator-normalized, not decodable.)
    const QRegularExpression stationRegex = cwCavernNaming::stationRegex();
    const QMap<QString, QVector3D> positions = lookup.positions();
    for (auto iter = positions.constBegin(); iter != positions.constEnd(); ++iter) {
        const QString& name = iter.key();
        CHECK_FALSE(name.contains(QLatin1Char(':')));
        if (name.startsWith(QLatin1String("cave_"))) {
            CHECK(stationRegex.match(name).hasMatch());
        }
    }

    // The fully-qualified, dot-normalized station is resolvable.
    CHECK(lookup.hasPosition(
        "cave_f5881643adce48de9d5fe2224c217a86.trip_6f7da06a8b8c471cbfbefe23b99e4fd4.holberg1.1"));
}

TEST_CASE("cwSurvex3DFileReader should build a survey network from .3d file", "[cwSurvex3DFileReader]") {
    QString survexDataFile = testcasesDatasetPath("test_cwSurvexport/data.svx");
    REQUIRE(QFile::exists(survexDataFile));

    const QString output3dPath = survexDataFile + QStringLiteral(".3d");
    auto cavernResult = cwCavernRunner::run(survexDataFile, output3dPath);

    REQUIRE_FALSE(cavernResult.hasError());
    REQUIRE(QFileInfo(cavernResult.value().output3dPath).exists());

    cwSurvex3DFileReader reader;
    auto parsed = reader.readNetworkAndLookup(cavernResult.value().output3dPath);

    // Lookup and network positions must agree for every station in the file.
    CHECK(parsed.lookup.positions().size() == 6);
    CHECK(parsed.network.stations().size() == 6);
    for (const auto &name : parsed.network.stations()) {
        CHECK(parsed.lookup.hasPosition(name));
        CHECK(parsed.network.hasPosition(name));
    }

    // Survex data.svx connects five shots: 26–26a, 26a–26b, 26c–26b,
    // 26d–26c, 26c–26e. Station 26c therefore has three neighbours.
    REQUIRE(parsed.network.hasPosition("26c"));
    auto neighbors26c = parsed.network.neighbors("26c");
    CHECK(neighbors26c.size() == 3);
    CHECK(neighbors26c.contains("26b"));
    CHECK(neighbors26c.contains("26d"));
    CHECK(neighbors26c.contains("26e"));

    // Station 26e is a leaf.
    auto neighbors26e = parsed.network.neighbors("26e");
    CHECK(neighbors26e.size() == 1);
    CHECK(neighbors26e.contains("26c"));
}
