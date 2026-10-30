//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwSurvex3DFileReader.h"
#include "cwCavernRunner.h"
#include "cwCavernNaming.h"
#include "cwStation.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QDir>
#include <QTextStream>

//Std includes
#ifndef Q_OS_WIN
#include <sys/resource.h>
#endif

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define CW_ADDRESS_SANITIZER 1
#  endif
#endif
#ifdef CW_ADDRESS_SANITIZER
extern "C" void __sanitizer_purge_allocator(void);
extern "C" size_t __sanitizer_get_current_allocated_bytes(void);
#endif

namespace {
    /// Hand freed memory back so that peak RSS reflects leaks rather than
    /// Address Sanitizer's quarantine, which parks every freed block.
    void releaseFreedMemory()
    {
#ifdef CW_ADDRESS_SANITIZER
        __sanitizer_purge_allocator();
#endif
    }

    /// Runs solve() a few times to warm caches, then reports how much measure()
    /// grows over measuredRuns further runs.
    template <typename Solve, typename Measure>
    qint64 growthOverRuns(int warmUpRuns, int measuredRuns, Solve solve, Measure measure)
    {
        for (int i = 0; i < warmUpRuns; ++i) {
            solve();
        }

        const qint64 baselineBytes = measure();

        for (int i = 0; i < measuredRuns; ++i) {
            solve();
        }

        return measure() - baselineBytes;
    }
}

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
    cwStationPositionLookup lookup = reader.readNetworkAndLookup(cavernResult.value().output3dPath).lookup;

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

TEST_CASE("cavern keeps the first of two Compass MAK fixed points for one station",
          "[cwSurvex3DFileReader]") {
    // Compass quietly accepts contradictory fixed points within one DAT's link
    // station list, so cavern (upstream f4704a03) warns and keeps the first.
    const QString driver = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/compass_mak_refix_driver.svx"));
    REQUIRE(QFile::exists(driver));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());
    const QString output3dPath = workDir.filePath(QStringLiteral("compass_mak_refix.3d"));

    auto cavernResult = cwCavernRunner::run(driver, output3dPath);
    REQUIRE(!cavernResult.hasError());
    CHECK(cavernResult.value().warningCount == 1);
    CHECK(cavernResult.value().logText.contains(QStringLiteral("already fixed")));
    REQUIRE(QFileInfo(output3dPath).exists());

    cwSurvex3DFileReader reader;
    auto parsed = reader.readNetworkAndLookup(output3dPath);
    CHECK(parsed.network.stations().size() == 3);
    CHECK(parsed.lookup.position(QStringLiteral("cave0.a.1")).x()
          == Catch::Approx(0.0).margin(0.01));
    CHECK(parsed.lookup.position(QStringLiteral("cave0.a.A3")).x()
          == Catch::Approx(6.10).margin(0.01));
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
    cwStationPositionLookup lookup = reader.readNetworkAndLookup(cleanRun.value().output3dPath).lookup;
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
    cwStationPositionLookup lookup = reader.readNetworkAndLookup(secondRun.value().output3dPath).lookup;
    CHECK(lookup.positions().size() == 2);
    CHECK(lookup.hasPosition(QStringLiteral("c")));
    CHECK(lookup.hasPosition(QStringLiteral("d")));
}

TEST_CASE("cavern reports the same deprecation warnings on every run",
          "[cwSurvex3DFileReader]") {
    // Each deprecation warning is capped at five hits per run, so the caps must
    // reset between runs for the same file to report the same count every time.
    const QString survexFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/deprecation_warnings.svx"));
    REQUIRE(QFile::exists(survexFile));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    auto firstRun = cwCavernRunner::run(survexFile,
                                        workDir.filePath(QStringLiteral("depr_run1.3d")));
    REQUIRE_FALSE(firstRun.hasError());
    CHECK(firstRun.value().warningCount > 0);
    CHECK(firstRun.value().logText.contains(QStringLiteral("deprecated")));

    auto secondRun = cwCavernRunner::run(survexFile,
                                         workDir.filePath(QStringLiteral("depr_run2.3d")));
    REQUIRE_FALSE(secondRun.hasError());
    CHECK(secondRun.value().warningCount == firstRun.value().warningCount);
    CHECK(secondRun.value().logText.contains(QStringLiteral("deprecated")));
}

