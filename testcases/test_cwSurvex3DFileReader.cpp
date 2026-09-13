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
#include <QDir>

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
    // labels arrive as "<caveLabel>:<tripLabel>:holberg1:1", the cave scope
    // never splits off, and every station drops out of the solve.
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

    // No label may keep the raw ':' separator, and every scoped label must split
    // on '.' the way the decode side expects. (The Walls include emits its own
    // top-level stations with no enclosing survey; they only need to be
    // separator-normalized, not splittable.)
    const QMap<QString, QVector3D> positions = lookup.positions();
    for (auto iter = positions.constBegin(); iter != positions.constEnd(); ++iter) {
        const QString& name = iter.key();
        CHECK_FALSE(name.contains(QLatin1Char(':')));
        if (name.contains(QLatin1Char('.'))) {
            CHECK_FALSE(cwCavernNaming::scopeHeadOf(name).isEmpty());
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

TEST_CASE("cavern survives a fatal error inside an included file and reports the next run's include chain",
          "[cwSurvex3DFileReader]") {
    // Regression: a fatal error inside an *include longjmps out of data_file()
    // leaving the parser's include chain pointing at unwound stack frames, so
    // the next run in the same process crashed in report_parent() while
    // formatting its first diagnostic.
    const QString fatalDriver = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/fatal_in_include_driver.svx"));
    const QString oneErrorDriver = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/one_error_driver.svx"));
    REQUIRE(QFile::exists(fatalDriver));
    REQUIRE(QFile::exists(oneErrorDriver));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    auto fatalRun = cwCavernRunner::run(fatalDriver,
                                        workDir.filePath(QStringLiteral("run1.3d")));
    CHECK(fatalRun.hasError());
    CHECK(fatalRun.errorMessage().contains(QStringLiteral("Too many errors"),
                                           Qt::CaseInsensitive));

    auto nextRun = cwCavernRunner::run(oneErrorDriver,
                                       workDir.filePath(QStringLiteral("run2.3d")));
    CHECK(nextRun.hasError());
    CHECK(nextRun.errorMessage().contains(QStringLiteral("one_error_included.svx")));
    // A stale chain names the previous run's files in the include tree.
    CHECK_FALSE(nextRun.errorMessage().contains(QStringLiteral("too_many_errors")));
    CHECK(nextRun.errorMessage().count(QStringLiteral("In file included from")) == 1);
}

TEST_CASE("cavern closes the enclosing include files a fatal error skips past",
          "[cwSurvex3DFileReader]") {
    // Regression: data_file() keeps the enclosing file's handle in a
    // stack-local, so a fatal two includes deep longjmps past both fclose()
    // calls, burning two descriptors per run.
    const QString driver = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/fd_leak_parent.svx"));
    REQUIRE(QFile::exists(driver));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    const auto openDescriptorCount = []() {
        return QDir(QStringLiteral("/dev/fd")).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size();
    };

    const auto runOnce = [&](int run) {
        auto cavernResult = cwCavernRunner::run(
            driver, workDir.filePath(QStringLiteral("fd_leak_%1.3d").arg(run)));
        CHECK(cavernResult.hasError());
        CHECK(cavernResult.errorMessage().contains(QStringLiteral("Too many errors"),
                                                   Qt::CaseInsensitive));
    };

    // The first run settles any one-time allocations, so the baseline is a
    // steady state.
    runOnce(0);
    const int baselineDescriptors = openDescriptorCount();

    constexpr int kFatalRunCount = 5;
    for (int run = 1; run <= kFatalRunCount; ++run) {
        runOnce(run);
    }

    // Other threads in the process open and close descriptors of their own, so
    // allow a little drift: the leak this guards against burned two per run.
    constexpr int kDescriptorDrift = 1;
    CHECK(openDescriptorCount() <= baselineDescriptors + kDescriptorDrift);
}

TEST_CASE("cavern survives a fatal error inside *begin without a double free",
          "[cwSurvex3DFileReader]") {
    // Regression: a fatal error inside *begin longjmps out of the run with the
    // settings node still pushed, and teardown then double-freed the settings
    // a child shares with its parent (see cavern_free_settings_chain).
    // The fixture wraps enough malformed lines in *begin to pass cavern's
    // error limit, so "Too many errors - giving up" fires while pushed.
    const QString survexDataFile =
        testcasesDatasetSourcePath(QStringLiteral("test_cwSurvex3DFileReader/too_many_errors.svx"));
    REQUIRE(QFile::exists(survexDataFile));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    // Run twice: the second run exercises cavern_prepare_state() on a chain
    // that the first run's aborted teardown already cleaned up.
    for (int run = 0; run < 2; ++run) {
        const QString output3dPath =
            workDir.filePath(QStringLiteral("too_many_errors_%1.3d").arg(run));
        auto cavernResult = cwCavernRunner::run(survexDataFile, output3dPath);

        CHECK(cavernResult.hasError());
        CHECK(cavernResult.errorMessage().contains(QStringLiteral("Too many errors"),
                                                   Qt::CaseInsensitive));
    }
}

TEST_CASE("cavern reads a Compass DAT whose lines end in CR CR LF",
          "[cwSurvex3DFileReader]") {
    // Regression: survex's line-end normalizer treated the second CR of a
    // "\r\r\n" line end as the start of a new line, so the fixed-layout Compass
    // DAT reader saw a phantom blank line between every real one and reported
    // "Expecting SURVEY" / "End of line not blank" for the whole file.
    const QString driver = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/compass_crcrlf_driver.svx"));
    REQUIRE(QFile::exists(driver));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());
    const QString output3dPath = workDir.filePath(QStringLiteral("compass_crcrlf.3d"));

    auto cavernResult = cwCavernRunner::run(driver, output3dPath);
    REQUIRE_FALSE(cavernResult.hasError());
    CHECK(cavernResult.value().warningCount == 0);
    CHECK_FALSE(cavernResult.value().logText.contains(QStringLiteral("error:")));
    REQUIRE(QFileInfo(cavernResult.value().output3dPath).exists());

    // Assert on the solved survey so a change that silently drops the file's
    // legs still fails.
    cwSurvex3DFileReader reader;
    auto parsed = reader.readNetworkAndLookup(cavernResult.value().output3dPath);

    constexpr int kStationCount = 6;
    CHECK(parsed.network.stations().size() == kStationCount);
    for (int i = 1; i <= kStationCount; ++i) {
        CHECK(parsed.lookup.hasPosition(QStringLiteral("cave0.crcrlf.A%1").arg(i)));
    }

    // Five 10 ft legs due east from A1, which the .MAK fixes at the origin.
    const QVector3D a1 = parsed.lookup.position(QStringLiteral("cave0.crcrlf.A1"));
    const QVector3D a6 = parsed.lookup.position(QStringLiteral("cave0.crcrlf.A6"));
    CHECK(a1.x() == Catch::Approx(0.0).margin(0.01));
    CHECK(a6.x() == Catch::Approx(15.24).margin(0.01));
    CHECK(a6.y() == Catch::Approx(0.0).margin(0.01));
    CHECK(a6.z() == Catch::Approx(0.0).margin(0.01));
}

TEST_CASE("cavern reads each DAT of a Compass MAK into its own survey",
          "[cwSurvex3DFileReader]") {
    // Regression: every DAT of a project was read into one namespace, so a
    // project which numbers each cave's stations from 1 - as Compass projects
    // do - fixed one station "1" several times over and cavern refused to
    // write any output.
    const QString driver = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/compass_mak_scopes_driver.svx"));
    REQUIRE(QFile::exists(driver));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());
    const QString output3dPath = workDir.filePath(QStringLiteral("compass_mak_scopes.3d"));

    auto cavernResult = cwCavernRunner::run(driver, output3dPath);
    REQUIRE_FALSE(cavernResult.hasError());
    CHECK(cavernResult.value().warningCount == 0);
    CHECK_FALSE(cavernResult.value().logText.contains(QStringLiteral("error:")));
    REQUIRE(QFileInfo(cavernResult.value().output3dPath).exists());

    cwSurvex3DFileReader reader;
    auto parsed = reader.readNetworkAndLookup(cavernResult.value().output3dPath);

    // A.DAT and B.DAT both start at a station called "1", and the .MAK fixes
    // each of them where that file's own link station says.
    CHECK(parsed.network.stations().size() == 8);
    const QVector3D a1 = parsed.lookup.position(QStringLiteral("cave0.a.1"));
    CHECK(a1.x() == Catch::Approx(0.0).margin(0.01));
    CHECK(a1.y() == Catch::Approx(0.0).margin(0.01));
    CHECK(a1.z() == Catch::Approx(0.0).margin(0.01));

    const QVector3D b1 = parsed.lookup.position(QStringLiteral("cave0.b.1"));
    CHECK(b1.x() == Catch::Approx(30.48).margin(0.01));
    CHECK(b1.y() == Catch::Approx(60.96).margin(0.01));
    CHECK(b1.z() == Catch::Approx(-15.24).margin(0.01));

    // Two 10 ft legs east in each file, from that file's own station "1".
    CHECK(parsed.lookup.position(QStringLiteral("cave0.a.A3")).x()
          == Catch::Approx(6.10).margin(0.01));
    CHECK(parsed.lookup.position(QStringLiteral("cave0.b.B3")).x()
          == Catch::Approx(36.58).margin(0.01));

    // C.DAT lists B3 as a link station, which ties it to B.DAT's B3 - so
    // C.DAT's 10 ft leg north starts there rather than in mid air.
    const QVector3D cb3 = parsed.lookup.position(QStringLiteral("cave0.c.B3"));
    const QVector3D bb3 = parsed.lookup.position(QStringLiteral("cave0.b.B3"));
    CHECK(cb3.x() == Catch::Approx(bb3.x()).margin(0.01));
    CHECK(cb3.y() == Catch::Approx(bb3.y()).margin(0.01));
    CHECK(cb3.z() == Catch::Approx(bb3.z()).margin(0.01));

    const QVector3D c2 = parsed.lookup.position(QStringLiteral("cave0.c.C2"));
    CHECK(c2.x() == Catch::Approx(36.58).margin(0.01));
    CHECK(c2.y() == Catch::Approx(64.01).margin(0.01));
    CHECK(c2.z() == Catch::Approx(-15.24).margin(0.01));
}

