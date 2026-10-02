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
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwLinePlotManager.h"
#include "cwStationPositionLookup.h"
#include "cwTreeImportData.h"
#include "cwTreeImportDataNode.h"

// Std includes
#include <algorithm>
#include <memory>

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

//! Write \a contents into \a dir as \a name, so a test can *include one file
//! from another.
void writeSvxFile(const QTemporaryDir& dir, const QString& name, const QByteArray& contents)
{
    QFile file(dir.filePath(name));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(contents);
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

    // A local grid is the ordinary reason an svx has *fix and no *cs, so all
    // three numbers have to reach the row even though nothing can be derived
    // from them yet — the fix says it has no system, and naming one reads them
    // straight back out.
    CHECK(fixes.first().state() == cwFixStation::NoSystem);
    CHECK(fixes.first().coordinate() == QStringLiteral("1, 2, 3.000m"));

    cwFixStation named = fixes.first();
    named.setInputCS(QStringLiteral("EPSG:32611"));
    CHECK(named.easting() == 1.0);
    CHECK(named.northing() == 2.0);
    CHECK(named.elevation() == 3.0);
}

TEST_CASE("Survex importer translates *cs into the system PROJ knows it by",
          "[SurvexImport][fix]") {
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*begin Keyword\n"
        "*cs UTM16N\n"
        "*data normal from to tape compass clino\n"
        "*fix a1 580661.57 4113846.34 219\n"
        "a1 a2 1.0 90 0\n"
        "*end Keyword\n",
        importer);

    REQUIRE_FALSE(importer.hasParseErrors());
    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 1);

    // UTM16N is survex's spelling of it. Everything downstream of the fix — the
    // picker, the export, the local projection — reads inputCS through PROJ, so
    // the zone is stored the way PROJ names it.
    CHECK(fixes.first().inputCS() == QStringLiteral("EPSG:32616"));
    CHECK(fixes.first().state() == cwFixStation::Valid);
    CHECK(fixes.first().easting() == 580661.57);
    CHECK(fixes.first().northing() == 4113846.34);
}

TEST_CASE("A *cs LOCAL survey is ungeoreferenced without complaint",
          "[SurvexImport][fix]") {
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*begin Local\n"
        "*cs LOCAL\n"
        "*data normal from to tape compass clino\n"
        "*fix a1 1 2 3\n"
        "a1 a2 1.0 90 0\n"
        "*end Local\n",
        importer);

    // LOCAL says the survey is on a grid of its own, which is a statement, not
    // a system we failed to read — so it warns about nothing.
    INFO(importer.parseErrors().join(QLatin1Char('\n')).toStdString());
    CHECK_FALSE(importer.hasParseErrors());

    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 1);
    CHECK(fixes.first().inputCS().isEmpty());
    CHECK(fixes.first().state() == cwFixStation::NoSystem);
    CHECK(fixes.first().coordinate() == QStringLiteral("1, 2, 3.000m"));
}

TEST_CASE("A *cs spelling survex doesn't define leaves the fix saying so",
          "[SurvexImport][fix]") {
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*begin Unknown\n"
        "*cs MADE-UP-GRID\n"
        "*data normal from to tape compass clino\n"
        "*fix a1 1 2 3\n"
        "a1 a2 1.0 90 0\n"
        "*end Unknown\n",
        importer);

    const QStringList errors = importer.parseErrors();
    CHECK(std::any_of(errors.begin(), errors.end(), [](const QString& error) {
        return error.contains(QStringLiteral("MADE-UP-GRID"));
    }));

    // Storing the text would put a coordinate system on the fix that nothing
    // can read, and the numbers would be read back under whichever axis order
    // that string happened to imply. No system says what happened.
    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 1);
    CHECK(fixes.first().inputCS().isEmpty());
    CHECK(fixes.first().state() == cwFixStation::NoSystem);
}