TEST_CASE("cavern frees the survey graph between runs",
          "[cwSurvex3DFileReader]") {
#ifdef Q_OS_WIN
    SKIP("Peak resident set size is read with getrusage, which Windows lacks");
#else
    // cavern_cleanup_state() used to drop the survey graph on the floor, so
    // every run leaked one. Peak resident set size is the observable.
    constexpr int kStationCount = 4000;
    constexpr int kWarmUpRuns = 3;
    constexpr int kMeasuredRuns = 20;

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    const QString driverPath = workDir.filePath(QStringLiteral("graph_leak.svx"));
    {
        QFile driver(driverPath);
        REQUIRE(driver.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&driver);
        out << "*fix 0 0 0 0\n";
        out << "*data normal from to tape compass clino\n";
        for (int i = 0; i < kStationCount; ++i) {
            out << i << ' ' << (i + 1) << " 10.0 " << (i % 360) << " 0\n";
        }
    }

    const auto peakResidentBytes = []() -> qint64 {
        rusage usage;
        getrusage(RUSAGE_SELF, &usage);
#ifdef Q_OS_MACOS
        return static_cast<qint64>(usage.ru_maxrss);
#else
        return static_cast<qint64>(usage.ru_maxrss) * 1024;
#endif
    };

    // Re-solving writes over the same .3d, as a project re-solve does.
    const QString output3dPath = workDir.filePath(QStringLiteral("graph_leak.3d"));
    const auto solve = [&]() {
        auto result = cwCavernRunner::run(driverPath, output3dPath);
        REQUIRE_FALSE(result.hasError());
        releaseFreedMemory();
    };

    const qint64 growthBytes = growthOverRuns(kWarmUpRuns, kMeasuredRuns, solve, peakResidentBytes);
    INFO("peak RSS growth over " << kMeasuredRuns << " runs: " << growthBytes << " bytes");

    // Measured: ~6MB of growth while leaking, 32KB once cleanup frees the graph.
    constexpr qint64 kAllowedGrowthBytes = 4 * 1024 * 1024;
    CHECK(growthBytes < kAllowedGrowthBytes);
#endif
}

TEST_CASE("cavern keeps its message state to one copy across runs",
          "[cwSurvex3DFileReader]") {
#ifdef CW_ADDRESS_SANITIZER
    // msg_init() runs once per cavern_run() and used to strdup the program
    // name, the message-file search path and the stripped language tag afresh
    // every time, dropping the previous copies. Live heap bytes are the
    // observable: they must hold steady once the caches are warm.
    constexpr int kWarmUpRuns = 3;
    constexpr int kMeasuredRuns = 50;

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    const QString driverPath = workDir.filePath(QStringLiteral("msg_state.svx"));
    {
        QFile driver(driverPath);
        REQUIRE(driver.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&driver);
        out << "*fix a 0 0 0\n";
        out << "*data normal from to tape compass clino\n";
        out << "a b 10.0 0 0\n";
    }

    const QString output3dPath = workDir.filePath(QStringLiteral("msg_state.3d"));
    const auto solve = [&]() {
        auto result = cwCavernRunner::run(driverPath, output3dPath);
        REQUIRE_FALSE(result.hasError());
        releaseFreedMemory();
    };

    const auto liveHeapBytes = []() {
        return static_cast<qint64>(__sanitizer_get_current_allocated_bytes());
    };

    const qint64 growthBytes = growthOverRuns(kWarmUpRuns, kMeasuredRuns, solve, liveHeapBytes);
    INFO("live heap growth over " << kMeasuredRuns << " runs: " << growthBytes << " bytes");

    // Measured over 50 runs: 10700 bytes of growth while msg_init() re-copies
    // its strings (214 per run), 5750 bytes once it keeps the copies it
    // already made (115 per run, from elsewhere in the process). The budget
    // sits between the two so unrelated drift in that remainder has room.
    constexpr qint64 kAllowedGrowthBytesPerRun = 170;
    constexpr qint64 kAllowedGrowthBytes = kAllowedGrowthBytesPerRun * kMeasuredRuns;
    CHECK(growthBytes < kAllowedGrowthBytes);
#else
    SKIP("Live heap bytes are read from the Address Sanitizer allocator");
#endif
}

