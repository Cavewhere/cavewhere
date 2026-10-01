/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The region driver at depth N: every native node is its own "*begin <label>"
// block nested the way the tree nests, a sourced root is one *include whose
// children are never blocks, and every region equate is emitted after the last
// block closes, qualified from the region down.

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavernRunner.h"
#include "cwCavingRegion.h"
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwExternalCenterline.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwStationHandle.h"
#include "cwSurvex3DFileReader.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterRegion.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

constexpr double kSideCaveEasting = 100.0;
constexpr double kSideCaveNorthing = 200.0;
constexpr double kSideCaveElevation = 30.0;

cwStationHandle nativeHandle(const cwSurveyNode* node, const QString& tail)
{
    return cwStationHandle(cwStationHandle::NativeCave, node->id(), tail);
}

cwStationHandle tripHandle(const cwTrip* trip, const QString& tail)
{
    return cwStationHandle(cwStationHandle::Trip, trip->id(), tail);
}

QString exportedText(const cwCavingRegion& region,
                     const cwSurvexExporterRegion::Options& options,
                     const QString& outputPath)
{
    const auto result = cwSurvexExporterRegion::exportRegion(region.data(), outputPath, options);
    INFO("export: " << result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());

    QFile file(outputPath);
    REQUIRE(file.open(QFile::ReadOnly));
    return QString::fromUtf8(file.readAll());
}

//! The driver's block structure alone: every *begin, *end, *fix, *include and
//! *equate line, in order. A native trip's body is the trip exporter's own
//! business and does not change with depth; the lines around it are what this
//! file is about.
QStringList skeleton(const QString& driver)
{
    QStringList lines;
    for (const QString& rawLine : driver.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        for (const QString& keyword : {QStringLiteral("*begin"), QStringLiteral("*end"),
                                       QStringLiteral("*fix"), QStringLiteral("*include"),
                                       QStringLiteral("*equate"), QStringLiteral("*cs")}) {
            if (line.startsWith(keyword)) {
                lines.append(line);
                break;
            }
        }
    }
    return lines;
}

//! The §7.1 tree, native part plus one sourced root:
//!
//!   Kentucky field seasons            Folder
//!     Side Cave                       Cave, a fix and one trip
//!       Upper level                   Section: an attached trip and a native trip
//!   OMEGA2                            sourced root (cave-level attach), a window trip
struct SevenOneTree {
    cwCave* folder = nullptr;
    cwCave* sideCave = nullptr;
    cwCave* section = nullptr;
    cwCave* omega2 = nullptr;
    cwTrip* domeClimb = nullptr;
    cwSurvexExporterRegion::Options options;
    QString domeClimbInclude;
    QString omega2Include;
};