TEST_CASE("cavern warns about a Compass MAK naming a DAT which is absent",
          "[cwSurvex3DFileReader]") {
    // Regression: a .MAK listing a DAT that isn't on disk made cavern withhold
    // every output file, so an otherwise readable project loaded as nothing.
    const QString driver = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/compass_mak_missing_driver.svx"));
    REQUIRE(QFile::exists(driver));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());
    const QString output3dPath = workDir.filePath(QStringLiteral("compass_mak_missing.3d"));

    auto cavernResult = cwCavernRunner::run(driver, output3dPath);
    REQUIRE_FALSE(cavernResult.hasError());
    CHECK(cavernResult.value().warningCount == 1);
    CHECK(cavernResult.value().logText.contains(QStringLiteral("NOSUCH.DAT")));
    REQUIRE(QFileInfo(cavernResult.value().output3dPath).exists());

    cwSurvex3DFileReader reader;
    auto parsed = reader.readNetworkAndLookup(cavernResult.value().output3dPath);

    // The DAT which is present still contributes all of its stations.
    CHECK(parsed.network.stations().size() == 3);
    CHECK(parsed.lookup.hasPosition(QStringLiteral("cave0.a.1")));
    CHECK(parsed.lookup.position(QStringLiteral("cave0.a.A3")).x()
          == Catch::Approx(6.10).margin(0.01));
}