TEST_CASE("cavern recovers in the including file from an error after an *include",
          "[cwSurvex3DFileReader]") {
    // Regression: data_file() aims jbSkipLine at its own frame and leaves it
    // there when it returns, so after an *include the including file's next
    // recoverable error longjmps into the child's dead frame and resumes on
    // stack memory the parser has since reused. The driver's bad compass
    // reading sits below the *include, with a good leg under it, so recovery
    // has to land in the driver and carry on.
    const QString driver = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/include_then_error_driver.svx"));
    REQUIRE(QFile::exists(driver));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    auto cavernResult = cwCavernRunner::run(
        driver, workDir.filePath(QStringLiteral("include_then_error.3d")));

    // cavern deletes its output when a run reports errors, so the log is what
    // says how the parser recovered.
    REQUIRE(cavernResult.hasError());
    const QString log = cavernResult.errorMessage();
    INFO(log.toStdString());

    // The one error is the bad compass reading, reported against the driver.
    CHECK(log.contains(QStringLiteral("include_then_error_driver.svx")));
    CHECK(log.contains(QStringLiteral("bogus")));
    CHECK(log.contains(QStringLiteral("1 error")));

    // The run reaches the end of the driver: the two stations of the include
    // plus the driver's own three, joined by three legs.
    CHECK(log.contains(QStringLiteral("Survey contains 5 survey stations")));
    CHECK(log.contains(QStringLiteral("joined by 3 shots")));
}

TEST_CASE("cavern picks the same label separator for a Walls empty name on every run",
          "[cwSurvex3DFileReader][Walls]") {
    // Regression: the flag that claims ' ' for the Walls empty name lived in a
    // process-lifetime static, so run 2 could pick the space as the .3d separator.
    const QString driver = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/walls_empty_name_driver.svx"));
    REQUIRE(QFile::exists(driver));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());

    cwSurvex3DFileReader reader;

    auto firstRun = cwCavernRunner::run(driver,
                                        workDir.filePath(QStringLiteral("empty_name_run1.3d")));
    REQUIRE_FALSE(firstRun.hasError());
    cwStationPositionLookup firstLookup =
        reader.readNetworkAndLookup(firstRun.value().output3dPath).lookup;

    auto secondRun = cwCavernRunner::run(driver,
                                         workDir.filePath(QStringLiteral("empty_name_run2.3d")));
    REQUIRE_FALSE(secondRun.hasError());
    cwStationPositionLookup secondLookup =
        reader.readNetworkAndLookup(secondRun.value().output3dPath).lookup;

    CHECK(secondLookup.positions().keys() == firstLookup.positions().keys());
    CHECK(secondLookup.hasPosition(QStringLiteral("pfx.empty name")));
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

namespace {
    /// Writes each file into a temporary directory, runs cavern on the first
    /// one, and reads the .3d it writes.
    cwSurvex3DFileReader::NetworkAndLookup solveSurvexFiles(
        const QList<QPair<QString, QByteArray>>& files)
    {
        QTemporaryDir dir;
        REQUIRE(dir.isValid());
        for (const auto& file : files) {
            QFile out(dir.filePath(file.first));
            REQUIRE(out.open(QIODevice::WriteOnly));
            out.write(file.second);
        }

        const QString svxPath = dir.filePath(files.constFirst().first);
        const auto cavernResult = cwCavernRunner::run(
            svxPath, dir.filePath(QFileInfo(svxPath).completeBaseName() + QStringLiteral(".3d")));
        REQUIRE_FALSE(cavernResult.hasError());

        cwSurvex3DFileReader reader;
        return reader.readNetworkAndLookup(cavernResult.value().output3dPath);
    }