SevenOneTree buildSevenOneTree(cwCavingRegion& region, const QTemporaryDir& tempRoot)
{
    SevenOneTree tree;
    tree.folder = addChildNode(region.rootNode(), QStringLiteral("Kentucky field seasons"),
                               cwSurveyNode::Kind::Folder);
    tree.sideCave = addChildNode(tree.folder, QStringLiteral("Side Cave"), cwSurveyNode::Kind::Cave);

    cwFixStation fix;
    fix.setStationName(QStringLiteral("s1"));
    fix.setEasting(kSideCaveEasting);
    fix.setNorthing(kSideCaveNorthing);
    fix.setElevation(kSideCaveElevation);
    tree.sideCave->fixStations()->appendFixStation(fix);
    addNativeTripWithShot(tree.sideCave, QStringLiteral("Sump dig"),
                          QStringLiteral("s1"), QStringLiteral("s2"));

    tree.section = addChildNode(tree.sideCave, QStringLiteral("Upper level"),
                                cwSurveyNode::Kind::Folder);
    tree.domeClimb = addAttachedTrip(tree.section, QStringLiteral("Dome climb"));
    addNativeTripWithShot(tree.section, QStringLiteral("Upper survey"),
                          QStringLiteral("u1"), QStringLiteral("u2"));

    const QString domeDir = tempSubdir(tempRoot, QStringLiteral("dome"));
    tree.domeClimbInclude = seedAttachment(domeDir, fixturePath(QStringLiteral("survex_simple.svx")));
    tree.options.tripAttachmentDirs.insert(tree.domeClimb->id(), domeDir);

    tree.omega2 = addEmptyCave(region, QStringLiteral("OMEGA2"));
    tree.omega2->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_blocks.svx")));
    addEmptyTrip(tree.omega2, QStringLiteral("Window"));
    const QString omegaDir = tempSubdir(tempRoot, QStringLiteral("omega2"));
    tree.omega2Include = seedAttachment(omegaDir, fixturePath(QStringLiteral("survex_blocks.svx")));
    tree.options.caveAttachmentDirs.insert(tree.omega2->id(), omegaDir);

    region.equates()->appendEquate(cwEquate({tripHandle(tree.domeClimb, QStringLiteral("simple.a1")),
                                             nativeHandle(tree.omega2, QStringLiteral("doghill.d2"))}));
    region.equates()->appendEquate(cwEquate({nativeHandle(tree.section, QStringLiteral("u1")),
                                             nativeHandle(tree.sideCave, QStringLiteral("s2"))}));
    return tree;
}

} // namespace

TEST_CASE("Every native node exports as its own nested *begin block", "[Exporter][NodeTree]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const SevenOneTree tree = buildSevenOneTree(region, tempRoot);

    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = exportedText(region, tree.options, driverPath);
    INFO("driver:\n" << driver.toStdString());

    // The node's own trips first, then its child nodes, then its *end. The
    // Section gets no fallback fix: Side Cave's fix already anchors the path it
    // sits on, and a second anchor would pin a connected survey twice.
    const QStringList expected = {
        QStringLiteral("*begin  ;All the caves"),
        QStringLiteral("*begin kentucky_field_seasons ;Kentucky field seasons"),
        QStringLiteral("*begin side_cave ;Side Cave"),
        QStringLiteral("*fix s1 100.000000 200.000000 30.000000"),
        QStringLiteral("*begin ; Sump dig"),
        QStringLiteral("*end"),
        QStringLiteral("*begin upper_level ;Upper level"),
        QStringLiteral("*begin dome_climb ; Dome climb"),
        QStringLiteral("*include \"%1\"").arg(tree.domeClimbInclude),
        QStringLiteral("*end dome_climb"),
        QStringLiteral("*begin ; Upper survey"),
        QStringLiteral("*end"),
        QStringLiteral("*end upper_level ; End of Upper level"),
        QStringLiteral("*end side_cave ; End of Side Cave"),
        QStringLiteral("*end kentucky_field_seasons ; End of Kentucky field seasons"),
        QStringLiteral("*begin omega2 ;OMEGA2"),
        QStringLiteral("*include \"%1\"").arg(tree.omega2Include),
        QStringLiteral("*end omega2 ; End of OMEGA2"),
        QStringLiteral("*equate kentucky_field_seasons.side_cave.upper_level.dome_climb.simple.a1"
                       " omega2.doghill.d2"),
        QStringLiteral("*equate kentucky_field_seasons.side_cave.upper_level.u1"
                       " kentucky_field_seasons.side_cave.s2"),
        QStringLiteral("*end"),
    };
    CHECK(skeleton(driver) == expected);
}