TEST_CASE("cavern rejects two Compass MAK fixed points for one station",
          "[cwSurvex3DFileReader]") {
    // Guard for the case above: contradictory fixed points within one DAT's
    // link station list are still an error, since nothing can place the file.
    const QString driver = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/compass_mak_refix_driver.svx"));
    REQUIRE(QFile::exists(driver));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());
    const QString output3dPath = workDir.filePath(QStringLiteral("compass_mak_refix.3d"));

    auto cavernResult = cwCavernRunner::run(driver, output3dPath);
    REQUIRE(cavernResult.hasError());
    CHECK_FALSE(QFileInfo(output3dPath).exists());
}

TEST_CASE("cavern survives a fatal error inside a Walls file with a pushed options level",
          "[cwSurvex3DFileReader][Walls][Attach]") {
    // Regression: a fatal error inside a Walls .srv longjmps past
    // pop_walls_options(), leaving datain.c's static options stack pushed. The
    // next run then read a non-empty stack as "nested", skipped its own
    // settings level, and inherited the dead run's options - here the previous
    // file's #Prefix, so the clean .srv's stations came out as STALE.A1 /
    // STALE.A2.
    const QString fatalFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/walls_stale_state.srv"));
    const QString cleanFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/walls_clean.srv"));
    REQUIRE(QFile::exists(fatalFile));
    REQUIRE(QFile::exists(cleanFile));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    auto fatalRun = cwCavernRunner::run(fatalFile,
                                        workDir.filePath(QStringLiteral("walls_run1.3d")));
    CHECK(fatalRun.hasError());
    CHECK(fatalRun.errorMessage().contains(QStringLiteral("Too many errors"),
                                           Qt::CaseInsensitive));

    auto cleanRun = cwCavernRunner::run(cleanFile,
                                        workDir.filePath(QStringLiteral("walls_run2.3d")));
    REQUIRE_FALSE(cleanRun.hasError());
    REQUIRE(QFileInfo(cleanRun.value().output3dPath).exists());

    // The clean .srv names no prefix, so its two stations stand alone.
    cwSurvex3DFileReader reader;
    cwStationPositionLookup lookup = reader.readStationPositions(cleanRun.value().output3dPath);
    CHECK(lookup.positions().size() == 2);
    CHECK(lookup.hasPosition(QStringLiteral("A1")));
    CHECK(lookup.hasPosition(QStringLiteral("A2")));
}