    /// The station's neighbors, sorted and comma-joined, so a failure prints them.
    std::string neighborsOf(const cwSurveyNetwork& network, const char* station)
    {
        QStringList neighbors = network.neighbors(QString::fromLatin1(station));
        neighbors.sort();
        return neighbors.join(QStringLiteral(", ")).toStdString();
    }

    /// Every edge whose two ends sit under different top-level surveys,
    /// sorted and joined, so a failure prints them.
    std::string crossSurveyEdges(const cwSurveyNetwork& network)
    {
        QStringList edges;
        const QStringList stations = network.stations();
        for (const QString& station : stations) {
            const QStringList neighbors = network.neighbors(station);
            for (const QString& neighbor : neighbors) {
                if (station < neighbor
                    && cwCavernNaming::scopeHeadOf(station) != cwCavernNaming::scopeHeadOf(neighbor)) {
                    edges.append(station + QStringLiteral(" - ") + neighbor);
                }
            }
        }
        edges.sort();
        return edges.join(QStringLiteral("; ")).toStdString();
    }

    QString describe(const cwSurveyNetwork& network)
    {
        QStringList stations = network.stations();
        stations.sort();
        QStringList lines;
        for (const QString& station : std::as_const(stations)) {
            QStringList neighbors = network.neighbors(station);
            neighbors.sort();
            lines.append(station + QStringLiteral(" -> ") + neighbors.join(QStringLiteral(", ")));
        }
        return lines.join(QLatin1Char('\n'));
    }

    // Two caves shaped like a CaveWhere driver: each cave wraps its trip in an
    // anonymous *begin, and a region equate ties alpha.a2 to beta.b1.
    constexpr const char* kEquatedCaves =
        "*begin alpha\n"
        "*fix a1 0 0 0\n"
        "*begin\n"
        "*data normal from to tape compass clino\n"
        "a1 a2 10 0 0\n"
        "*end\n"
        "*end alpha\n"
        "*begin beta\n"
        "*begin\n"
        "*data normal from to tape compass clino\n"
        "b1 b2 10 0 0\n"
        "*end\n"
        "*end beta\n"
        "*equate alpha.a2 beta.b1\n";
}

TEST_CASE("cwSurvex3DFileReader names each leg end under the leg's own survey across a region equate",
          "[cwSurvex3DFileReader]") {
    const auto parsed = solveSurvexFiles({{QStringLiteral("equated.svx"), kEquatedCaves}});
    INFO("network:\n" << describe(parsed.network).toStdString());

    CHECK(neighborsOf(parsed.network, "alpha.a1") == "alpha.a2");
    CHECK(neighborsOf(parsed.network, "alpha.a2") == "alpha.a1");
    CHECK(neighborsOf(parsed.network, "beta.b1") == "beta.b2");
    CHECK(neighborsOf(parsed.network, "beta.b2") == "beta.b1");
    CHECK(crossSurveyEdges(parsed.network) == "");
}

TEST_CASE("cwSurvex3DFileReader keeps each survey's names on legs whose stations coincide",
          "[cwSurvex3DFileReader]") {
    // No equate: beta's fix just happens to land on alpha.a2.
    const auto parsed = solveSurvexFiles({{QStringLiteral("coincide.svx"),
                                           "*begin alpha\n"
                                           "*fix a1 0 0 0\n"
                                           "*data normal from to tape compass clino\n"
                                           "a1 a2 10 0 0\n"
                                           "*end alpha\n"
                                           "*begin beta\n"
                                           "*fix b1 0 10 0\n"
                                           "*data normal from to tape compass clino\n"
                                           "b1 b2 10 0 0\n"
                                           "*end beta\n"}});
    INFO("network:\n" << describe(parsed.network).toStdString());

    CHECK(neighborsOf(parsed.network, "alpha.a2") == "alpha.a1");
    CHECK(neighborsOf(parsed.network, "beta.b1") == "beta.b2");
    CHECK(crossSurveyEdges(parsed.network) == "");
}

