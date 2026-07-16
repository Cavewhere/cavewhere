/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwExternalCenterlineScanner.h"

// Test helpers
#include "LoadProjectHelper.h"

// Qt
#include <QByteArray>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>

namespace {

using cwExternalCenterlineScanner::Format;
using cwExternalCenterlineScanner::ScanResult;

constexpr const char* kSurvexExtension = ".svx";

QString datasetExternalCenterlinePath(const QString& fileName)
{
    // Use the in-source path (no copy) so the scanner can see the
    // sibling fixture files (entrance.svx, passage.svx, cycle_b.svx, ...).
    // testcasesDatasetPath copies only the single requested file.
    return testcasesDatasetSourcePath(QStringLiteral("external-centerlines/%1").arg(fileName));
}

QString tempPath(const QTemporaryDir& tempDir, const QString& fileName)
{
    return QDir(tempDir.path()).absoluteFilePath(fileName);
}

QString writeUtf8File(const QString& path, const QByteArray& contents)
{
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    REQUIRE(file.open(QFile::WriteOnly));
    file.write(contents);
    file.close();
    return path;
}

bool filesystemIsCaseSensitive(const QTemporaryDir& tempDir)
{
    // Probe by writing a lower-case file and looking for it via an
    // upper-case path. macOS APFS default is case-INSENSITIVE; Linux
    // ext4 default is case-sensitive; Windows NTFS is case-
    // insensitive by default. Gate the case-fallback test on this so
    // it never runs on a case-insensitive FS where the regular
    // lookup already succeeds.
    const QString lower = tempPath(tempDir, QStringLiteral("__cw_probe.txt"));
    {
        QFile probe(lower);
        REQUIRE(probe.open(QFile::WriteOnly));
        probe.close();
    }
    const QString upper = tempPath(tempDir, QStringLiteral("__CW_PROBE.txt"));
    const bool sensitive = !QFileInfo::exists(upper);
    QFile::remove(lower);
    return sensitive;
}

bool hasFileNameInDeps(const ScanResult& result, const QString& fileName)
{
    for (const QString& path : result.dependencies) {
        if (QFileInfo(path).fileName() == fileName) {
            return true;
        }
    }
    return false;
}

bool anyWarningContains(const ScanResult& result, const QString& needle)
{
    for (const QString& warning : result.warnings) {
        if (warning.contains(needle)) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("formatFor maps each known extension and rejects unknown", "[Scanner][Survex]")
{
    using namespace cwExternalCenterlineScanner;
    CHECK(formatFor(QStringLiteral("foo.svx")) == Format::Survex);
    CHECK(formatFor(QStringLiteral("FOO.SVX")) == Format::Survex);  // case-insensitive
    CHECK(formatFor(QStringLiteral("foo.dat")) == Format::Compass);
    CHECK(formatFor(QStringLiteral("foo.mak")) == Format::Compass);
    CHECK(formatFor(QStringLiteral("foo.wpj")) == Format::Walls);
    CHECK(formatFor(QStringLiteral("foo.srv")) == Format::Walls);
    CHECK(formatFor(QStringLiteral("foo")) == Format::Unknown);
    CHECK(formatFor(QStringLiteral("foo.unknown")) == Format::Unknown);
}

TEST_CASE("scan rejects unknown extension", "[Scanner][Survex]")
{
    auto result = cwExternalCenterlineScanner::scan(QStringLiteral("/tmp/foo.unknown"));
    CHECK(result.hasError());
}

TEST_CASE("scanSurvex on bare file returns just itself", "[Scanner][Survex]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 1);
    CHECK(QFileInfo(scan.dependencies.first()).fileName()
          == QStringLiteral("survex_simple.svx"));
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("scanSurvex walks a 3-deep *include chain", "[Scanner][Survex]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("survex_nested.svx"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 3);
    CHECK(hasFileNameInDeps(scan, QStringLiteral("survex_nested.svx")));
    CHECK(hasFileNameInDeps(scan, QStringLiteral("entrance.svx")));
    CHECK(hasFileNameInDeps(scan, QStringLiteral("passage.svx")));
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("scanSurvex auto-appends .svx on bareword includes", "[Scanner][Survex]")
{
    // Use a self-contained driver with ONLY a bareword include, so this
    // test isolates the auto-append behaviour from the nested chain.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString driverPath = tempPath(tempDir, QStringLiteral("bareword.svx"));
    writeUtf8File(driverPath,
                  QByteArrayLiteral("*include bareword_target\n"));
    const QString targetPath = tempPath(tempDir, QStringLiteral("bareword_target.svx"));
    writeUtf8File(targetPath,
                  QByteArrayLiteral("*begin BarewordTarget\n*end BarewordTarget\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(driverPath);
    REQUIRE_FALSE(result.hasError());
    CHECK(hasFileNameInDeps(result.value(), QStringLiteral("bareword.svx")));
    CHECK(hasFileNameInDeps(result.value(), QStringLiteral("bareword_target.svx")));
}

TEST_CASE("scanSurvex catches a mutual *include cycle", "[Scanner][Survex]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("survex_cycle_a.svx"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 2);
    CHECK(hasFileNameInDeps(scan, QStringLiteral("survex_cycle_a.svx")));
    CHECK(hasFileNameInDeps(scan, QStringLiteral("survex_cycle_b.svx")));
    CHECK(anyWarningContains(scan, QStringLiteral("circular include")));
}

TEST_CASE("scanSurvex warns on a missing *include target without failing the scan",
          "[Scanner][Survex]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString driverPath = tempPath(tempDir, QStringLiteral("driver.svx"));
    writeUtf8File(driverPath,
                  QByteArrayLiteral("*include \"nope.svx\"\n*include \"present.svx\"\n"));
    const QString presentPath = tempPath(tempDir, QStringLiteral("present.svx"));
    writeUtf8File(presentPath, QByteArrayLiteral("*begin Present\n*end Present\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(driverPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 2);
    CHECK(hasFileNameInDeps(scan, QStringLiteral("driver.svx")));
    CHECK(hasFileNameInDeps(scan, QStringLiteral("present.svx")));
    CHECK(anyWarningContains(scan, QStringLiteral("missing *include target")));
}

TEST_CASE("scanSurvex treats a commented *include as text, not as a directive",
          "[Scanner][Survex]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString driverPath = tempPath(tempDir, QStringLiteral("commented-driver.svx"));
    writeUtf8File(driverPath,
                  QByteArrayLiteral("; *include \"nope.svx\"\n*data normal from to tape\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(driverPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 1);
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("scanSurvex consumes a UTF-8 byte-order mark", "[Scanner][Survex]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString driverPath = tempPath(tempDir, QStringLiteral("bom-driver.svx"));
    QByteArray contents;
    contents.append(QByteArray::fromHex("EFBBBF"));  // UTF-8 BOM
    contents.append("*begin BomDriver\n*end BomDriver\n");
    writeUtf8File(driverPath, contents);

    auto result = cwExternalCenterlineScanner::scanSurvex(driverPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 1);
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("scanSurvex falls back to Latin-1 on invalid UTF-8 and warns",
          "[Scanner][Survex]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString driverPath = tempPath(tempDir, QStringLiteral("latin1-driver.svx"));
    // 0xE9 + space is invalid UTF-8 (E9 starts a 3-byte sequence
    // requiring two continuation bytes >= 0x80) but valid Latin-1
    // ('é' followed by a space).
    QByteArray contents;
    contents.append("; comment ");
    contents.append(static_cast<char>(0xE9));
    contents.append(" trailing\n");
    contents.append("*begin Latin1\n*end Latin1\n");
    writeUtf8File(driverPath, contents);

    auto result = cwExternalCenterlineScanner::scanSurvex(driverPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 1);
    CHECK(anyWarningContains(scan, QStringLiteral("Latin-1")));
}

TEST_CASE("scanSurvex case-fallback resolves miscased include on case-sensitive FS",
          "[Scanner][Survex]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    if (!filesystemIsCaseSensitive(tempDir)) {
        // macOS / Windows default FS is case-insensitive; the
        // literal lookup already succeeds and the fallback never
        // triggers. Skip cleanly rather than asserting platform-
        // specific behaviour.
        return;
    }

    const QString driverPath = tempPath(tempDir, QStringLiteral("case-driver.svx"));
    writeUtf8File(driverPath,
                  QByteArrayLiteral("*include \"Foo.svx\"\n"));  // capitalised
    const QString targetPath = tempPath(tempDir, QStringLiteral("foo.svx"));
    writeUtf8File(targetPath, QByteArrayLiteral("*begin Foo\n*end Foo\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(driverPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 2);
    CHECK(hasFileNameInDeps(scan, QStringLiteral("case-driver.svx")));
    CHECK(hasFileNameInDeps(scan, QStringLiteral("foo.svx")));
    CHECK(anyWarningContains(scan, QStringLiteral("case-fallback")));
}

TEST_CASE("scanSurvex returns an error when the entry file does not exist",
          "[Scanner][Survex]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString missing = tempPath(tempDir, QStringLiteral("missing.svx"));
    REQUIRE_FALSE(QFileInfo::exists(missing));

    auto result = cwExternalCenterlineScanner::scanSurvex(missing);
    CHECK(result.hasError());
    Q_UNUSED(kSurvexExtension);
}

TEST_CASE("scan dispatches Survex entry files through scanSurvex",
          "[Scanner][Survex]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    auto result = cwExternalCenterlineScanner::scan(path);
    REQUIRE_FALSE(result.hasError());
    CHECK(result.value().dependencies.size() == 1);
}

TEST_CASE("scanCompass on a bare .dat returns just itself", "[Scanner][Compass]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("compass_simple.dat"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanCompass(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 1);
    CHECK(QFileInfo(scan.dependencies.first()).fileName()
          == QStringLiteral("compass_simple.dat"));
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("scanCompass on a .mak resolves every '#' reference", "[Scanner][Compass]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("compass_multi.mak"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanCompass(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 3);
    CHECK(hasFileNameInDeps(scan, QStringLiteral("compass_multi.mak")));
    CHECK(hasFileNameInDeps(scan, QStringLiteral("compass_simple.dat")));
    CHECK(hasFileNameInDeps(scan, QStringLiteral("compass_other.dat")));
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("scanCompass warns on a missing '#' reference without failing the scan",
          "[Scanner][Compass]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString makPath = tempPath(tempDir, QStringLiteral("driver.mak"));
    writeUtf8File(makPath,
                  QByteArrayLiteral("/ project\n"
                                    "&UTM;\n"
                                    "#missing.dat;\n"));

    auto result = cwExternalCenterlineScanner::scanCompass(makPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 1);
    CHECK(anyWarningContains(scan, QStringLiteral("missing Compass reference")));
}

TEST_CASE("scanCompass accepts quoted .mak references", "[Scanner][Compass]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString datPath = tempPath(tempDir, QStringLiteral("spaced name.dat"));
    writeUtf8File(datPath, QByteArrayLiteral("anything\n"));
    const QString makPath = tempPath(tempDir, QStringLiteral("quoted.mak"));
    writeUtf8File(makPath,
                  QByteArrayLiteral("/ Compass project with a quoted reference\n"
                                    "#\"spaced name.dat\",A1,A2;\n"));

    auto result = cwExternalCenterlineScanner::scanCompass(makPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 2);
    CHECK(hasFileNameInDeps(scan, QStringLiteral("quoted.mak")));
    CHECK(hasFileNameInDeps(scan, QStringLiteral("spaced name.dat")));
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("scanCompass ignores '/'-prefixed comment lines", "[Scanner][Compass]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString makPath = tempPath(tempDir, QStringLiteral("comments.mak"));
    writeUtf8File(makPath,
                  QByteArrayLiteral("/#commented.dat,A,B;\n"
                                    "/ also a comment;\n"));

    auto result = cwExternalCenterlineScanner::scanCompass(makPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 1);
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("scanWalls on a bare .srv returns just itself", "[Scanner][Walls]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("MAIN.SRV"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanWalls(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 1);
    CHECK(QFileInfo(scan.dependencies.first()).fileName().toUpper()
          == QStringLiteral("MAIN.SRV"));
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("scanWalls on a .wpj resolves its .SURVEY entries", "[Scanner][Walls]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("walls_simple.wpj"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanWalls(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    // The .wpj entry plus its single MAIN.SRV survey -> two files.
    CHECK(scan.dependencies.size() == 2);
    CHECK(hasFileNameInDeps(scan, QStringLiteral("walls_simple.wpj")));
    bool sawSurvey = false;
    for (const QString& dep : scan.dependencies) {
        if (QFileInfo(dep).fileName().toUpper() == QStringLiteral("MAIN.SRV")) {
            sawSurvey = true;
            break;
        }
    }
    CHECK(sawSurvey);
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("scan dispatches each format through its matching scanner",
          "[Scanner][CrossFormat]")
{
    using namespace cwExternalCenterlineScanner;
    {
        auto result = scan(datasetExternalCenterlinePath(QStringLiteral("compass_simple.dat")));
        REQUIRE_FALSE(result.hasError());
        CHECK(result.value().dependencies.size() == 1);
    }
    {
        auto result = scan(datasetExternalCenterlinePath(QStringLiteral("walls_simple.wpj")));
        REQUIRE_FALSE(result.hasError());
        CHECK(result.value().dependencies.size() == 2);
    }
}

TEST_CASE("scanSurvex pulls in a Compass .dat via *include", "[Scanner][CrossFormat]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("cross_format.svx"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 2);
    CHECK(hasFileNameInDeps(scan, QStringLiteral("cross_format.svx")));
    CHECK(hasFileNameInDeps(scan, QStringLiteral("compass_simple.dat")));
    CHECK(scan.warnings.isEmpty());
}

TEST_CASE("seededMetadata populates date, team, and declination from a Survex entry",
          "[Scanner][Metadata]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("survex_with_metadata.svx"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.warnings.isEmpty());

    REQUIRE(scan.seededMetadata.date.has_value());
    CHECK(scan.seededMetadata.date.value() == QDate(2025, 6, 1));
    REQUIRE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.declination.value() == 7.2);
    // One entry per *team line
    CHECK(scan.seededMetadata.team
          == QStringList({QStringLiteral("Alice"), QStringLiteral("Bob")}));
}

TEST_CASE("seededMetadata stays empty for a Survex file without directives",
          "[Scanner][Metadata]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("survex_no_metadata.svx"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.warnings.isEmpty());
    CHECK_FALSE(scan.seededMetadata.date.has_value());
    CHECK_FALSE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.team.isEmpty());
}

TEST_CASE("seededMetadata handles quoted *team names and dotted *date",
          "[Scanner][Metadata]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("quoted-team.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin QuotedTeam\n"
                                    "*date 2025.06.01\n"
                                    "*team \"Alice Smith\" notes\n"
                                    "*team Bob\n"
                                    "*data normal from to tape compass clino\n"
                                    "A1 A2 10.0 0 0\n"
                                    "*end QuotedTeam\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    REQUIRE(scan.seededMetadata.date.has_value());
    CHECK(scan.seededMetadata.date.value() == QDate(2025, 6, 1));
    CHECK(scan.seededMetadata.team
          == QStringList({QStringLiteral("Alice Smith"), QStringLiteral("Bob")}));
}

TEST_CASE("seededMetadata warns on a malformed *date and leaves it unset",
          "[Scanner][Metadata]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("bad-date.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin BadDate\n"
                                    "*date xyzzy\n"
                                    "*data normal from to tape compass clino\n"
                                    "A1 A2 10.0 0 0\n"
                                    "*end BadDate\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK_FALSE(scan.seededMetadata.date.has_value());
    CHECK(anyWarningContains(scan, QStringLiteral("*date")));
}

TEST_CASE("seededMetadata warns on a non-numeric *calibrate declination",
          "[Scanner][Metadata]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("bad-decl.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin BadDecl\n"
                                    "*calibrate declination auto\n"
                                    "*data normal from to tape compass clino\n"
                                    "A1 A2 10.0 0 0\n"
                                    "*end BadDecl\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK_FALSE(scan.seededMetadata.declination.has_value());
    CHECK(anyWarningContains(scan, QStringLiteral("declination")));
}

TEST_CASE("seededMetadata reads the modern *declination command",
          "[Scanner][Metadata]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("declination-cmd.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin DeclCmd\n"
                                    "*declination 7.2 degrees\n"
                                    "*data normal from to tape compass clino\n"
                                    "A1 A2 10.0 0 0\n"
                                    "*end DeclCmd\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.warnings.isEmpty());
    REQUIRE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.declination.value() == 7.2);
    CHECK(scan.seededMetadata.fileOwnsDeclination());
}

TEST_CASE("seededMetadata flags *declination auto as file-owned without a value",
          "[Scanner][Metadata]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("declination-auto.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin DeclAuto\n"
                                    "*declination auto 393000 5789000 100\n"
                                    "*data normal from to tape compass clino\n"
                                    "A1 A2 10.0 0 0\n"
                                    "*end DeclAuto\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.warnings.isEmpty());
    CHECK_FALSE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.declinationIsAuto);
    CHECK(scan.seededMetadata.fileOwnsDeclination());
}

TEST_CASE("seededMetadata warns on a non-numeric Compass DECLINATION field",
          "[Scanner][Metadata]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("bad-decl.dat"));
    writeUtf8File(path,
                  QByteArrayLiteral("BadDeclCave\n"
                                    "SURVEY NAME: BAD\n"
                                    "SURVEY DATE: 6 1 2025\n"
                                    "SURVEY TEAM:\n"
                                    "Alice\n"
                                    "DECLINATION: N/A  FORMAT: DDDDLRUDLADN\n"));

    auto result = cwExternalCenterlineScanner::scanCompass(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK_FALSE(scan.seededMetadata.declination.has_value());
    CHECK_FALSE(scan.seededMetadata.fileOwnsDeclination());
    CHECK(anyWarningContains(scan, QStringLiteral("DECLINATION")));
}

TEST_CASE("seededMetadata warns on a non-numeric Walls DECL= value",
          "[Scanner][Metadata]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Walls' degree:minute form is valid to dewalls/cavern but not
    // parsed here; it must warn rather than silently seeding 7.0.
    const QString path = tempPath(tempDir, QStringLiteral("bad-decl.srv"));
    writeUtf8File(path,
                  QByteArrayLiteral("#UNITS D=Meters A=Degrees DECL=7:30\n"
                                    "W1\tW2\t10.0\t0\t0\n"));

    auto result = cwExternalCenterlineScanner::scanWalls(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK_FALSE(scan.seededMetadata.declination.has_value());
    CHECK(anyWarningContains(scan, QStringLiteral("DECL")));
}

TEST_CASE("seededMetadata comes from the entry file only, not nested includes",
          "[Scanner][Metadata]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString childPath = tempPath(tempDir, QStringLiteral("child.svx"));
    writeUtf8File(childPath,
                  QByteArrayLiteral("*begin Child\n"
                                    "*date 2025-06-01\n"
                                    "*calibrate declination 7.2\n"
                                    "*data normal from to tape compass clino\n"
                                    "C1 C2 10.0 0 0\n"
                                    "*end Child\n"));
    const QString driverPath = tempPath(tempDir, QStringLiteral("driver.svx"));
    writeUtf8File(driverPath,
                  QByteArrayLiteral("*include \"child.svx\"\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(driverPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 2);
    CHECK_FALSE(scan.seededMetadata.date.has_value());
    CHECK_FALSE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.team.isEmpty());
}

TEST_CASE("seededMetadata reads a Compass .dat header",
          "[Scanner][Metadata]")
{
    const QString path = datasetExternalCenterlinePath(
        QStringLiteral("compass_with_metadata.dat"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanCompass(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.warnings.isEmpty());

    REQUIRE(scan.seededMetadata.date.has_value());
    CHECK(scan.seededMetadata.date.value() == QDate(2025, 6, 1));
    // From the DECLINATION: header field, NOT the CORRECTIONS
    // instrument corrections (which are 0.00 in the fixture).
    REQUIRE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.declination.value() == 7.2);
    CHECK(scan.seededMetadata.team
          == QStringList({QStringLiteral("Alice"), QStringLiteral("Bob")}));
}

TEST_CASE("seededMetadata on a Compass .mak entry consults the first .dat",
          "[Scanner][Metadata]")
{
    const QString path = datasetExternalCenterlinePath(
        QStringLiteral("compass_with_metadata.mak"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanCompass(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    REQUIRE(scan.seededMetadata.date.has_value());
    CHECK(scan.seededMetadata.date.value() == QDate(2025, 6, 1));
    REQUIRE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.declination.value() == 7.2);
    CHECK(scan.seededMetadata.team
          == QStringList({QStringLiteral("Alice"), QStringLiteral("Bob")}));
}

TEST_CASE("seededMetadata reads Walls #DATE, #UNITS DECL, and #NOTE team",
          "[Scanner][Metadata]")
{
    const QString path = datasetExternalCenterlinePath(
        QStringLiteral("walls_with_metadata.srv"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanWalls(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.warnings.isEmpty());

    REQUIRE(scan.seededMetadata.date.has_value());
    CHECK(scan.seededMetadata.date.value() == QDate(2025, 6, 1));
    REQUIRE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.declination.value() == 7.2);
    CHECK(scan.seededMetadata.team
          == QStringList({QStringLiteral("Alice"), QStringLiteral("Bob")}));
}

TEST_CASE("seededMetadata parses CRLF-terminated files",
          "[Scanner][Metadata]")
{
    // Real Compass and Walls files are conventionally DOS-formatted;
    // the parsers split on '\n' and must shed the trailing '\r'
    // before date/number parsing.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("crlf.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin Crlf\r\n"
                                    "*date 2025-06-01\r\n"
                                    "*team Alice\r\n"
                                    "*calibrate declination 7.2\r\n"
                                    "*data normal from to tape compass clino\r\n"
                                    "A1 A2 10.0 0 0\r\n"
                                    "*end Crlf\r\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.warnings.isEmpty());
    REQUIRE(scan.seededMetadata.date.has_value());
    CHECK(scan.seededMetadata.date.value() == QDate(2025, 6, 1));
    REQUIRE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.declination.value() == 7.2);
    CHECK(scan.seededMetadata.team == QStringList({QStringLiteral("Alice")}));
}

TEST_CASE("seededMetadata reads only the first survey of a multi-survey Compass .dat",
          "[Scanner][Metadata]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // First survey has a declination but no date; the second (after
    // the form-feed separator) has a date that must NOT leak into
    // the seed.
    const QString path = tempPath(tempDir, QStringLiteral("two-surveys.dat"));
    writeUtf8File(path,
                  QByteArrayLiteral("TwoSurveyCave\n"
                                    "SURVEY NAME: ONE\n"
                                    "SURVEY TEAM:\n"
                                    "Alice\n"
                                    "DECLINATION: 3.50  FORMAT: DDDDLRUDLADN\n"
                                    "\n"
                                    "FROM TO LENGTH BEARING INC\n"
                                    "S1 S2 10.00 0.00 0.00\n"
                                    "\f\n"
                                    "TwoSurveyCave\n"
                                    "SURVEY NAME: TWO\n"
                                    "SURVEY DATE: 6 1 2025\n"
                                    "SURVEY TEAM:\n"
                                    "Bob\n"
                                    "DECLINATION: 9.00  FORMAT: DDDDLRUDLADN\n"));

    auto result = cwExternalCenterlineScanner::scanCompass(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK_FALSE(scan.seededMetadata.date.has_value());
    REQUIRE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.declination.value() == 3.5);
    CHECK(scan.seededMetadata.team == QStringList({QStringLiteral("Alice")}));
}

TEST_CASE("seededMetadata stays empty for a Walls .wpj entry",
          "[Scanner][Metadata]")
{
    // A .wpj seeds nothing - its leaf .srv files each carry their
    // own dates, so a project-level seed would be ambiguous. The
    // referenced MAIN.SRV declares decl=0, which must NOT leak in.
    const QString path = datasetExternalCenterlinePath(QStringLiteral("walls_simple.wpj"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanWalls(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK_FALSE(scan.seededMetadata.date.has_value());
    CHECK_FALSE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.team.isEmpty());
}

TEST_CASE("seededMetadata treats a Walls DECL=0 as file-owned declination",
          "[Scanner][Metadata]")
{
    // MAIN.SRV declares "#units ... decl=0" - zero still counts as
    // set, because the file explicitly owns its declination.
    const QString path = datasetExternalCenterlinePath(QStringLiteral("MAIN.SRV"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanWalls(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    REQUIRE(scan.seededMetadata.declination.has_value());
    CHECK(scan.seededMetadata.declination.value() == 0.0);
}

TEST_CASE("scanSurvex's cycle detection spans format boundaries",
          "[Scanner][CrossFormat]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // svx -> mak -> svx forms a cycle that must be caught by the
    // shared canonical-path visited set even though we hop formats
    // mid-walk.
    const QString svxPath = tempPath(tempDir, QStringLiteral("cycle.svx"));
    writeUtf8File(svxPath,
                  QByteArrayLiteral("*begin Cycle\n*include \"cycle.mak\"\n*end Cycle\n"));
    const QString makPath = tempPath(tempDir, QStringLiteral("cycle.mak"));
    writeUtf8File(makPath,
                  QByteArrayLiteral("/ deliberate cycle back to cycle.svx\n"
                                    "#cycle.svx;\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(svxPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(scan.dependencies.size() == 2);
    CHECK(hasFileNameInDeps(scan, QStringLiteral("cycle.svx")));
    CHECK(hasFileNameInDeps(scan, QStringLiteral("cycle.mak")));
    CHECK(anyWarningContains(scan, QStringLiteral("circular include")));
}