TEST_CASE("A nested tree solves with every station under its node's label path",
          "[Exporter][NodeTree]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const SevenOneTree tree = buildSevenOneTree(region, tempRoot);

    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    exportedText(region, tree.options, driverPath);

    const QString threeDPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.3d"));
    const auto ran = cwCavernRunner::run(driverPath, threeDPath);
    INFO("cavern: " << ran.errorMessage().toStdString());
    REQUIRE_FALSE(ran.hasError());

    cwSurvex3DFileReader reader;
    const cwStationPositionLookup solved = reader.readStationPositions(threeDPath);

    for (const QString& name : {QStringLiteral("kentucky_field_seasons.side_cave.s1"),
                                QStringLiteral("kentucky_field_seasons.side_cave.s2"),
                                QStringLiteral("kentucky_field_seasons.side_cave.upper_level.u1"),
                                QStringLiteral("kentucky_field_seasons.side_cave.upper_level.u2"),
                                QStringLiteral("kentucky_field_seasons.side_cave.upper_level.dome_climb.simple.a1"),
                                QStringLiteral("omega2.doghill.d2")}) {
        INFO("station: " << name.toStdString());
        CHECK(solved.hasPosition(name));
    }

    // Side Cave's own fix is where s1 lands, so the Section's u1 — tied to s2,
    // 10 m north of it — lands there too, rather than on a fallback fix.
    const QVector3D s1 = solved.position(QStringLiteral("kentucky_field_seasons.side_cave.s1"));
    CHECK(s1.x() == Catch::Approx(kSideCaveEasting).margin(0.001));
    CHECK(s1.y() == Catch::Approx(kSideCaveNorthing).margin(0.001));
    const QVector3D u1 = solved.position(QStringLiteral("kentucky_field_seasons.side_cave.upper_level.u1"));
    const QVector3D s2 = solved.position(QStringLiteral("kentucky_field_seasons.side_cave.s2"));
    CHECK((u1 - s2).length() == Catch::Approx(0.0).margin(0.001));
}

TEST_CASE("A node whose subtree holds no survey data emits nothing", "[Exporter][NodeTree]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* fisherRidge = addEmptyCave(region, QStringLiteral("Fisher Ridge"));
    addNativeTripWithShot(fisherRidge, QStringLiteral("Entrance"),
                          QStringLiteral("f1"), QStringLiteral("f2"));

    // A Folder that only groups an empty cave, a bare Folder, and a cave whose
    // one trip holds no shots: none of them has anything for cavern to read.
    cwCave* groupingFolder = addChildNode(region.rootNode(), QStringLiteral("Grouping folder"),
                                          cwSurveyNode::Kind::Folder);
    addChildNode(groupingFolder, QStringLiteral("Empty cave"), cwSurveyNode::Kind::Cave);
    addChildNode(region.rootNode(), QStringLiteral("Bare folder"), cwSurveyNode::Kind::Folder);
    cwCave* tripOnly = addEmptyCave(region, QStringLiteral("Trip only"));
    addEmptyTrip(tripOnly, QStringLiteral("Not yet entered"));

    // Data deep inside a Folder still opens every block on the way down to it.
    cwCave* holdingFolder = addChildNode(region.rootNode(), QStringLiteral("Holding folder"),
                                         cwSurveyNode::Kind::Folder);
    cwCave* deepCave = addChildNode(holdingFolder, QStringLiteral("Deep cave"),
                                    cwSurveyNode::Kind::Cave);
    addNativeTripWithShot(deepCave, QStringLiteral("Deep"), QStringLiteral("d1"), QStringLiteral("d2"));

    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = exportedText(region, {}, driverPath);
    INFO("driver:\n" << driver.toStdString());

    CHECK(driver.contains(QStringLiteral("*begin fisher_ridge")));
    CHECK(driver.contains(QStringLiteral("*begin holding_folder")));
    CHECK(driver.contains(QStringLiteral("*begin deep_cave")));
    CHECK_FALSE(driver.contains(QStringLiteral("grouping_folder")));
    CHECK_FALSE(driver.contains(QStringLiteral("empty_cave")));
    CHECK_FALSE(driver.contains(QStringLiteral("bare_folder")));
    CHECK_FALSE(driver.contains(QStringLiteral("trip_only")));

    // Each top-level cave still gets its own anchor, so the deep one solves.
    CHECK(driver.contains(QStringLiteral("*fix d1 0 0 0")));
    CHECK(driver.contains(QStringLiteral("*fix f1 0 0 0")));

    const QString threeDPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.3d"));
    const auto ran = cwCavernRunner::run(driverPath, threeDPath);
    INFO("cavern: " << ran.errorMessage().toStdString());
    CHECK_FALSE(ran.hasError());
}