TEST_CASE("cwSurvex3DFileReader keys leg errors and splays at an equated station by their own survey",
          "[cwSurvex3DFileReader]") {
    // A misclosed triangle in each cave, both through the equated point, with
    // a splay to the anonymous wall station off it from each cave.
    const auto parsed = solveSurvexFiles({{QStringLiteral("loops.svx"),
                                           "*begin alpha\n"
                                           "*fix a1 0 0 0\n"
                                           "*data normal from to tape compass clino\n"
                                           "a1 a2 10 0 0\n"
                                           "a2 a3 10 90 0\n"
                                           "a3 a1 15 225 0\n"
                                           "*flags splay\n"
                                           "a2 .. 2 270 0\n"
                                           "*end alpha\n"
                                           "*begin beta\n"
                                           "*data normal from to tape compass clino\n"
                                           "b1 b2 10 0 0\n"
                                           "b2 b3 10 270 0\n"
                                           "b3 b1 15 135 0\n"
                                           "*flags splay\n"
                                           "b1 .. 3 90 0\n"
                                           "*end beta\n"
                                           "*equate alpha.a2 beta.b1\n"}});

    QStringList errorKeys;
    QStringList crossSurveyErrorKeys;
    for (auto it = parsed.legErrors.constBegin(); it != parsed.legErrors.constEnd(); ++it) {
        const QString errorKey = it.key().first + QStringLiteral(" - ") + it.key().second;
        errorKeys.append(errorKey);
        if (cwCavernNaming::scopeHeadOf(it.key().first) != cwCavernNaming::scopeHeadOf(it.key().second)) {
            crossSurveyErrorKeys.append(errorKey);
        }
    }
    errorKeys.sort();
    crossSurveyErrorKeys.sort();
    INFO("leg error keys: " << errorKeys.join(QStringLiteral(", ")).toStdString());

    const auto key = [](const char* from, const char* to) {
        return cwSurvex3DFileReader::legKey(QString::fromLatin1(from), QString::fromLatin1(to));
    };
    CHECK(parsed.legErrors.contains(key("alpha.a1", "alpha.a2")));
    CHECK(parsed.legErrors.contains(key("alpha.a2", "alpha.a3")));
    CHECK(parsed.legErrors.contains(key("alpha.a3", "alpha.a1")));
    CHECK(parsed.legErrors.contains(key("beta.b1", "beta.b2")));
    CHECK(parsed.legErrors.contains(key("beta.b2", "beta.b3")));
    CHECK(parsed.legErrors.contains(key("beta.b3", "beta.b1")));

    CHECK(crossSurveyErrorKeys.join(QStringLiteral("; ")).toStdString() == "");

    QStringList splayAnchors;
    for (auto it = parsed.splayTips.constBegin(); it != parsed.splayTips.constEnd(); ++it) {
        splayAnchors.append(it.key() + QStringLiteral(" x") + QString::number(it.value().size()));
    }
    splayAnchors.sort();
    INFO("splay anchors: " << splayAnchors.join(QStringLiteral(", ")).toStdString());
    CHECK(parsed.splayTips.value(QStringLiteral("alpha.a2")).size() == 1);
    CHECK(parsed.splayTips.value(QStringLiteral("beta.b1")).size() == 1);
}

TEST_CASE("cwSurvex3DFileReader names a parent survey's leg into a child station by the parent's station",
          "[cwSurvex3DFileReader]") {
    // Cavern labels the leg "leaf.a2 m1" written in mid with leaf's prefix,
    // and m1 shares its point with other.c1 through the equate. The leg ends
    // at mid's own m1, and other's leg keeps c1.
    const auto parsed = solveSurvexFiles({{QStringLiteral("nested.svx"),
                                           "*begin top\n"
                                           "*begin mid\n"
                                           "*fix leaf.a1 0 0 0\n"
                                           "*begin leaf\n"
                                           "*data normal from to tape compass clino\n"
                                           "a1 a2 10 0 0\n"
                                           "*end leaf\n"
                                           "*begin other\n"
                                           "*data normal from to tape compass clino\n"
                                           "c1 c2 10 270 0\n"
                                           "*end other\n"
                                           "*data normal from to tape compass clino\n"
                                           "leaf.a2 m1 5 270 0\n"
                                           "*equate m1 other.c1\n"
                                           "*end mid\n"
                                           "*end top\n"}});
    INFO("network:\n" << describe(parsed.network).toStdString());

    CHECK(neighborsOf(parsed.network, "top.mid.m1") == "top.mid.leaf.a2");
    CHECK(neighborsOf(parsed.network, "top.mid.other.c1") == "top.mid.other.c2");
}