TEST_CASE("cavern clears Walls macros a fatal error leaves defined",
          "[cwSurvex3DFileReader][Walls][Attach]") {
    // Regression: datain.c clears the .srv macro table only at the end of
    // data_file_walls_srv(), so a fatal error longjmps past it and leaves the
    // macros defined. The next run then resolved $(loc) from the dead run,
    // silently prefixing its stations instead of reporting the macro as
    // undefined.
    const QString fatalFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/walls_stale_state.srv"));
    const QString macroUserFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/walls_use_macro.srv"));
    REQUIRE(QFile::exists(fatalFile));
    REQUIRE(QFile::exists(macroUserFile));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    auto fatalRun = cwCavernRunner::run(fatalFile,
                                        workDir.filePath(QStringLiteral("walls_macro_run1.3d")));
    CHECK(fatalRun.hasError());
    CHECK(fatalRun.errorMessage().contains(QStringLiteral("Too many errors"),
                                           Qt::CaseInsensitive));

    auto macroRun = cwCavernRunner::run(macroUserFile,
                                        workDir.filePath(QStringLiteral("walls_macro_run2.3d")));
    CHECK(macroRun.hasError());
    CHECK(macroRun.errorMessage().contains(QStringLiteral("not defined"),
                                           Qt::CaseInsensitive));
    CHECK_FALSE(macroRun.errorMessage().contains(QStringLiteral("SRVPFX"),
                                                 Qt::CaseInsensitive));
}