TEST_CASE("The two lat/long orders put a fix in the same place",
          "[SurvexImport][fix]") {
    cwSurvexImporter longLatImporter;
    writeSvxAndImport(
        "*begin LongLat\n"
        "*cs LONG-LAT\n"
        "*data normal from to tape compass clino\n"
        "*fix a1 -86.0914594 37.1675542 219\n"
        "a1 a2 1.0 90 0\n"
        "*end LongLat\n",
        longLatImporter);

    cwSurvexImporter latLongImporter;
    writeSvxAndImport(
        "*begin LatLong\n"
        "*cs LAT-LONG\n"
        "*data normal from to tape compass clino\n"
        "*fix a1 37.1675542 -86.0914594 219\n"
        "a1 a2 1.0 90 0\n"
        "*end LatLong\n",
        latLongImporter);

    const QList<cwFixStation> longLat = longLatImporter.capturedFixStations();
    const QList<cwFixStation> latLong = latLongImporter.capturedFixStations();
    REQUIRE(longLat.size() == 1);
    REQUIRE(latLong.size() == 1);

    // The two files name the same point in Kentucky. Reading the second pair in
    // the first one's order would put it in the Indian Ocean, and nothing
    // downstream could tell.
    CHECK(longLat.first().easting() == -86.0914594);
    CHECK(longLat.first().northing() == 37.1675542);
    CHECK(latLong.first().easting() == longLat.first().easting());
    CHECK(latLong.first().northing() == longLat.first().northing());
    CHECK(latLong.first().inputCS() == longLat.first().inputCS());
    CHECK(latLong.first().coordinate() == longLat.first().coordinate());
}

TEST_CASE("A cave fixed under a survex keyword system still solves",
          "[SurvexImport][fix]") {
    // The regression: the fix's system used to be stored as survex wrote it,
    // which PROJ can't read — so no local projection derived from it, the
    // export named no output system, and cavern refused the file it was handed
    // ("input projection is set but output projection isn't"). Asserted on the
    // solve rather than on the exported text, which is where it was missed.
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*begin SolveCave\n"
        "*cs UTM16N\n"
        "*begin SolveTrip\n"
        "*data normal from to tape compass clino\n"
        "*fix a1 580661.57 4113846.34 219\n"
        "a1 a2 10.0 90 0\n"
        "*end SolveTrip\n"
        "*end SolveCave\n",
        importer);

    REQUIRE_FALSE(importer.hasParseErrors());

    cwTreeImportData* data = importer.data();
    REQUIRE(data->nodes().size() == 1);
    cwTreeImportDataNode* root = data->nodes().first();
    root->setImportType(cwTreeImportDataNode::Cave);
    REQUIRE(root->childNodeCount() == 1);
    root->childNode(0)->setImportType(cwTreeImportDataNode::Trip);

    const QList<cwCave*> caves = data->caves();
    REQUIRE(caves.size() == 1);

    auto region = std::make_unique<cwCavingRegion>();
    region->addCave(caves.first());
    for(const cwFixStation& fix : importer.capturedFixStations()) {
        region->cave(0)->fixStations()->appendFixStation(fix);
    }

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(region.get());
    plotManager->waitToFinish();

    INFO(plotManager->solveErrorMessage().toStdString());
    CHECK_FALSE(plotManager->hasSolveError());
    CHECK(region->cave(0)->stationPositionLookup().hasPosition(QStringLiteral("a2")));
}

TEST_CASE("A *cs stops at the *end of the block that named it",
          "[SurvexImport][fix]") {
    // Survex scopes *cs to its block — cmd_begin copies proj_str into the child
    // settings and pop_settings puts the parent's back — so the inner LAT-LONG
    // is gone by the time a2 is fixed. Getting this wrong doesn't just mislabel
    // a2's system: LAT-LONG transposes the pair, so a leak silently swaps the
    // easting and northing of every later fix.
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*begin outer\n"
        "*cs UTM16N\n"
        "*begin inner\n"
        "*cs LAT-LONG\n"
        "*fix b1 37.1675542 -86.0914594 219\n"
        "*end inner\n"
        "*fix a2 580661.57 4113846.34 219\n"
        "*end outer\n",
        importer);

    INFO(importer.parseErrors().join(QLatin1Char('\n')).toStdString());
    REQUIRE_FALSE(importer.hasParseErrors());

    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 2);

    CHECK(fixes.at(0).inputCS() == QStringLiteral("EPSG:4326"));
    CHECK(fixes.at(0).easting() == -86.0914594);
    CHECK(fixes.at(0).northing() == 37.1675542);

    CHECK(fixes.at(1).inputCS() == QStringLiteral("EPSG:32616"));
    CHECK(fixes.at(1).easting() == 580661.57);
    CHECK(fixes.at(1).northing() == 4113846.34);
}