TEST_CASE("cwSurvex3DFileReader names each leg end under its own survey with a non-dot separator",
          "[cwSurvex3DFileReader][Walls]") {
    // A Walls include makes cavern join labels with ':', on the NODE labels and
    // the LINE survey prefixes alike. Both caves sit one level down, so each
    // leg's survey prefix ("site:alpha", "site:beta") carries the separator.
    const auto parsed = solveSurvexFiles(
        {{QStringLiteral("separator.svx"),
          "*include walls_leg.srv\n"
          "*begin site\n"
          "*begin alpha\n"
          "*fix a1 0 0 0\n"
          "*data normal from to tape compass clino\n"
          "a1 a2 10 0 0\n"
          "*end alpha\n"
          "*begin beta\n"
          "*data normal from to tape compass clino\n"
          "b1 b2 10 0 0\n"
          "*end beta\n"
          "*equate alpha.a2 beta.b1\n"
          "*end site\n"},
         {QStringLiteral("walls_leg.srv"),
          "#UNITS D=Meters A=Degrees V=Degrees\n"
          "#Fix WA 100 100 0\n"
          "WA\tWB\t10.0\t0\t0\n"}});
    INFO("network:\n" << describe(parsed.network).toStdString());

    CHECK(neighborsOf(parsed.network, "site.alpha.a1") == "site.alpha.a2");
    CHECK(neighborsOf(parsed.network, "site.alpha.a2") == "site.alpha.a1");
    CHECK(neighborsOf(parsed.network, "site.beta.b1") == "site.beta.b2");
    CHECK(neighborsOf(parsed.network, "site.beta.b2") == "site.beta.b1");
}

TEST_CASE("cwSurvex3DFileReader collects the stations cavern flags as entrances",
          "[cwSurvex3DFileReader]") {
    const QString survexDataFile = testcasesDatasetSourcePath(
        QStringLiteral("test_cwSurvex3DFileReader/entrance.svx"));
    REQUIRE(QFile::exists(survexDataFile));

    QTemporaryDir workDir;
    REQUIRE(workDir.isValid());
    const QString output3dPath = workDir.filePath(QStringLiteral("entrance.3d"));

    auto cavernResult = cwCavernRunner::run(survexDataFile, output3dPath);
    REQUIRE_FALSE(cavernResult.hasError());
    CHECK(cavernResult.value().warningCount == 0);
    REQUIRE(QFileInfo(cavernResult.value().output3dPath).exists());

    cwSurvex3DFileReader reader;
    const auto parsed = reader.readNetworkAndLookup(cavernResult.value().output3dPath);

    QStringList entrances = parsed.entranceStations;
    entrances.sort();
    INFO("entrances: " << entrances.join(QStringLiteral(", ")).toStdString());
    CHECK(entrances == QStringList({QStringLiteral("cave.a1"), QStringLiteral("cave.a3")}));
    CHECK(parsed.fixedStations == QStringList({cwStation::canonicalKey(QStringLiteral("cave.a1"))}));
}

TEST_CASE("cwSurvex3DFileReader orders a leg between keys and keeps their case",
          "[cwSurvex3DFileReader]") {
    const auto describeLeg = [](const QPair<QString, QString>& leg) {
        return (leg.first + QStringLiteral(" - ") + leg.second).toStdString();
    };
    CHECK(describeLeg(cwSurvex3DFileReader::legKeyOfKeys(QStringLiteral("p2"), QStringLiteral("P2")))
          == "P2 - p2");
    CHECK(describeLeg(cwSurvex3DFileReader::legKeyOfKeys(QStringLiteral("P2"), QStringLiteral("p2")))
          == "P2 - p2");
}