TEST_CASE("cavern clears Walls project macros between runs",
          "[cwSurvex3DFileReader][Walls][Attach]") {
    // Regression: datain.c kept a second macro table for .wpj-level macros
    // that nothing ever cleared, so a later standalone .srv resolved $(loc)
    // from the earlier project and prefixed its stations with WPJPFX instead
    // of reporting the macro as undefined.
    const QString projectFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/walls_wpj_macro.wpj"));
    const QString macroUserFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/walls_use_macro.srv"));
    REQUIRE(QFile::exists(projectFile));
    REQUIRE(QFile::exists(macroUserFile));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    auto projectRun = cwCavernRunner::run(projectFile,
                                          workDir.filePath(QStringLiteral("walls_wpj_run1.3d")));
    CHECK_FALSE(projectRun.hasError());

    auto macroRun = cwCavernRunner::run(macroUserFile,
                                        workDir.filePath(QStringLiteral("walls_wpj_run2.3d")));
    CHECK(macroRun.hasError());
    CHECK(macroRun.errorMessage().contains(QStringLiteral("not defined"),
                                           Qt::CaseInsensitive));
    CHECK_FALSE(macroRun.errorMessage().contains(QStringLiteral("WPJPFX"),
                                                 Qt::CaseInsensitive));
}

TEST_CASE("cavern clears Walls project macros a fatal error leaves swapped aside",
          "[cwSurvex3DFileReader][Walls][Attach]") {
    // Regression: while a .wpj reads a member .srv the project macro table is
    // swapped aside, and a fatal error in that member longjmps past the swap
    // back. The next run then resolved $(loc) from the swapped-aside table,
    // prefixing its stations with WPJPFX instead of reporting the macro as
    // undefined.
    const QString projectFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/walls_wpj_fatal.wpj"));
    const QString macroUserFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/walls_use_macro.srv"));
    REQUIRE(QFile::exists(projectFile));
    REQUIRE(QFile::exists(macroUserFile));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    auto fatalRun = cwCavernRunner::run(projectFile,
                                        workDir.filePath(QStringLiteral("walls_wpj_fatal_run1.3d")));
    CHECK(fatalRun.hasError());
    CHECK(fatalRun.errorMessage().contains(QStringLiteral("Too many errors"),
                                           Qt::CaseInsensitive));

    auto macroRun = cwCavernRunner::run(macroUserFile,
                                        workDir.filePath(QStringLiteral("walls_wpj_fatal_run2.3d")));
    CHECK(macroRun.hasError());
    CHECK(macroRun.errorMessage().contains(QStringLiteral("not defined"),
                                           Qt::CaseInsensitive));
    CHECK_FALSE(macroRun.errorMessage().contains(QStringLiteral("WPJPFX"),
                                                 Qt::CaseInsensitive));
}

TEST_CASE("cavern forgets a fix with no coordinates between runs",
          "[cwSurvex3DFileReader]") {
    // Regression: cmd_fix remembers the first station fixed without
    // coordinates in function statics that live for the whole process, so a
    // second run fixing a different station at the origin reported
    // "Already had FIX command with no coordinates" and cited the first run's
    // file. Every re-solve of a project using the fix-at-origin convenience
    // failed after the first.
    const QString firstFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/fix_no_coords_a.svx"));
    const QString secondFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/fix_no_coords_c.svx"));
    REQUIRE(QFile::exists(firstFile));
    REQUIRE(QFile::exists(secondFile));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    auto firstRun = cwCavernRunner::run(firstFile,
                                        workDir.filePath(QStringLiteral("fix_run1.3d")));
    REQUIRE_FALSE(firstRun.hasError());

    auto secondRun = cwCavernRunner::run(secondFile,
                                         workDir.filePath(QStringLiteral("fix_run2.3d")));
    REQUIRE_FALSE(secondRun.hasError());
    REQUIRE(QFileInfo(secondRun.value().output3dPath).exists());

    cwSurvex3DFileReader reader;
    cwStationPositionLookup lookup = reader.readStationPositions(secondRun.value().output3dPath);
    CHECK(lookup.positions().size() == 2);
    CHECK(lookup.hasPosition(QStringLiteral("c")));
    CHECK(lookup.hasPosition(QStringLiteral("d")));
}