TEST_CASE("The single-cave export nests the cave's sections", "[Exporter][NodeTree]")
{
    cwCavingRegion region;
    cwCave* sideCave = addEmptyCave(region, QStringLiteral("Side Cave"));
    addNativeTripWithShot(sideCave, QStringLiteral("Sump dig"), QStringLiteral("s1"), QStringLiteral("s2"));
    cwCave* section = addChildNode(sideCave, QStringLiteral("Upper level"), cwSurveyNode::Kind::Folder);
    addNativeTripWithShot(section, QStringLiteral("Upper survey"), QStringLiteral("u1"), QStringLiteral("u2"));
    region.equates()->appendEquate(cwEquate({nativeHandle(section, QStringLiteral("u1")),
                                             nativeHandle(sideCave, QStringLiteral("s2"))}));

    QString driver;
    {
        QTextStream stream(&driver);
        cwSurvexExporterCaveTask exporter;
        exporter.setEquates(region.equates()->equates());
        CHECK(exporter.writeCave(stream, sideCave->data()));
    }
    INFO("driver:\n" << driver.toStdString());

    const QStringList expected = {
        QStringLiteral("*begin side_cave ;Side Cave"),
        QStringLiteral("*fix s1 0 0 0"),
        QStringLiteral("*begin ; Sump dig"),
        QStringLiteral("*end"),
        QStringLiteral("*begin upper_level ;Upper level"),
        QStringLiteral("*begin ; Upper survey"),
        QStringLiteral("*end"),
        QStringLiteral("*end upper_level ; End of Upper level"),
        QStringLiteral("*end side_cave ; End of Side Cave"),
        QStringLiteral("*equate side_cave.upper_level.u1 side_cave.s2"),
    };
    CHECK(skeleton(driver) == expected);
}

TEST_CASE("A fix with no station of its node's own opens no block", "[Exporter][NodeTree]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* fisherRidge = addEmptyCave(region, QStringLiteral("Fisher Ridge"));
    addNativeTripWithShot(fisherRidge, QStringLiteral("Entrance"),
                          QStringLiteral("f1"), QStringLiteral("f2"));

    // A fix is validated against the node's own stations, so on a node with
    // none — a top-level cave, or a Section whose stations all live in its
    // parent — every fix is rejected and the block would hold nothing.
    cwFixStation fix;
    fix.setStationName(QStringLiteral("f1"));
    fix.setEasting(kSideCaveEasting);
    fix.setNorthing(kSideCaveNorthing);
    fix.setElevation(kSideCaveElevation);

    cwCave* fixOnly = addEmptyCave(region, QStringLiteral("Fix only"));
    fixOnly->fixStations()->appendFixStation(fix);
    cwCave* fixOnlySection = addChildNode(fisherRidge, QStringLiteral("Fix only section"),
                                          cwSurveyNode::Kind::Folder);
    fixOnlySection->fixStations()->appendFixStation(fix);

    // A tie into the fix-only cave names a scope the driver never opens.
    region.equates()->appendEquate(cwEquate({nativeHandle(fixOnly, QStringLiteral("f1")),
                                             nativeHandle(fisherRidge, QStringLiteral("f2"))}));

    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = exportedText(region, {}, driverPath);
    INFO("driver:\n" << driver.toStdString());

    CHECK(driver.contains(QStringLiteral("*begin fisher_ridge")));
    CHECK_FALSE(driver.contains(QStringLiteral("fix_only")));
    CHECK_FALSE(driver.contains(QStringLiteral("*equate")));
}