namespace {
    QByteArray externalCenterlineBytes(const QString& fileName)
    {
        QFile file(testcasesDatasetSourcePath(QStringLiteral("external-centerlines/%1").arg(fileName)));
        REQUIRE(file.open(QIODevice::ReadOnly));
        return file.readAll();
    }

    /// The lookup's keys, sorted and comma-joined, so a failure prints them.
    std::string sortedKeys(const cwStationPositionLookup& lookup)
    {
        QStringList keys = lookup.positions().keys();
        keys.sort();
        return keys.join(QStringLiteral(", ")).toStdString();
    }

    // The 100 ft between each member of compass_case_pairs.dat's pairs.
    constexpr double kCasePairSeparationMeters = 30.48;
}

TEST_CASE("cwSurvex3DFileReader keeps each member of a Compass case pair as its own station",
          "[cwSurvex3DFileReader]") {
    // Cavern reads a .dat with case folding off, so P2 and p2 are two stations
    // 100 ft apart.
    const auto parsed = solveSurvexFiles(
        {{QStringLiteral("case_pairs.svx"), "*include compass_case_pairs.dat\n"},
         {QStringLiteral("compass_case_pairs.dat"),
          externalCenterlineBytes(QStringLiteral("compass_case_pairs.dat"))}});
    INFO("network:\n" << describe(parsed.network).toStdString());

    CHECK(sortedKeys(parsed.lookup) == "P1, P2, P3, p1, p2, p3, q1");

    const float separation = (parsed.lookup.position(QStringLiteral("P2"))
                              - parsed.lookup.position(QStringLiteral("p2"))).length();
    CHECK(separation == Catch::Approx(kCasePairSeparationMeters).margin(0.01));

    CHECK(neighborsOf(parsed.network, "P2") == "P1, P3");
    CHECK(neighborsOf(parsed.network, "p2") == "p1, p3");
    CHECK(neighborsOf(parsed.network, "p1") == "P1, p2");
    CHECK(neighborsOf(parsed.network, "q1") == "P3");

    CHECK(parsed.lookup.key(QStringLiteral("Q1")).toStdString() == "q1");
    CHECK(parsed.spellings.value(QStringLiteral("q1")).toStdString() == "Q1");
    CHECK(parsed.spellings.value(QStringLiteral("P2")).toStdString() == "P2");
    CHECK(parsed.spellings.value(QStringLiteral("p2")).toStdString() == "p2");
}

TEST_CASE("cwSurvex3DFileReader lowercases the survey levels of a preserved case pair",
          "[cwSurvex3DFileReader]") {
    // A misclosed loop through the pair, a splay off each member, and a fix on
    // one, so every product of the read names its member.
    const auto parsed = solveSurvexFiles({{QStringLiteral("preserved.svx"),
                                           "*case preserve\n"
                                           "*begin Foo\n"
                                           "*fix P2 0 0 0\n"
                                           "*data normal from to tape compass clino\n"
                                           "P1 P2 10 0 0\n"
                                           "P2 p2 10 90 0\n"
                                           "p2 P1 15 225 0\n"
                                           "*flags splay\n"
                                           "P2 .. 2 270 0\n"
                                           "p2 .. 3 90 0\n"
                                           "*end Foo\n"}});
    INFO("network:\n" << describe(parsed.network).toStdString());

    CHECK(sortedKeys(parsed.lookup) == "foo.P2, foo.p1, foo.p2");
    CHECK(neighborsOf(parsed.network, "foo.P2") == "foo.p1, foo.p2");
    CHECK(neighborsOf(parsed.network, "foo.p2") == "foo.P2, foo.p1");
    CHECK(parsed.fixedStations.join(QStringLiteral(", ")).toStdString() == "foo.P2");

    QStringList errorKeys;
    for (auto it = parsed.legErrors.constBegin(); it != parsed.legErrors.constEnd(); ++it) {
        errorKeys.append(it.key().first + QStringLiteral(" - ") + it.key().second);
    }
    errorKeys.sort();
    CHECK(errorKeys.join(QStringLiteral("; ")).toStdString()
          == "foo.P2 - foo.p1; foo.P2 - foo.p2; foo.p1 - foo.p2");

    QStringList splayAnchors;
    for (auto it = parsed.splayTips.constBegin(); it != parsed.splayTips.constEnd(); ++it) {
        splayAnchors.append(it.key() + QStringLiteral(" x") + QString::number(it.value().size()));
    }
    splayAnchors.sort();
    CHECK(splayAnchors.join(QStringLiteral(", ")).toStdString() == "foo.P2 x1, foo.p2 x1");

    CHECK(parsed.spellings.value(QStringLiteral("foo.p1")).toStdString() == "foo.P1");
}