TEST_CASE("One cave's *cs doesn't reach the next cave in the file",
          "[SurvexImport][fix]") {
    // The ordinary shape of a multi-cave file: one cave is georeferenced and
    // the next is on a local grid. caveB's fix has to come out ungeoreferenced
    // with its numbers in the order the file wrote them.
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*begin caveA\n"
        "*cs LAT-LONG\n"
        "*fix a1 37.1675542 -86.0914594 219\n"
        "*end caveA\n"
        "*begin caveB\n"
        "*fix b1 100 200 300\n"
        "*end caveB\n",
        importer);

    INFO(importer.parseErrors().join(QLatin1Char('\n')).toStdString());
    REQUIRE_FALSE(importer.hasParseErrors());

    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 2);

    CHECK(fixes.at(1).inputCS().isEmpty());
    CHECK(fixes.at(1).state() == cwFixStation::NoSystem);
    CHECK(fixes.at(1).coordinate() == QStringLiteral("100, 200, 300.000m"));
}

TEST_CASE("A block inherits the *cs of the block around it",
          "[SurvexImport][fix]") {
    // The scoping cuts one way only: a nested block starts under whatever the
    // parent named, which is how a file georeferences a whole cave with one
    // *cs at the top.
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*begin outer\n"
        "*cs UTM16N\n"
        "*begin inner\n"
        "*fix a1 580661.57 4113846.34 219\n"
        "*end inner\n"
        "*end outer\n",
        importer);

    INFO(importer.parseErrors().join(QLatin1Char('\n')).toStdString());
    REQUIRE_FALSE(importer.hasParseErrors());

    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 1);
    CHECK(fixes.first().inputCS() == QStringLiteral("EPSG:32616"));
    CHECK(fixes.first().easting() == 580661.57);
}

TEST_CASE("A *cs reaches into an *include", "[SurvexImport][fix]") {
    // *include isn't a scope in survex — only *begin is — so proj_str carries
    // into the included file. The importer's other block state resets at a file
    // boundary, which is why the coordinate system is inherited on its own.
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const QString includedName = QStringLiteral("included_%1")
                                     .arg(QCoreApplication::applicationPid());
    writeSvxFile(dir, includedName + QStringLiteral(".svx"),
                 "*begin innerCave\n"
                 "*fix a1 580661.57 4113846.34 219\n"
                 "*end innerCave\n");

    const QString rootPath = dir.filePath(QStringLiteral("root_%1.svx")
                                              .arg(QCoreApplication::applicationPid()));
    QFile root(rootPath);
    REQUIRE(root.open(QIODevice::WriteOnly | QIODevice::Text));
    root.write(QStringLiteral("*begin outer\n"
                              "*cs UTM16N\n"
                              "*include %1\n"
                              "*end outer\n")
                   .arg(includedName)
                   .toUtf8());
    root.close();

    cwSurvexImporter importer;
    importer.setInputFiles(QStringList() << rootPath);
    importer.start();
    importer.waitToFinish();

    INFO(importer.parseErrors().join(QLatin1Char('\n')).toStdString());
    REQUIRE_FALSE(importer.hasParseErrors());

    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 1);
    CHECK(fixes.first().inputCS() == QStringLiteral("EPSG:32616"));
    CHECK(fixes.first().easting() == 580661.57);
}

TEST_CASE("A *cs above the first *begin covers the whole file",
          "[SurvexImport][fix]") {
    // The idiomatic shape: name the system once at the top and let every cave
    // below it inherit. The root block's state is the outermost frame, so this
    // is the same inheritance a nested *begin gets.
    cwSurvexImporter importer;
    writeSvxAndImport(
        "*cs UTM16N\n"
        "*begin cave\n"
        "*fix a1 580661.57 4113846.34 219\n"
        "*end cave\n",
        importer);

    INFO(importer.parseErrors().join(QLatin1Char('\n')).toStdString());
    REQUIRE_FALSE(importer.hasParseErrors());

    const QList<cwFixStation> fixes = importer.capturedFixStations();
    REQUIRE(fixes.size() == 1);
    CHECK(fixes.first().inputCS() == QStringLiteral("EPSG:32616"));
    CHECK(fixes.first().state() == cwFixStation::Valid);
}
