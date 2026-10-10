//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwSurvex3DFileReader.h"
#include "cwCavernRunner.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

TEST_CASE("cwSurvex3DFileReader should return empty lookup for missing file", "[cwSurvex3DFileReader]") {
    cwSurvex3DFileReader reader;
    auto lookup = reader.readNetworkAndLookup("/nonexistent/file.3d").lookup;
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
    cwStationPositionLookup lookup =
        reader.readNetworkAndLookup(cavernResult.value().output3dPath).lookup;

    CHECK(!lookup.isEmpty());

    // The test dataset has 6 stations: 26, 26a-26e
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

TEST_CASE("cwSurvex3DFileReader keys a leg the same in either direction", "[cwSurvex3DFileReader]") {
    CHECK(cwSurvex3DFileReader::legKey(QStringLiteral("A2"), QStringLiteral("a1"))
          == cwSurvex3DFileReader::legKey(QStringLiteral("a1"), QStringLiteral("a2")));
}

TEST_CASE("cwSurvex3DFileReader reads the loop closure error of each loop leg", "[cwSurvex3DFileReader]") {
    // A misclosed triangle a1->a2->a3->a1 with a dead end a3->a4 hanging off it.
    // Cavern writes one img_ERROR_INFO for the loop's traverse and none for
    // the dead end.
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const QString svxPath = dir.filePath(QStringLiteral("loop.svx"));
    const QString output3dPath = dir.filePath(QStringLiteral("loop.3d"));

    QFile svx(svxPath);
    REQUIRE(svx.open(QIODevice::WriteOnly));
    svx.write("*fix a1 0 0 0\n"
              "*data normal from to tape compass clino\n"
              "a1 a2 10.0 0.0 0.0\n"
              "a2 a3 10.0 90.0 0.0\n"
              "a3 a1 15.0 225.0 0.0\n"
              "a3 a4 5.0 90.0 0.0\n");
    svx.close();

    const auto cavernResult = cwCavernRunner::run(svxPath, output3dPath);
    REQUIRE_FALSE(cavernResult.hasError());

    cwSurvex3DFileReader reader;
    const auto parsed = reader.readNetworkAndLookup(cavernResult.value().output3dPath);

    const auto key = [](const char* from, const char* to) {
        return cwSurvex3DFileReader::legKey(QString::fromLatin1(from), QString::fromLatin1(to));
    };

    CHECK(parsed.legErrors.size() == 3);
    REQUIRE(parsed.legErrors.contains(key("a1", "a2")));
    REQUIRE(parsed.legErrors.contains(key("a2", "a3")));
    REQUIRE(parsed.legErrors.contains(key("a3", "a1")));
    CHECK_FALSE(parsed.legErrors.contains(key("a3", "a4")));

    const cwLegError error = parsed.legErrors.value(key("a1", "a2"));
    CHECK(error.e > 0.0f);
    CHECK(parsed.legErrors.value(key("a2", "a3")).e == error.e);
    CHECK(parsed.legErrors.value(key("a1", "a3")).e == error.e);
}