TEST_CASE("cwSurvex3DFileReader keys case variants equated at one point as one station",
          "[cwSurvex3DFileReader][Equate]") {
    // The driver's p9 is lowercased; the preserved block's P9 keeps its case.
    // The equate puts both labels at one coordinate, so they are one station.
    const auto parsed = solveSurvexFiles({{QStringLiteral("equated_case.svx"),
                                           "*begin cave\n"
                                           "*data normal from to tape compass clino\n"
                                           "n1 p9 10 0 0\n"
                                           "*begin\n"
                                           "*case preserve\n"
                                           "*data normal from to tape compass clino\n"
                                           "P9 Z1 5 90 0\n"
                                           "*equate p9 P9\n"
                                           "*end\n"
                                           "*end cave\n"}});
    INFO("network:\n" << describe(parsed.network).toStdString());

    CHECK(sortedKeys(parsed.lookup) == "cave.n1, cave.p9, cave.z1");
    CHECK(neighborsOf(parsed.network, "cave.p9") == "cave.n1, cave.z1");
    CHECK(parsed.spellings.value(QStringLiteral("cave.p9")).toStdString() == "cave.p9");
}

TEST_CASE("cwSurvex3DFileReader keeps each member of a Walls case pair under a lowercase level",
          "[cwSurvex3DFileReader][CasePairs]") {
    // Cavern labels them cave:XY:P2 and so on. BB2 and Bb2 are a pair with no
    // lowercase member, so bB2 names neither.
    const auto parsed = solveSurvexFiles(
        {{QStringLiteral("walls_pairs.svx"), "*begin cave\n*include walls_case_pairs.srv\n*end cave\n"},
         {QStringLiteral("walls_case_pairs.srv"),
          externalCenterlineBytes(QStringLiteral("walls_case_pairs.srv"))}});
    INFO("network:\n" << describe(parsed.network).toStdString());

    CHECK(sortedKeys(parsed.lookup) == "cave.xy.BB2, cave.xy.Bb2, cave.xy.P1, cave.xy.P2, cave.xy.p1, cave.xy.p2");
    CHECK(neighborsOf(parsed.network, "cave.xy.P2") == "cave.xy.BB2, cave.xy.P1");
    CHECK(neighborsOf(parsed.network, "cave.xy.p2") == "cave.xy.Bb2, cave.xy.p1");
    CHECK(neighborsOf(parsed.network, "cave.xy.P1") == "cave.xy.P2, cave.xy.p1");
    CHECK(neighborsOf(parsed.network, "cave.xy.p1") == "cave.xy.P1, cave.xy.p2");
    CHECK(parsed.lookup.key(QStringLiteral("cave.xy.bB2")).isEmpty());
}

TEST_CASE("cwSurvex3DFileReader keys a Survex *case toupper file in lowercase",
          "[cwSurvex3DFileReader][CasePairs]") {
    // Cavern labels them FOO.P1 and so on: no two labels differ only in case.
    const auto parsed = solveSurvexFiles(
        {{QStringLiteral("upper.svx"), "*include survex_case_upper.svx\n"},
         {QStringLiteral("survex_case_upper.svx"),
          externalCenterlineBytes(QStringLiteral("survex_case_upper.svx"))}});
    INFO("network:\n" << describe(parsed.network).toStdString());

    CHECK(sortedKeys(parsed.lookup) == "foo.p1, foo.p2, foo.p3");
    CHECK(neighborsOf(parsed.network, "foo.p2") == "foo.p1, foo.p3");
    CHECK(parsed.spellings.value(QStringLiteral("foo.p2")).toStdString() == "foo.P2");
}
