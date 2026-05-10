/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QTemporaryDir>
#include <QFile>
#include <QCoreApplication>

// Our includes
#include "cwSurvexImporter.h"
#include "cwFixStation.h"

namespace {

QString writeSvxAndImport(const QByteArray& contents, cwSurvexImporter& importer)
{
    static QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("svxfix_%1_%2.svx")
        .arg(QCoreApplication::applicationPid())
        .arg(reinterpret_cast<quintptr>(&importer), 0, 16));
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(contents);
    f.close();

    importer.setInputFiles(QStringList() << path);
    importer.start();
    importer.waitToFinish();
    return path;
}

} // namespace

TEST_CASE("Survex importer captures *fix coords with most recent *cs", "[SurvexImport][fix]") {
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*begin TestCave\n"
        "*cs EPSG:32616\n"
        "*data normal from to tape compass clino\n"
        "*fix a1 500000 4000000 100\n"
        "*fix a2 500100 4000050 110\n"
        "a1 a2 100.0 90 0\n"
        "*end TestCave\n",
        importer);

    REQUIRE_FALSE(importer.hasParseErrors());

    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 2);

    CHECK(fixes.at(0).stationName().endsWith(QStringLiteral("a1"), Qt::CaseInsensitive));
    CHECK(fixes.at(0).inputCS() == QStringLiteral("EPSG:32616"));
    CHECK(fixes.at(0).easting() == 500000.0);
    CHECK(fixes.at(0).northing() == 4000000.0);
    CHECK(fixes.at(0).elevation() == 100.0);

    CHECK(fixes.at(1).stationName().endsWith(QStringLiteral("a2"), Qt::CaseInsensitive));
    CHECK(fixes.at(1).inputCS() == QStringLiteral("EPSG:32616"));
}

TEST_CASE("Survex importer ignores *cs out (output CS is region-level globalCS)", "[SurvexImport][fix]") {
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*begin Out\n"
        "*cs out EPSG:32616\n"
        "*cs EPSG:4326\n"
        "*data normal from to tape compass clino\n"
        "*fix a1 -85.0 36.0 200\n"
        "a1 a2 1.0 90 0\n"
        "*end Out\n",
        importer);

    REQUIRE_FALSE(importer.hasParseErrors());
    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 1);
    // The fix's inputCS is from the regular *cs, not from *cs out.
    CHECK(fixes.first().inputCS() == QStringLiteral("EPSG:4326"));
}

TEST_CASE("Survex importer captures *fix without preceding *cs as empty inputCS",
          "[SurvexImport][fix]") {
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*begin NoCS\n"
        "*data normal from to tape compass clino\n"
        "*fix a1 1 2 3\n"
        "a1 a2 1.0 90 0\n"
        "*end NoCS\n",
        importer);

    REQUIRE_FALSE(importer.hasParseErrors());
    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 1);
    CHECK(fixes.first().inputCS().isEmpty());
    CHECK(fixes.first().easting() == 1.0);
}
