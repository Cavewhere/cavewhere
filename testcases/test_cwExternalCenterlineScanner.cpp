/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwExternalCenterlineScanner.h"
#include "cwExternalStationHarvest.h"
#include "cwStation.h"

// Test helpers
#include "LoadProjectHelper.h"

// Qt
#include <QByteArray>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMetaEnum>
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

TEST_CASE("formatName maps formats to display names", "[Scanner]")
{
    using namespace cwExternalCenterlineScanner;
    CHECK(formatName(Format::Survex) == QStringLiteral("Survex"));
    CHECK(formatName(Format::Compass) == QStringLiteral("Compass"));
    CHECK(formatName(Format::Walls) == QStringLiteral("Walls"));
    CHECK(formatName(Format::Unknown).isEmpty());
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
    // *calibrate declination 7.2 corrects bearings by -7.2: the seed stores
    // the declination as cavern applies it.
    CHECK(scan.seededMetadata.declination.value() == -7.2);
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
    CHECK(scan.seededMetadata.declination.value() == -7.2);
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

TEST_CASE("scanSurvex fails on an absolute *include path", "[Scanner][Survex]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // The child exists on disk, so this proves the absolute path is
    // refused before resolution rather than riding the missing-target
    // warning path.
    const QString childPath = tempPath(tempDir, QStringLiteral("abs_child.svx"));
    writeUtf8File(childPath,
                  QByteArrayLiteral("*begin AbsChild\n*end AbsChild\n"));

    const QString driverPath = tempPath(tempDir, QStringLiteral("abs_driver.svx"));
    const QString includeLine =
        QStringLiteral("*include \"%1\"\n").arg(childPath);
    writeUtf8File(driverPath, includeLine.toUtf8());

    auto result = cwExternalCenterlineScanner::scanSurvex(driverPath);
    REQUIRE(result.hasError());
    const QString error = result.errorMessage();
    CHECK(error.contains(QStringLiteral("absolute include path")));
    CHECK(error.contains(QFileInfo(driverPath).canonicalFilePath()));
    CHECK(error.contains(childPath));
}

TEST_CASE("scanCompass fails on an absolute .mak reference", "[Scanner][Compass]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString datPath = tempPath(tempDir, QStringLiteral("abs_cave.dat"));
    writeUtf8File(datPath, QByteArrayLiteral("ABS CAVE\n"));

    const QString makPath = tempPath(tempDir, QStringLiteral("abs_project.mak"));
    const QString referenceLine = QStringLiteral("#%1,A;\n").arg(datPath);
    writeUtf8File(makPath, referenceLine.toUtf8());

    auto result = cwExternalCenterlineScanner::scanCompass(makPath);
    REQUIRE(result.hasError());
    const QString error = result.errorMessage();
    CHECK(error.contains(QStringLiteral("absolute include path")));
    CHECK(error.contains(QFileInfo(makPath).canonicalFilePath()));
    CHECK(error.contains(datPath));
}

TEST_CASE("scanWalls fails on an absolute reference", "[Scanner][Walls]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString srvPath = tempPath(tempDir, QStringLiteral("MAIN.SRV"));
    writeUtf8File(srvPath, QByteArrayLiteral("#UNITS DECL=0\n"));

    SECTION("absolute .NAME on a survey entry") {
        const QString wpjPath = tempPath(tempDir, QStringLiteral("abs_name.wpj"));
        const QString absoluteName =
            QDir(tempDir.path()).absoluteFilePath(QStringLiteral("MAIN"));
        const QString contents =
            QStringLiteral(";WALLS Project file\n"
                           ".BOOK\tTest Cave\n"
                           ".NAME\tTEST-CAVE\n"
                           ".STATUS\t8\n"
                           ".SURVEY\tMain Passage\n"
                           ".NAME\t%1\n"
                           ".STATUS\t8\n"
                           ".ENDBOOK\n")
                .arg(absoluteName);
        writeUtf8File(wpjPath, contents.toUtf8());

        auto result = cwExternalCenterlineScanner::scanWalls(wpjPath);
        REQUIRE(result.hasError());
        CHECK(result.errorMessage().contains(QStringLiteral("absolute include path")));
        CHECK(result.errorMessage().contains(absoluteName));
    }

    SECTION("absolute .PATH on a nested book") {
        const QString wpjPath = tempPath(tempDir, QStringLiteral("abs_path.wpj"));
        const QString absoluteDir = QDir(tempDir.path()).absolutePath();
        const QString contents =
            QStringLiteral(";WALLS Project file\n"
                           ".BOOK\tTest Cave\n"
                           ".NAME\tTEST-CAVE\n"
                           ".STATUS\t8\n"
                           ".BOOK\tSub Book\n"
                           ".NAME\tSUB\n"
                           ".PATH\t%1\n"
                           ".STATUS\t8\n"
                           ".SURVEY\tMain Passage\n"
                           ".NAME\tMAIN\n"
                           ".STATUS\t8\n"
                           ".ENDBOOK\n"
                           ".ENDBOOK\n")
                .arg(absoluteDir);
        writeUtf8File(wpjPath, contents.toUtf8());

        auto result = cwExternalCenterlineScanner::scanWalls(wpjPath);
        REQUIRE(result.hasError());
        CHECK(result.errorMessage().contains(QStringLiteral("absolute include path")));
        CHECK(result.errorMessage().contains(absoluteDir));
    }
}

TEST_CASE("entryDirectIncludes lists only the entry file's first-level includes",
          "[Scanner][Survex]")
{
    const QString nestedPath =
        datasetExternalCenterlinePath(QStringLiteral("survex_nested.svx"));
    REQUIRE(QFileInfo::exists(nestedPath));

    auto nested = cwExternalCenterlineScanner::scanSurvex(nestedPath);
    REQUIRE_FALSE(nested.hasError());
    const ScanResult nestedScan = nested.value();
    // entrance.svx is included by the entry; passage.svx is one level
    // deeper and stays out.
    REQUIRE(nestedScan.entryDirectIncludes.size() == 1);
    CHECK(QFileInfo(nestedScan.entryDirectIncludes.first()).fileName()
          == QStringLiteral("entrance.svx"));

    const QString barePath =
        datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    REQUIRE(QFileInfo::exists(barePath));

    auto bare = cwExternalCenterlineScanner::scanSurvex(barePath);
    REQUIRE_FALSE(bare.hasError());
    CHECK(bare.value().entryDirectIncludes.isEmpty());
}

TEST_CASE("entryDirectIncludes leaves out a file that includes itself",
          "[Scanner][Survex]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString selfPath = tempPath(tempDir, QStringLiteral("self.svx"));
    writeUtf8File(selfPath,
                  QByteArrayLiteral("*begin Self\n*include self.svx\n*end Self\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(selfPath);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(anyWarningContains(scan, QStringLiteral("circular include")));
    CHECK(scan.entryDirectIncludes.isEmpty());
}

TEST_CASE("blocks map a Survex *begin tree with per-block station counts",
          "[Scanner][Blocks]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("survex_blocks.svx"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    // Block extraction must leave the walk's own outcome alone.
    CHECK(scan.dependencies.size() == 1);
    CHECK(scan.warnings.isEmpty());

    REQUIRE(scan.blocks.size() == 4);
    // Document order: a parent always precedes its children.
    CHECK(scan.blocks.at(0).path == QStringLiteral("doghill"));
    CHECK(scan.blocks.at(0).name() == QStringLiteral("doghill"));
    CHECK(scan.blocks.at(0).depth == 0);
    CHECK(scan.blocks.at(0).stationCount == 3);

    CHECK(scan.blocks.at(1).path == QStringLiteral("doghill.big-passage"));
    CHECK(scan.blocks.at(1).name() == QStringLiteral("big-passage"));
    CHECK(scan.blocks.at(1).depth == 1);
    CHECK(scan.blocks.at(1).stationCount == 4);

    CHECK(scan.blocks.at(2).path == QStringLiteral("doghill.big-passage.east"));
    CHECK(scan.blocks.at(2).name() == QStringLiteral("east"));
    CHECK(scan.blocks.at(2).depth == 2);
    CHECK(scan.blocks.at(2).stationCount == 5);

    // A block with no shots of its own is still reported, at 0.
    CHECK(scan.blocks.at(3).path == QStringLiteral("doghill.big-passage.sump"));
    CHECK(scan.blocks.at(3).depth == 2);
    CHECK(scan.blocks.at(3).stationCount == 0);

    // Each block carries only the date it writes itself: doghill and east
    // write none, big-passage writes a range whose start is the date, and
    // sump's date is visible even though it holds no stations.
    CHECK_FALSE(scan.blocks.at(0).date.isValid());
    CHECK(scan.blocks.at(1).date == QDate(2024, 1, 5));
    CHECK_FALSE(scan.blocks.at(2).date.isValid());
    CHECK(scan.blocks.at(3).date == QDate(2025, 3, 15));

    CHECK(scan.entryHasOwnShots);
}

TEST_CASE("a block keeps the first *date it writes", "[Scanner][Blocks]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("dates.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin Outer\n"
                                    "*date 2024-06-01\n"
                                    "*date 2024-07-01\n"
                                    "a1 a2 1.0 0 0\n"
                                    "*end Outer\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    REQUIRE(scan.blocks.size() == 1);
    CHECK(scan.blocks.first().date == QDate(2024, 6, 1));
}

TEST_CASE("an unparseable *date leaves the block dateless and silent",
          "[Scanner][Blocks]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // The block walk says nothing about a bad date - cavern validates
    // date syntax itself. The bad date lives in an included file, which
    // the entry-file-only metadata pass never reads, so a warning here
    // could only have come from the block walk.
    const QString childPath = tempPath(tempDir, QStringLiteral("baddate-child.svx"));
    writeUtf8File(childPath,
                  QByteArrayLiteral("*begin Outer\n"
                                    "*date yesterday\n"
                                    "a1 a2 1.0 0 0\n"
                                    "*end Outer\n"));
    const QString path = tempPath(tempDir, QStringLiteral("baddate.svx"));
    writeUtf8File(path, QByteArrayLiteral("*include baddate-child.svx\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    REQUIRE(scan.blocks.size() == 1);
    CHECK_FALSE(scan.blocks.first().date.isValid());
    CHECK_FALSE(anyWarningContains(scan, QStringLiteral("*date")));
}

TEST_CASE("rootDate takes the first Compass survey's SURVEY DATE",
          "[Scanner][Blocks]")
{
    // Every Compass station is a root station, so the date the root
    // window seeds from is the first survey's - B's 2025-02-02 is
    // read past, not kept.
    const QString path = datasetExternalCenterlinePath(QStringLiteral("compass_multi.mak"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanCompass(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    CHECK(scan.blocks.isEmpty());
    CHECK(scan.rootDate == QDate(2025, 1, 1));
}

TEST_CASE("a Walls prefix block carries the #DATE written under it",
          "[Scanner][Blocks]")
{
    {
        const QString path = datasetExternalCenterlinePath(
            QStringLiteral("walls_prefixed/walls_prefixed.wpj"));
        REQUIRE(QFileInfo::exists(path));

        auto result = cwExternalCenterlineScanner::scanWalls(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        // ROOT.SRV writes its #DATE with no prefix in force, so it
        // dates the root; PREFIXED.SRV writes its own under XY.
        CHECK(scan.rootDate == QDate(2025, 1, 1));
        REQUIRE(scan.blocks.size() == 1);
        CHECK(scan.blocks.first().path == QStringLiteral("XY"));
        CHECK(scan.blocks.first().date == QDate(2025, 1, 2));
    }
    {
        // The book's .OPTIONS prefix puts BK in force before the
        // .srv's first line, so its #DATE dates the block and the
        // root stays dateless.
        const QString path = datasetExternalCenterlinePath(
            QStringLiteral("walls_book_prefix/walls_book_prefix.wpj"));
        REQUIRE(QFileInfo::exists(path));

        auto result = cwExternalCenterlineScanner::scanWalls(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        CHECK_FALSE(scan.rootDate.isValid());
        REQUIRE(scan.blocks.size() == 1);
        CHECK(scan.blocks.first().path == QStringLiteral("BK"));
        CHECK(scan.blocks.first().date == QDate(2025, 1, 3));
    }
}

TEST_CASE("blocks nest across an *include boundary", "[Scanner][Blocks]")
{
    const QString path = datasetExternalCenterlinePath(QStringLiteral("survex_nested.svx"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    // Entrance lives in entrance.svx and Passage in passage.svx, yet
    // both nest under the Nested block opened by the entry file.
    REQUIRE(scan.blocks.size() == 3);
    CHECK(scan.blocks.at(0).path == QStringLiteral("Nested"));
    CHECK(scan.blocks.at(0).depth == 0);
    CHECK(scan.blocks.at(0).stationCount == 0);
    CHECK(scan.blocks.at(1).path == QStringLiteral("Nested.Entrance"));
    CHECK(scan.blocks.at(1).depth == 1);
    CHECK(scan.blocks.at(1).stationCount == 3);  // E1, E2, Passage.P1
    CHECK(scan.blocks.at(2).path == QStringLiteral("Nested.Entrance.Passage"));
    CHECK(scan.blocks.at(2).depth == 2);
    CHECK(scan.blocks.at(2).stationCount == 3);  // P1, P2, P3

    // The entry writes only *begin / *include / *end.
    CHECK_FALSE(scan.entryHasOwnShots);
}

TEST_CASE("a Survex master of *include lines has no shots of its own",
          "[Scanner][Blocks]")
{
    const QString path =
        datasetExternalCenterlinePath(QStringLiteral("survex_master/master.svx"));
    REQUIRE(QFileInfo::exists(path));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    CHECK_FALSE(scan.entryHasOwnShots);
    CHECK(scan.entryDirectIncludes.size() == 3);

    REQUIRE(scan.blocks.size() == 3);
    for (const cwScanBlock& block : scan.blocks) {
        CHECK(block.depth == 0);
    }
    CHECK(scan.blocks.at(0).path == QStringLiteral("alpha"));
    CHECK(scan.blocks.at(0).stationCount == 2);
    CHECK(scan.blocks.at(1).path == QStringLiteral("bravo"));
    CHECK(scan.blocks.at(1).stationCount == 3);
    CHECK(scan.blocks.at(2).path == QStringLiteral("charlie"));
    CHECK(scan.blocks.at(2).stationCount == 4);
}

TEST_CASE("Compass surveys make no blocks; rootStationCount counts every station",
          "[Scanner][Blocks]")
{
    // cavern discards "SURVEY NAME:" and reads each .dat with no
    // surrounding level, so a Compass survey is no naming level:
    // every station of every survey lands in the file root.
    {
        const QString path =
            datasetExternalCenterlinePath(QStringLiteral("compass_multi.mak"));
        auto result = cwExternalCenterlineScanner::scanCompass(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        CHECK(scan.blocks.isEmpty());
        // A1, A2, A3 from survey A and B1, B2 from survey B, which
        // ties to A2 - the shared station counts once.
        CHECK(scan.rootStationCount == 5);
        // The .mak holds no shots itself.
        CHECK_FALSE(scan.entryHasOwnShots);
    }
    {
        const QString path =
            datasetExternalCenterlinePath(QStringLiteral("compass_simple.dat"));
        auto result = cwExternalCenterlineScanner::scanCompass(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        CHECK(scan.blocks.isEmpty());
        CHECK(scan.rootStationCount == 3);
        CHECK(scan.entryHasOwnShots);
    }
}

TEST_CASE("Walls blocks name the prefix levels, and nothing else",
          "[Scanner][Blocks]")
{
    {
        // A .BOOK / .SURVEY title is metadata to cavern, so a
        // prefix-less project makes no block at all and every
        // station is a root station.
        const QString path =
            datasetExternalCenterlinePath(QStringLiteral("walls_simple.wpj"));
        auto result = cwExternalCenterlineScanner::scanWalls(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        CHECK(scan.blocks.isEmpty());
        CHECK(scan.rootStationCount == 4);  // A1 .. A4
        CHECK_FALSE(scan.entryHasOwnShots);
    }
    {
        const QString path = datasetExternalCenterlinePath(QStringLiteral("MAIN.SRV"));
        auto result = cwExternalCenterlineScanner::scanWalls(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        CHECK(scan.blocks.isEmpty());
        CHECK(scan.rootStationCount == 4);
        CHECK(scan.entryHasOwnShots);
    }
    {
        // #PREFIX XY is a level: its two stations belong to the XY
        // block, the prefix-less survey's three to the root, and the
        // qualified token XY:P1 overlays XY on the empty prefix in
        // force, so it lands in the same XY block.
        const QString path = datasetExternalCenterlinePath(
            QStringLiteral("walls_prefixed/walls_prefixed.wpj"));
        auto result = cwExternalCenterlineScanner::scanWalls(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        REQUIRE(scan.blocks.size() == 1);
        CHECK(scan.blocks.first().path == QStringLiteral("XY"));
        CHECK(scan.blocks.first().name() == QStringLiteral("XY"));
        CHECK(scan.blocks.first().depth == 0);
        CHECK(scan.blocks.first().stationCount == 2);  // P1, P2
        CHECK(scan.rootStationCount == 3);             // A1, A2, A3
        CHECK_FALSE(scan.entryHasOwnShots);
    }
    {
        // The prefix comes from the book's .OPTIONS line, so the
        // whole .srv sits under BK and the root stays empty.
        const QString path = datasetExternalCenterlinePath(
            QStringLiteral("walls_book_prefix/walls_book_prefix.wpj"));
        auto result = cwExternalCenterlineScanner::scanWalls(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        REQUIRE(scan.blocks.size() == 1);
        CHECK(scan.blocks.first().path == QStringLiteral("BK"));
        CHECK(scan.blocks.first().stationCount == 2);  // O1, O2
        CHECK(scan.rootStationCount == 0);
    }
}

TEST_CASE("Walls prefix slots nest outermost first and clear on an empty value",
          "[Scanner][Blocks]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // #PREFIX is the innermost slot, #PREFIX3 the outermost, so the
    // path reads slot 3 first. A prefix= token on a #UNITS line sets
    // the same slots, and an empty value clears one.
    const QString path = tempPath(tempDir, QStringLiteral("slots.srv"));
    writeUtf8File(path,
                  QByteArrayLiteral("#PREFIX3 top\n"
                                    "#PREFIX2 mid\n"
                                    "#PREFIX deep\n"
                                    "d1 d2 10.0 0 0\n"
                                    "#UNITS prefix=other\n"
                                    "o1 o2 10.0 0 0\n"
                                    "#PREFIX\n"
                                    "m1 m2 10.0 0 0\n"
                                    "#PREFIX2\n"
                                    "#PREFIX3\n"
                                    "r1 r2 10.0 0 0\n"));

    auto result = cwExternalCenterlineScanner::scanWalls(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    // Ancestors are materialized before the paths that need them,
    // so parents precede children even though no station sits
    // directly in top or top.mid.
    REQUIRE(scan.blocks.size() == 4);
    CHECK(scan.blocks.at(0).path == QStringLiteral("top"));
    CHECK(scan.blocks.at(0).depth == 0);
    CHECK(scan.blocks.at(0).stationCount == 0);
    CHECK(scan.blocks.at(1).path == QStringLiteral("top.mid"));
    CHECK(scan.blocks.at(1).depth == 1);
    CHECK(scan.blocks.at(1).stationCount == 2);  // m1, m2
    CHECK(scan.blocks.at(2).path == QStringLiteral("top.mid.deep"));
    CHECK(scan.blocks.at(2).depth == 2);
    CHECK(scan.blocks.at(2).stationCount == 2);  // d1, d2
    CHECK(scan.blocks.at(3).path == QStringLiteral("top.mid.other"));
    CHECK(scan.blocks.at(3).depth == 2);
    CHECK(scan.blocks.at(3).stationCount == 2);  // o1, o2
    CHECK(scan.rootStationCount == 2);           // r1, r2
}

TEST_CASE("A qualified Walls token overlays the innermost prefix levels",
          "[Scanner][Blocks]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Measured with the cavern CLI: under "#PREFIX3 top / #PREFIX2 mid
    // / #PREFIX deep", "XY:P1" solves to cave0:top:mid:XY:P1, ":P9" to
    // cave0:top:mid:P9 and "Q:R:S1" to cave0:top:Q:R:S1 - the explicit
    // segments replace the innermost levels and the outer levels stay
    // in force.
    const QString path = tempPath(tempDir, QStringLiteral("overlay.srv"));
    writeUtf8File(path,
                  QByteArrayLiteral("#PREFIX3 top\n"
                                    "#PREFIX2 mid\n"
                                    "#PREFIX deep\n"
                                    "d1 d2 10.0 0 0\n"
                                    "d2 XY:P1 10.0 0 0\n"
                                    "XY:P1 :P9 10.0 0 0\n"
                                    ":P9 Q:R:S1 10.0 0 0\n"));

    auto result = cwExternalCenterlineScanner::scanWalls(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    QHash<QString, int> countByPath;
    for (const cwScanBlock& block : scan.blocks) {
        countByPath.insert(block.path, block.stationCount);
    }

    CHECK(countByPath.value(QStringLiteral("top.mid.deep")) == 2);  // d1, d2
    CHECK(countByPath.value(QStringLiteral("top.mid.XY")) == 1);    // P1
    CHECK(countByPath.value(QStringLiteral("top.mid")) == 1);       // P9
    CHECK(countByPath.value(QStringLiteral("top.Q.R")) == 1);       // S1
    CHECK(scan.rootStationCount == 0);
}

TEST_CASE("rootStationCount counts the stations outside every Survex block",
          "[Scanner][Blocks]")
{
    {
        const QString path =
            datasetExternalCenterlinePath(QStringLiteral("survex_root_and_block.svx"));
        REQUIRE(QFileInfo::exists(path));

        auto result = cwExternalCenterlineScanner::scanSurvex(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        REQUIRE(scan.blocks.size() == 1);
        CHECK(scan.blocks.first().path == QStringLiteral("side"));
        CHECK(scan.blocks.first().stationCount == 2);  // s1, s2
        // r1, r2, r3 - the tie shot's "side.s1" names a station
        // inside the block, so the root leaves it out.
        CHECK(scan.rootStationCount == 3);
    }
    {
        // No *begin at all: the whole file is root.
        const QString path =
            datasetExternalCenterlinePath(QStringLiteral("survex_bare.svx"));
        auto result = cwExternalCenterlineScanner::scanSurvex(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        CHECK(scan.blocks.isEmpty());
        CHECK(scan.rootStationCount == 2);  // a1, a2
    }
    {
        // Regression: every station of the nested-block fixture sits
        // inside a block, so its blocks and a root count of 0 are
        // exactly what they were before the root set existed.
        const QString path =
            datasetExternalCenterlinePath(QStringLiteral("survex_blocks.svx"));
        auto result = cwExternalCenterlineScanner::scanSurvex(path);
        REQUIRE_FALSE(result.hasError());
        const ScanResult scan = result.value();

        REQUIRE(scan.blocks.size() == 4);
        CHECK(scan.blocks.at(0).stationCount == 3);
        CHECK(scan.blocks.at(1).stationCount == 4);
        CHECK(scan.blocks.at(2).stationCount == 5);
        CHECK(scan.blocks.at(3).stationCount == 0);
        CHECK(scan.rootStationCount == 0);
        CHECK_FALSE(scan.rootDate.isValid());
    }
}

namespace Catch {

//! Prints a fix's station, coordinate and systems, so a failed comparison
//! shows which part differs.
template<>
struct StringMaker<cwExternalCenterlineScanner::ScannedFix>
{
    static std::string convert(const cwExternalCenterlineScanner::ScannedFix& fix)
    {
        return QStringLiteral("{%1 \"%2\" \"%3\" \"%4\"}")
            .arg(fix.station, fix.coordinate, fix.coordinateSystem, fix.placementSystem)
            .toStdString();
    }
};

} // namespace Catch

namespace {

using cwExternalCenterlineScanner::ScannedFix;

//! \a fixes with where each was read cleared, so a case compares what a fix
//! says rather than which line or node holds it.
QList<ScannedFix> withoutLocation(QList<ScannedFix> fixes)
{
    for (ScannedFix& fix : fixes) {
        fix.file.clear();
        fix.line = 0;
        fix.nodePath.clear();
    }
    return fixes;
}

QList<ScannedFix> scannedFixes(const QString& entryFile)
{
    auto result = cwExternalCenterlineScanner::scan(entryFile);
    INFO("scan: " << result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    return withoutLocation(result.value().fixes);
}

//! A fix as a case compares it, with the system cavern places it in beside the
//! name its row shows.
ScannedFix placedFix(const QString& station, const QString& coordinate,
                     const QString& coordinateSystem, const QString& placementSystem)
{
    return {.station = station,
            .coordinate = coordinate,
            .coordinateSystem = coordinateSystem,
            .placementSystem = placementSystem};
}

ScannedFix fixNamed(const QList<ScannedFix>& fixes, const QString& station)
{
    for (const ScannedFix& fix : fixes) {
        if (fix.station == station) {
            return fix;
        }
    }
    FAIL("No scanned fix names " << station.toStdString());
    return ScannedFix();
}

constexpr double kMinutesPerDegree = 60.0;
constexpr double kSecondsPerDegree = 3600.0;
// A lat/long fix's text keeps its degrees to a billionth (about 0.1 mm).
constexpr double kDegreeTextMargin = 1e-9;

double degrees(double wholeDegrees, double minutes, double seconds)
{
    return wholeDegrees + minutes / kMinutesPerDegree + seconds / kSecondsPerDegree;
}

//! Checks that \a text reads "longitude latitude elevation" in decimal degrees
//! and meters, as cavern reads a Walls lat/long #FIX.
void checkDegreeText(const QString& text, double longitude, double latitude, double elevation)
{
    INFO("coordinate text: " << text.toStdString());
    const QStringList tokens = text.split(QLatin1Char(' '));
    REQUIRE(tokens.size() == 3);
    bool longitudeOk = false;
    bool latitudeOk = false;
    bool elevationOk = false;
    CHECK(tokens.at(0).toDouble(&longitudeOk) == Catch::Approx(longitude).margin(kDegreeTextMargin));
    CHECK(tokens.at(1).toDouble(&latitudeOk) == Catch::Approx(latitude).margin(kDegreeTextMargin));
    CHECK(tokens.at(2).toDouble(&elevationOk) == Catch::Approx(elevation));
    CHECK(longitudeOk);
    CHECK(latitudeOk);
    CHECK(elevationOk);
}

} // namespace

TEST_CASE("scanSurvex records where each *fix is written and the closure's *cs out",
          "[Scanner][Attach]")
{
    constexpr int kEntsFixLine = 3;
    constexpr int kHoupingFixLine = 4;
    const auto scanOf = [](const QString& entry) {
        auto result = cwExternalCenterlineScanner::scan(datasetExternalCenterlinePath(
            QStringLiteral("survex_bare_fix_tree/north/%1").arg(entry)));
        INFO("scan: " << result.errorMessage().toStdString());
        REQUIRE_FALSE(result.hasError());
        return result.value();
    };

    const auto plain = scanOf(QStringLiteral("north.svx"));
    CHECK(plain.outputCoordinateSystem.isEmpty());
    REQUIRE(plain.fixes.size() == 1);
    const ScannedFix& bare = plain.fixes.first();
    CHECK(bare.station == QStringLiteral("sanqiao.e1"));
    CHECK_FALSE(bare.hasCoordinateSystem());
    CHECK(bare.file.endsWith(QStringLiteral("/sanqiao/fixedpts/ents.svx")));
    CHECK(bare.line == kEntsFixLine);

    const auto withOut = scanOf(QStringLiteral("north_cs_out.svx"));
    CHECK(withOut.outputCoordinateSystem == QStringLiteral("UTM48N"));
    REQUIRE(withOut.fixes.size() == 2);
    CHECK(withOut.fixes.at(0).station == QStringLiteral("houping.h1"));
    CHECK(withOut.fixes.at(0).coordinateSystem == QStringLiteral("UTM48N"));
    CHECK(withOut.fixes.at(0).line == kHoupingFixLine);
    CHECK(withOut.fixes.at(1).station == QStringLiteral("sanqiao.e1"));
    CHECK_FALSE(withOut.fixes.at(1).hasCoordinateSystem());
}

TEST_CASE("scanSurvex records each *fix's coordinate and the input *cs in force", "[Scanner][Attach]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    SECTION("a bare *fix has none")
    {
        const QList<ScannedFix> fixes =
            scannedFixes(datasetExternalCenterlinePath(QStringLiteral("survex_blocks.svx")));
        CHECK(fixes == QList<ScannedFix>{{QStringLiteral("doghill.d1"), QStringLiteral("0 0 0"), QString()}});
    }

    SECTION("*cs scopes to its *begin block, and *cs out names no input system")
    {
        const QString path = writeUtf8File(tempPath(tempDir, QStringLiteral("scoped.svx")),
                                           QByteArrayLiteral("*cs out EPSG:32616\n"
                                                             "*fix root 0 0 0\n"
                                                             "*begin inner\n"
                                                             "*cs EPSG:32616 ; input\n"
                                                             "*fix a 1 2 3\n"
                                                             "*begin deeper\n"
                                                             "*FIX b 1 2 3\n"
                                                             "*end deeper\n"
                                                             "*end inner\n"
                                                             "*fix after 0 0 0\n"));
        const QString utm16 = QStringLiteral("EPSG:32616");
        CHECK(scannedFixes(path) == QList<ScannedFix>{{QStringLiteral("root"), QStringLiteral("0 0 0"), QString()},
                                                      {QStringLiteral("inner.a"), QStringLiteral("1 2 3"), utm16},
                                                      {QStringLiteral("inner.deeper.b"), QStringLiteral("1 2 3"), utm16},
                                                      {QStringLiteral("after"), QStringLiteral("0 0 0"), QString()}});
    }

    SECTION("a file with no *fix records none")
    {
        CHECK(scannedFixes(datasetExternalCenterlinePath(QStringLiteral("passage.svx"))).isEmpty());
    }
}

TEST_CASE("scanCompass records .mak fixes and the datum and zone in force",
          "[Scanner][Attach]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    writeUtf8File(tempPath(tempDir, QStringLiteral("cave.dat")), QByteArrayLiteral("anything\n"));

    SECTION("a datum and a zone give the fixes after them a system")
    {
        const QString makPath = writeUtf8File(
            tempPath(tempDir, QStringLiteral("georef.mak")),
            QByteArrayLiteral("#cave.dat,A1[m,0,0,0],A2;\n"
                              "&North American 1983;\n"
                              "$16;\n"
                              "#cave.dat,B1[M,580661.57,4113846.34,219];\n"));
        CHECK(scannedFixes(makPath)
              == QList<ScannedFix>{{QStringLiteral("cave.A1"), QStringLiteral("0 0 0"), QString()},
                                   placedFix(QStringLiteral("cave.B1"), QStringLiteral("580661.57 4113846.34 219"),
                                             QStringLiteral("North American 1983, UTM zone 16N"),
                                             QStringLiteral("EPSG:26916"))});
    }

    SECTION("a base location's zone stands in for a missing zone line")
    {
        const QString makPath = writeUtf8File(
            tempPath(tempDir, QStringLiteral("base.mak")),
            QByteArrayLiteral("@580661.57,4113846.34,219,16,0.549;\n"
                              "&North American 1983;\n"
                              "#cave.dat,A1[m,580661.57,4113846.34,219];\n"));
        CHECK(scannedFixes(makPath)
              == QList<ScannedFix>{placedFix(QStringLiteral("cave.A1"), QStringLiteral("580661.57 4113846.34 219"),
                                             QStringLiteral("North American 1983, UTM zone 16N"),
                                             QStringLiteral("EPSG:26916"))});
    }

    SECTION("a fix is named under its DAT's survey, as cavern reads the line")
    {
        const QString makPath = writeUtf8File(
            tempPath(tempDir, QStringLiteral("nested.mak")),
            QByteArrayLiteral("#sub/Upper.Cave.DAT,A1[m,0,0,0];\n"
                              "#.DAT,B1[m,0,0,0];\n"));
        CHECK(scannedFixes(makPath)
              == QList<ScannedFix>{{QStringLiteral("upper.cave.A1"), QStringLiteral("0 0 0"), QString()},
                                   {QStringLiteral("dat.B1"), QStringLiteral("0 0 0"), QString()}});
    }

    SECTION("a lone .dat fixes nothing")
    {
        CHECK(scannedFixes(tempPath(tempDir, QStringLiteral("cave.dat"))).isEmpty());
    }
}

TEST_CASE("A .mak that names a UTM zone but no datum reads in Compass's default",
          "[Scanner][Attach]")
{
    using cwExternalCenterlineScanner::DefaultedDatum;

    const QString fixtureName = QStringLiteral("compass_zone_no_datum");
    const QString nad27Zone13 = QStringLiteral("North American 1927, UTM zone 13N");

    SECTION("the fixture's fix reads in North American 1927 at its zone")
    {
        const auto scan = cwExternalCenterlineScanner::scan(
            datasetExternalCenterlinePath(fixtureName + QStringLiteral(".mak")));
        REQUIRE_FALSE(scan.hasError());
        CHECK(withoutLocation(scan.value().fixes)
              == QList<ScannedFix>{placedFix(QStringLiteral("compass_zone_no_datum.A1"),
                                             QStringLiteral("478000.0092 4429999.98936 1655.00304"), nad27Zone13,
                                             QStringLiteral("EPSG:26713"))});
        CHECK(scan.value().defaultedDatums
              == QList<DefaultedDatum>{{fixtureName + QStringLiteral(".mak"), QStringLiteral("13N")}});
    }

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    writeUtf8File(tempPath(tempDir, QStringLiteral("a.dat")), QByteArrayLiteral("anything\n"));

    SECTION("a base location alone names the zone")
    {
        const QString makPath = writeUtf8File(
            tempPath(tempDir, QStringLiteral("base.mak")),
            QByteArrayLiteral("#a.dat;\n@1,2,3,16,0;\n#a.dat,B1[m,0,0,0];\n"));
        const auto scan = cwExternalCenterlineScanner::scan(makPath);
        REQUIRE_FALSE(scan.hasError());
        CHECK(withoutLocation(scan.value().fixes)
              == QList<ScannedFix>{placedFix(QStringLiteral("a.B1"), QStringLiteral("0 0 0"),
                                             QStringLiteral("North American 1927, UTM zone 16N"),
                                             QStringLiteral("EPSG:26716"))});
        CHECK(scan.value().defaultedDatums
              == QList<DefaultedDatum>{{QStringLiteral("base.mak"), QStringLiteral("16N")}});
    }

    SECTION("a .mak that writes a datum anywhere, or no zone, asks for no default")
    {
        const QString datumLater = writeUtf8File(
            tempPath(tempDir, QStringLiteral("later.mak")),
            QByteArrayLiteral("$16;\n#a.dat,A1[m,1,2,3];\n&North American 1983;\n"));
        const auto laterScan = cwExternalCenterlineScanner::scan(datumLater);
        REQUIRE_FALSE(laterScan.hasError());
        CHECK(withoutLocation(laterScan.value().fixes)
              == QList<ScannedFix>{{QStringLiteral("a.A1"), QStringLiteral("1 2 3"), QString()}});
        CHECK(laterScan.value().defaultedDatums.isEmpty());

        const QString noZone = writeUtf8File(tempPath(tempDir, QStringLiteral("nozone.mak")),
                                             QByteArrayLiteral("/ note\n#a.dat,A1;\n"));
        const auto noZoneScan = cwExternalCenterlineScanner::scan(noZone);
        REQUIRE_FALSE(noZoneScan.hasError());
        CHECK(noZoneScan.value().defaultedDatums.isEmpty());
    }
}

TEST_CASE("scanCompass reads a .mak '#' line that spans lines", "[Scanner][Attach]")
{
    // Compass's Project Manager writes each link and fixed station of a '#'
    // line on a line of its own, and cavern reads the statement up to its ';'.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    writeUtf8File(tempPath(tempDir, QStringLiteral("sub/cave.dat")), QByteArrayLiteral("anything\n"));
    const QString makPath = writeUtf8File(
        tempPath(tempDir, QStringLiteral("spanning.mak")),
        QByteArrayLiteral("&North American 1983; / inline / $16;\r\n"
                          "[sub;\r\n"
                          "#cave.dat,\r\n"
                          "  A1[M,580661.57,4113846.34,219],\r\n"
                          "  A2,\r\n"
                          "  A3[F,1904992.0,13497526.0,718.5];\r\n"
                          "];\r\n"));

    const auto scan = cwExternalCenterlineScanner::scan(makPath);
    REQUIRE_FALSE(scan.hasError());
    const QString system = QStringLiteral("North American 1983, UTM zone 16N");
    const QString placement = QStringLiteral("EPSG:26916");
    CHECK(withoutLocation(scan.value().fixes)
          == QList<ScannedFix>{placedFix(QStringLiteral("cave.A1"), QStringLiteral("580661.57 4113846.34 219"),
                                         system, placement),
                               placedFix(QStringLiteral("cave.A3"), QStringLiteral("580641.5616 4114045.9248 218.9988"),
                                         system, placement)});
    CHECK(scan.value().dependencies.size() == 2);
    CHECK(scan.value().warnings.isEmpty());
}

TEST_CASE("scanWalls records #FIX lines as cavern reads them and the system their .wpj's .REF names",
          "[Scanner][Attach]")
{
    // GEOREF.SRV fixes A1 in grid meters and A3 as W86:5:29.254 N37:10:3.195.
    const QString a1 = QStringLiteral("580661.57 4113846.34 219");
    const double a3Longitude = -degrees(86.0, 5.0, 29.254);
    const double a3Latitude = degrees(37.0, 10.0, 3.195);
    constexpr double a3Elevation = 219.0;

    SECTION("under the .wpj's NAD83 .REF")
    {
        const QList<ScannedFix> georeferenced =
            scannedFixes(testcasesDatasetSourcePath(QStringLiteral("walls/georef_cave.wpj")));
        REQUIRE(georeferenced.size() == 2);
        CHECK(fixNamed(georeferenced, QStringLiteral("A1"))
              == placedFix(QStringLiteral("A1"), a1, QStringLiteral("NAD83, UTM zone 16N"),
                           QStringLiteral("EPSG:26916")));
        const ScannedFix a3 = fixNamed(georeferenced, QStringLiteral("A3"));
        CHECK(a3.coordinateSystem.toStdString() == "NAD83, latitude/longitude");
        CHECK(a3.placementSystem.toStdString() == "EPSG:4269");
        checkDegreeText(a3.coordinate, a3Longitude, a3Latitude, a3Elevation);
    }

    SECTION("a .srv alone resolves its grid fix and keeps its lat/long fix as written")
    {
        // Cavern refuses a lat/long #FIX with no .REF to name its datum.
        const QList<ScannedFix> alone =
            scannedFixes(testcasesDatasetSourcePath(QStringLiteral("walls/GEOREF.SRV")));
        CHECK(alone == QList<ScannedFix>{
                  placedFix(QStringLiteral("A1"), a1, QString(), QString()),
                  placedFix(QStringLiteral("A3"), QStringLiteral("W86:5:29.254 N37:10:3.195 219"),
                            QString(), QString())});
    }
}

TEST_CASE("scanWalls resolves a #FIX under the file's #UNITS as cavern reads it",
          "[Scanner][Walls][Attach]")
{
    SECTION("the walls_fix_units fixture: ORDER=NEU with an F suffix, feet with a variance, d:m:s")
    {
        const QList<ScannedFix> fixes =
            scannedFixes(datasetExternalCenterlinePath(QStringLiteral("walls_fix_units/walls_fix_units.wpj")));
        REQUIRE(fixes.size() == 3);
        const QString grid = QStringLiteral("NAD27 CONUS, UTM zone 16N");
        const QString gridCode = QStringLiteral("EPSG:26716");

        // "#UNITS Meters ORDER=NEU" then "4040000.00 520000.00 766.19F /Invented benchmark".
        CHECK(fixNamed(fixes, QStringLiteral("M1"))
              == placedFix(QStringLiteral("M1"), QStringLiteral("520000 4040000 233.534712"), grid, gridCode));
        // "#Units feet" then "1707000 13256000 1000 (R5,?)".
        CHECK(fixNamed(fixes, QStringLiteral("F1"))
              == placedFix(QStringLiteral("F1"), QStringLiteral("520293.6 4040428.8 304.8"), grid, gridCode));

        // "W86:57:30.5 N36:31:15.25 120", read in the .REF datum's lat/long.
        const ScannedFix l1 = fixNamed(fixes, QStringLiteral("L1"));
        CHECK(l1.coordinateSystem.toStdString() == "NAD27 CONUS, latitude/longitude");
        CHECK(l1.placementSystem.toStdString() == "EPSG:4267");
        checkDegreeText(l1.coordinate, -degrees(86.0, 57.0, 30.5), degrees(36.0, 31.0, 15.25), 120.0);
    }

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    SECTION("#UNITS SAVE and RESTORE bracket a change of length unit")
    {
        const QString srv = writeUtf8File(tempPath(tempDir, QStringLiteral("SAVED.SRV")),
                                          QByteArrayLiteral("#UNITS SAVE Feet\n"
                                                            "#FIX S1 100 200 300\n"
                                                            "#UNITS RESTORE\n"
                                                            "#FIX S2 100 200 300\n"));
        CHECK(scannedFixes(srv) == QList<ScannedFix>{
                  placedFix(QStringLiteral("S1"), QStringLiteral("30.48 60.96 91.44"), QString(), QString()),
                  placedFix(QStringLiteral("S2"), QStringLiteral("100 200 300"), QString(), QString())});
    }

    SECTION("#UNITS RESET returns to meters and ENU")
    {
        const QString srv = writeUtf8File(tempPath(tempDir, QStringLiteral("RESET.SRV")),
                                          QByteArrayLiteral("#UNITS Feet ORDER=NEU\n"
                                                            "#FIX R1 20 10 30\n"
                                                            "#UNITS RESET\n"
                                                            "#FIX R2 10 20 30\n"));
        CHECK(scannedFixes(srv) == QList<ScannedFix>{
                  placedFix(QStringLiteral("R1"), QStringLiteral("3.048 6.096 9.144"), QString(), QString()),
                  placedFix(QStringLiteral("R2"), QStringLiteral("10 20 30"), QString(), QString())});
    }

    SECTION("ORDER=NE reads north first and leaves the elevation 0")
    {
        const QString srv = writeUtf8File(tempPath(tempDir, QStringLiteral("NORTH.SRV")),
                                          QByteArrayLiteral("#UNITS ORDER=NE\n"
                                                            "#FIX N1 200 100\n"));
        CHECK(scannedFixes(srv) == QList<ScannedFix>{
                  placedFix(QStringLiteral("N1"), QStringLiteral("100 200 0"), QString(), QString())});
    }

    SECTION("a .wpj entry's .OPTIONS reach its .srv")
    {
        writeUtf8File(tempPath(tempDir, QStringLiteral("OPTFEET.SRV")),
                      QByteArrayLiteral("#FIX P1 10 20 30\n"
                                        "P1\tP2\t10.0\t0\t0\n"));
        const QString wpj = writeUtf8File(tempPath(tempDir, QStringLiteral("options.wpj")),
                                          QByteArrayLiteral(";WALLS Project file\n"
                                                            ".BOOK\tOptions Cave\n"
                                                            ".NAME\tOPTIONS-CAVE\n"
                                                            ".STATUS\t8\n"
                                                            ".SURVEY\tFeet Passage\n"
                                                            ".NAME\tOPTFEET\n"
                                                            ".OPTIONS\tFeet\n"
                                                            ".STATUS\t8\n"
                                                            ".ENDBOOK\n"));
        CHECK(scannedFixes(wpj) == QList<ScannedFix>{
                  placedFix(QStringLiteral("P1"), QStringLiteral("3.048 6.096 9.144"), QString(), QString())});
    }

    SECTION("a note and a comment drop off, and a line cavern's syntax refuses keeps its text")
    {
        const QString srv = writeUtf8File(tempPath(tempDir, QStringLiteral("NOTES.SRV")),
                                          QByteArrayLiteral("#FIX U1 1 2 3 /Spring ; found 1990\n"
                                                            "#FIX U2 north of the spring\n"));
        CHECK(scannedFixes(srv) == QList<ScannedFix>{
                  placedFix(QStringLiteral("U1"), QStringLiteral("1 2 3"), QString(), QString()),
                  placedFix(QStringLiteral("U2"), QStringLiteral("north of the spring"), QString(), QString())});
    }
}

TEST_CASE("scanWalls reads a .REF's datum from its index when the name is empty",
          "[Scanner][Walls][Attach]")
{
    const QString fixture = QStringLiteral("walls_empty_datum/walls_empty_datum.wpj");
    const QString a1 = QStringLiteral("580661.57 4113846.34 219");

    SECTION("the fixture's index 15 is NAD27 CONUS")
    {
        const auto scan = cwExternalCenterlineScanner::scan(datasetExternalCenterlinePath(fixture));
        INFO("scan: " << scan.errorMessage().toStdString());
        REQUIRE_FALSE(scan.hasError());
        CHECK(scan.value().dependencies.size() == 3);
        CHECK(hasFileNameInDeps(scan.value(), QStringLiteral("walls_empty_datum.wpj")));
        CHECK(hasFileNameInDeps(scan.value(), QStringLiteral("EAST.SRV")));
        CHECK(hasFileNameInDeps(scan.value(), QStringLiteral("WEST.SRV")));
        CHECK(scan.value().warnings.isEmpty());
        CHECK(withoutLocation(scan.value().fixes)
              == QList<ScannedFix>{placedFix(QStringLiteral("A1"), a1,
                                             QStringLiteral("NAD27 CONUS, UTM zone 16N"),
                                             QStringLiteral("EPSG:26716"))});
    }

    SECTION("Walls' legacy index codes")
    {
        const QByteArray original = [&]() {
            QFile file(datasetExternalCenterlinePath(fixture));
            REQUIRE(file.open(QFile::ReadOnly));
            return file.readAll();
        }();
        const QByteArray fixtureRef = QByteArrayLiteral(" 15 \"\"");
        REQUIRE(original.contains(fixtureRef));

        QByteArray index;
        QString expectedSystem;
        QString expectedPlacement;
        SECTION("254 is NAD27 CONUS")
        {
            index = QByteArrayLiteral("254");
            expectedSystem = QStringLiteral("NAD27 CONUS, UTM zone 16N");
            expectedPlacement = QStringLiteral("EPSG:26716");
        }
        SECTION("the other codes from 251 to 255 are WGS 1984")
        {
            index = QByteArrayLiteral("253");
            expectedSystem = QStringLiteral("WGS 1984, UTM zone 16N");
            expectedPlacement = QStringLiteral("EPSG:32616");
        }
        SECTION("an index outside Walls' table leaves the datum unnamed")
        {
            // Cavern knows no system for an unnamed datum, so none places the fix.
            index = QByteArrayLiteral("99");
            expectedSystem = QStringLiteral("UTM zone 16N");
        }

        QTemporaryDir tempDir;
        REQUIRE(tempDir.isValid());
        for (const QString& srv : {QStringLiteral("EAST.SRV"), QStringLiteral("WEST.SRV")}) {
            REQUIRE(QFile::copy(datasetExternalCenterlinePath(QStringLiteral("walls_empty_datum/%1").arg(srv)),
                                tempPath(tempDir, srv)));
        }
        QByteArray rewritten = original;
        rewritten.replace(fixtureRef, QByteArrayLiteral(" ") + index + QByteArrayLiteral(" \"\""));
        const QString wpjPath = writeUtf8File(tempPath(tempDir, QStringLiteral("legacy_index.wpj")),
                                              rewritten);

        CHECK(scannedFixes(wpjPath)
              == QList<ScannedFix>{placedFix(QStringLiteral("A1"), a1, expectedSystem, expectedPlacement)});
    }
}

TEST_CASE("scanWalls fails on a malformed entry .wpj with the parser's message",
          "[Scanner][Walls]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    writeUtf8File(tempPath(tempDir, QStringLiteral("MAIN.SRV")),
                  QByteArrayLiteral("A1\tA2\t5.0\t0\t0\n"));
    // The .REF on line 5 stops after the UTM zone.
    const QString wpjPath = writeUtf8File(tempPath(tempDir, QStringLiteral("malformed.wpj")),
                                          QByteArrayLiteral(";WALLS Project file\n"
                                                            ".BOOK\tTest Cave\n"
                                                            ".NAME\tTEST-CAVE\n"
                                                            ".STATUS\t8\n"
                                                            ".REF\t4113846.340 580661.570 16\n"
                                                            ".SURVEY\tMain\n"
                                                            ".NAME\tMAIN\n"
                                                            ".STATUS\t8\n"
                                                            ".ENDBOOK\n"));

    const auto scan = cwExternalCenterlineScanner::scan(wpjPath);
    REQUIRE(scan.hasError());
    const QString error = scan.errorMessage();
    INFO("error: " << error.toStdString());
    CHECK(error.contains(QStringLiteral("malformed.wpj")));
    CHECK(error.contains(QStringLiteral("line 5")));
    CHECK(error.contains(QStringLiteral("Expected")));
    CHECK_FALSE(error.contains(QStringLiteral("could not be read")));
}

TEST_CASE("scanWalls names a #FIX under the prefix levels in force", "[Scanner][Attach]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString srvPath = writeUtf8File(tempPath(tempDir, QStringLiteral("prefixed.srv")),
                                          QByteArrayLiteral("#PREFIX2 OUTER\n"
                                                            "#PREFIX XY\n"
                                                            "#FIX A1 0 0 0\n"
                                                            "#FIX AB:B1 0 0 0\n"
                                                            "A1\tA2\t5.0\t0\t0\n"));
    CHECK(scannedFixes(srvPath)
          == QList<ScannedFix>{{QStringLiteral("OUTER.XY.A1"), QStringLiteral("0 0 0"), QString()},
                               {QStringLiteral("OUTER.AB.B1"), QStringLiteral("0 0 0"), QString()}});
}

TEST_CASE("A scanned fix names the station the harvest says the file fixes", "[Scanner][Attach]")
{
    // The scan reads the fix from the text; the harvest asks cavern. Pinning the
    // two to one answer per format keeps the Fix Stations page's read-only row
    // spelled the way a node fix on the same station must be.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QString entryFile;
    SECTION("Survex: the enclosing *begin blocks")
    {
        entryFile = datasetExternalCenterlinePath(QStringLiteral("survex_blocks.svx"));
    }
    SECTION("Compass: the .mak line's DAT survey")
    {
        REQUIRE(QFile::copy(datasetExternalCenterlinePath(QStringLiteral("compass_solvable.dat")),
                            tempPath(tempDir, QStringLiteral("compass_solvable.dat"))));
        entryFile = writeUtf8File(tempPath(tempDir, QStringLiteral("solvable.mak")),
                                  QByteArrayLiteral("#compass_solvable.dat,S1[m,0,0,0];\n"));
    }
    SECTION("Walls: the #PREFIX levels in force")
    {
        writeUtf8File(tempPath(tempDir, QStringLiteral("MAIN.SRV")),
                      QByteArrayLiteral("#PREFIX2 OUTER\n"
                                        "#PREFIX XY\n"
                                        "#FIX A1 0 0 0\n"
                                        "A1\tA2\t5.0\t0\t0\n"));
        entryFile = writeUtf8File(tempPath(tempDir, QStringLiteral("prefixed.wpj")),
                                  QByteArrayLiteral(";WALLS Project file\n"
                                                    ".BOOK\tTest Cave\n"
                                                    ".NAME\tTEST-CAVE\n"
                                                    ".STATUS\t8\n"
                                                    ".SURVEY\tMain Passage\n"
                                                    ".NAME\tMAIN\n"
                                                    ".STATUS\t8\n"
                                                    ".ENDBOOK\n"));
    }

    const QList<ScannedFix> fixes = scannedFixes(entryFile);
    REQUIRE(fixes.size() == 1);

    const auto harvest = cwExternalStationHarvest::harvestWithFixes(entryFile);
    INFO("harvest: " << harvest.errorMessage().toStdString());
    REQUIRE_FALSE(harvest.hasError());
    REQUIRE(harvest.value().fixedNames.size() == 1);
    CHECK(cwStation::canonicalKey(fixes.first().station)
          == cwStation::canonicalKey(harvest.value().fixedNames.first()));
}

namespace {

//! \a scan's dependencies relative to the fixture directory \a fixtureDir,
//! in walk order.
QStringList dependenciesUnder(const ScanResult& scan, const QString& fixtureDir)
{
    const QDir dir(QFileInfo(datasetExternalCenterlinePath(fixtureDir)).canonicalFilePath());
    QStringList relative;
    for (const QString& path : scan.dependencies) {
        relative.append(dir.relativeFilePath(path));
    }
    return relative;
}

QStringList fixStations(const ScanResult& scan)
{
    QStringList stations;
    for (const ScannedFix& fix : scan.fixes) {
        stations.append(fix.station);
    }
    return stations;
}

} // namespace

TEST_CASE("compass_project scans its three DATs and warns on the absent DELTA.DAT",
          "[Scanner][Compass][SourcedFixtures]")
{
    auto result = cwExternalCenterlineScanner::scan(
        datasetExternalCenterlinePath(QStringLiteral("compass_project/PROJECT.MAK")));
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    CHECK(dependenciesUnder(scan, QStringLiteral("compass_project"))
          == QStringList({QStringLiteral("PROJECT.MAK"), QStringLiteral("ALPHA.DAT"),
                          QStringLiteral("BRAVO.DAT"), QStringLiteral("CHARLIE.DAT")}));
    REQUIRE(scan.warnings.size() == 1);
    CHECK(scan.warnings.first().contains(QStringLiteral("missing Compass reference")));
    CHECK(scan.warnings.first().contains(QStringLiteral("DELTA.DAT")));

    // ALPHA: A1..A7 over two surveys sharing A5; BRAVO: A5 (the link) and
    // B1..B3; CHARLIE: C1..C3. Every Compass station is a root station.
    CHECK(scan.blocks.isEmpty());
    CHECK(scan.rootStationCount == 13);
    CHECK_FALSE(scan.entryHasOwnShots);
    CHECK(scan.rootDate == QDate(2020, 3, 14));
    CHECK(fixStations(scan) == QStringList({QStringLiteral("alpha.A1"), QStringLiteral("charlie.C1")}));
}

TEST_CASE("walls_project scans its nested books' surveys and the N prefix level",
          "[Scanner][Walls][SourcedFixtures]")
{
    auto result = cwExternalCenterlineScanner::scan(
        datasetExternalCenterlinePath(QStringLiteral("walls_project/KAUA.WPJ")));
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    // NOTES.TXT is an other-type entry, which cavern skips, so the scan
    // neither reads it nor warns about it.
    CHECK(dependenciesUnder(scan, QStringLiteral("walls_project"))
          == QStringList({QStringLiteral("KAUA.WPJ"), QStringLiteral("FIX.SRV"),
                          QStringLiteral("SURVEYS/D01.SRV"), QStringLiteral("SURVEYS/D02.SRV"),
                          QStringLiteral("SURVEYS/N01.SRV")}));
    CHECK(scan.warnings.isEmpty());

    // K1 and D1..D4 have no prefix; N01.SRV's 1..3 sit under #PREFIX N, and
    // its ":D4" names the root's D4.
    REQUIRE(scan.blocks.size() == 1);
    CHECK(scan.blocks.first().path == QStringLiteral("N"));
    CHECK(scan.blocks.first().depth == 0);
    CHECK(scan.blocks.first().stationCount == 3);
    CHECK(scan.rootStationCount == 5);
    CHECK_FALSE(scan.entryHasOwnShots);
    CHECK(fixStations(scan) == QStringList({QStringLiteral("K1")}));
}

TEST_CASE("walls_two_caves scans one survey under East and two under West",
          "[Scanner][Walls][SourcedFixtures]")
{
    auto result = cwExternalCenterlineScanner::scan(
        datasetExternalCenterlinePath(QStringLiteral("walls_two_caves/TWO.WPJ")));
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    CHECK(dependenciesUnder(scan, QStringLiteral("walls_two_caves"))
          == QStringList({QStringLiteral("TWO.WPJ"), QStringLiteral("EAST/E1.SRV"),
                          QStringLiteral("WEST/W1.SRV"), QStringLiteral("WEST/W2.SRV")}));
    CHECK(scan.warnings.isEmpty());

    // E1..E3 and W1..W5 (W3 shared by W1.SRV and W2.SRV), none prefixed.
    CHECK(scan.blocks.isEmpty());
    CHECK(scan.rootStationCount == 8);
    CHECK_FALSE(scan.entryHasOwnShots);
    CHECK(fixStations(scan) == QStringList({QStringLiteral("E1"), QStringLiteral("W1")}));
}

TEST_CASE("survex_two_cs scans two blocks, each fixed in its own input system",
          "[Scanner][Survex][SourcedFixtures]")
{
    auto result = cwExternalCenterlineScanner::scan(
        datasetExternalCenterlinePath(QStringLiteral("survex_two_cs/master.svx")));
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    CHECK(dependenciesUnder(scan, QStringLiteral("survex_two_cs"))
          == QStringList({QStringLiteral("master.svx"), QStringLiteral("utm.svx"),
                          QStringLiteral("latlong.svx")}));
    CHECK(scan.warnings.isEmpty());
    CHECK(scan.entryDirectIncludes.size() == 2);

    REQUIRE(scan.blocks.size() == 2);
    CHECK(scan.blocks.at(0).path == QStringLiteral("utm"));
    CHECK(scan.blocks.at(0).depth == 0);
    CHECK(scan.blocks.at(0).stationCount == 3);
    CHECK(scan.blocks.at(1).path == QStringLiteral("latlong"));
    CHECK(scan.blocks.at(1).depth == 0);
    CHECK(scan.blocks.at(1).stationCount == 2);
    CHECK(scan.rootStationCount == 0);
    CHECK_FALSE(scan.entryHasOwnShots);

    REQUIRE(scan.fixes.size() == 2);
    CHECK(scan.fixes.at(0).station == QStringLiteral("utm.u1"));
    CHECK(scan.fixes.at(0).coordinateSystem == QStringLiteral("EPSG:32616"));
    CHECK(scan.fixes.at(1).station == QStringLiteral("latlong.l1"));
    CHECK(scan.fixes.at(1).coordinateSystem == QStringLiteral("LONG-LAT"));
    CHECK(scan.outputCoordinateSystem == QStringLiteral("EPSG:32616"));
}

namespace {

using cwSurveyNodeKind::Kind;

//! One scan row in a form a list comparison prints readably: depth, kind,
//! sourcePath, display name, cavernLabel, role, subtree station count, and the
//! gray / missing flags.
QString describe(const cwScanRow& row)
{
    const QString kindName = QString::fromLatin1(
        QMetaEnum::fromType<Kind>().valueToKey(static_cast<int>(row.kind)));
    QStringList roles;
    if (row.isNode) {
        roles.append(QStringLiteral("node"));
    }
    if (row.isUnit) {
        roles.append(QStringLiteral("unit"));
    }
    QString text = QStringLiteral("%1 %2 '%3' '%4' '%5' %6 %7")
                       .arg(row.depth)
                       .arg(kindName, row.sourcePath, row.displayName, row.cavernLabel,
                            roles.join(QLatin1Char('+')))
                       .arg(row.stationCount);
    if (!row.hasSurveyContent) {
        text += QStringLiteral(" gray");
    }
    if (row.missing) {
        text += QStringLiteral(" missing");
    }
    return text;
}

QStringList describe(const QList<cwScanRow>& rows)
{
    QStringList descriptions;
    for (const cwScanRow& row : rows) {
        descriptions.append(describe(row));
    }
    return descriptions;
}

cwScanRow rowAt(const ScanResult& scan, const QString& sourcePath)
{
    for (const cwScanRow& row : scan.rows) {
        if (row.sourcePath == sourcePath) {
            return row;
        }
    }
    FAIL("no scan row has sourcePath " << sourcePath.toStdString());
    return cwScanRow();
}

ScanResult scanFixture(const QString& fixture)
{
    auto result = cwExternalCenterlineScanner::scan(datasetExternalCenterlinePath(fixture));
    REQUIRE_FALSE(result.hasError());
    return result.value();
}

} // namespace

TEST_CASE("compass_project's tree is a project root, one node per DAT, one unit per survey",
          "[Scanner][Compass][SourcedFixtures]")
{
    const ScanResult scan = scanFixture(QStringLiteral("compass_project/PROJECT.MAK"));

    // ALPHA's two surveys share A5 (7 names in the file), BRAVO's link
    // station A5 is its own name until cavern equates it (4), CHARLIE
    // holds 3; the absent DELTA.DAT stays listed, gray and missing.
    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 CompassProject '' 'PROJECT' '' node 14"),
        QStringLiteral("1 CompassFile 'ALPHA.DAT' 'Alpha' 'alpha' node 7"),
        QStringLiteral("2 CompassFile 'ALPHA.DAT/ALPHA1' 'ALPHA1' '' unit 5"),
        QStringLiteral("2 CompassFile 'ALPHA.DAT/ALPHA2' 'ALPHA2' '' unit 3"),
        QStringLiteral("1 CompassFile 'BRAVO.DAT' 'Bravo' 'bravo' node 4"),
        QStringLiteral("2 CompassFile 'BRAVO.DAT/BRAVO1' 'BRAVO1' '' unit 4"),
        QStringLiteral("1 CompassFile 'CHARLIE.DAT' 'Charlie' 'charlie' node 3"),
        QStringLiteral("2 CompassFile 'CHARLIE.DAT/CHARLIE1' 'CHARLIE1' '' unit 3"),
        QStringLiteral("1 CompassFile 'DELTA.DAT' 'Delta' 'delta' node 0 gray missing"),
    }));

    // Unit stations are cavern-relative: the DAT's survey level, then the
    // station. The two ALPHA surveys share alpha.A5.
    const cwScanRow alpha1 = rowAt(scan, QStringLiteral("ALPHA.DAT/ALPHA1"));
    const cwScanRow alpha2 = rowAt(scan, QStringLiteral("ALPHA.DAT/ALPHA2"));
    CHECK(alpha1.stations == QStringList({QStringLiteral("alpha.A1"), QStringLiteral("alpha.A2"),
                                          QStringLiteral("alpha.A3"), QStringLiteral("alpha.A4"),
                                          QStringLiteral("alpha.A5")}));
    CHECK(alpha2.stations == QStringList({QStringLiteral("alpha.A5"), QStringLiteral("alpha.A6"),
                                          QStringLiteral("alpha.A7")}));
    CHECK(rowAt(scan, QStringLiteral("BRAVO.DAT/BRAVO1")).stations
          == QStringList({QStringLiteral("bravo.A5"), QStringLiteral("bravo.B1"),
                          QStringLiteral("bravo.B2"), QStringLiteral("bravo.B3")}));

    // Dates are each survey's own SURVEY DATE.
    CHECK(alpha1.date == QDate(2020, 3, 14));
    CHECK(alpha2.date == QDate(2020, 3, 15));
    CHECK(rowAt(scan, QStringLiteral("CHARLIE.DAT/CHARLIE1")).date == QDate(2022, 5, 9));

    // The Phase 3 fields keep their meaning until C7.2b.
    CHECK(scan.blocks.isEmpty());
    CHECK(scan.rootStationCount == 13);
}

TEST_CASE("A lone Compass .dat is a file root holding its surveys with bare names",
          "[Scanner][Compass]")
{
    const ScanResult scan = scanFixture(QStringLiteral("compass_simple.dat"));

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 CompassFile '' 'compass_simple' '' node 3"),
        QStringLiteral("1 CompassFile 'A' 'A' '' unit 3"),
    }));
    CHECK(scan.rows.at(1).stations == QStringList({QStringLiteral("A1"), QStringLiteral("A2"),
                                                   QStringLiteral("A3")}));
    CHECK(scan.rows.at(1).date == QDate(2025, 1, 1));
}

TEST_CASE("A Compass survey without a name takes a positional segment",
          "[Scanner][Compass]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // The first survey is named "2"; the second writes no name and is the
    // second section, so a bare position would collide with the first.
    const QString path = tempPath(tempDir, QStringLiteral("unnamed.dat"));
    writeUtf8File(path,
                  QByteArrayLiteral("Cave\n"
                                    "SURVEY NAME: 2\n"
                                    "SURVEY TEAM:\n"
                                    "\n"
                                    "DECLINATION: 0.00  FORMAT: DDDDLRUDLADN\n"
                                    "\n"
                                    "FROM TO LENGTH BEARING INC\n"
                                    "S1 S2 10.00 0.00 0.00\n"
                                    "\f\n"
                                    "Cave\n"
                                    "SURVEY TEAM:\n"
                                    "\n"
                                    "DECLINATION: 0.00  FORMAT: DDDDLRUDLADN\n"
                                    "\n"
                                    "FROM TO LENGTH BEARING INC\n"
                                    "T1 T2 10.00 0.00 0.00\n"));

    auto result = cwExternalCenterlineScanner::scanCompass(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 CompassFile '' 'unnamed' '' node 4"),
        QStringLiteral("1 CompassFile '2' '2' '' unit 2"),
        QStringLiteral("1 CompassFile '#2' '#2' '' unit 2"),
    }));
    CHECK(rowAt(scan, QStringLiteral("#2")).stations
          == QStringList({QStringLiteral("T1"), QStringLiteral("T2")}));
}

TEST_CASE("walls_project's tree nests books, grays the books with no survey data",
          "[Scanner][Walls][SourcedFixtures]")
{
    const ScanResult scan = scanFixture(QStringLiteral("walls_project/KAUA.WPJ"));

    // Drawings holds only the other-type NOTES.TXT and Fixed Points only a
    // #FIX, so neither has a station-bearing unit. FIX.SRV writes no shots
    // and makes no unit.
    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 WallsBook '' 'KAUA' '' node 8"),
        QStringLiteral("1 WallsBook 'DRAWINGS' 'Drawings' '' node 0 gray"),
        QStringLiteral("1 WallsBook 'FIXED' 'Fixed Points' '' node 0 gray"),
        QStringLiteral("1 WallsBook 'SURVEYS' 'Surveys' '' node 8"),
        QStringLiteral("2 WallsBook 'SURVEYS/DEC2001' 'Dec 2001 Surveys' '' node 8"),
        QStringLiteral("3 WallsBook 'SURVEYS/DEC2001/D01' 'D Survey 1' '' unit 3"),
        QStringLiteral("3 WallsBook 'SURVEYS/DEC2001/D02' 'D Survey 2' '' unit 3"),
        QStringLiteral("3 WallsBook 'SURVEYS/DEC2001/N01' 'N Surveys' '' unit 4"),
    }));

    // The #PREFIX in force is part of each name; ":D4" names the root's D4,
    // the station D02 and N01 share.
    CHECK(rowAt(scan, QStringLiteral("SURVEYS/DEC2001/N01")).stations
          == QStringList({QStringLiteral("D4"), QStringLiteral("N.1"), QStringLiteral("N.2"),
                          QStringLiteral("N.3")}));
    CHECK(rowAt(scan, QStringLiteral("SURVEYS/DEC2001/D02")).stations
          == QStringList({QStringLiteral("D2"), QStringLiteral("D3"), QStringLiteral("D4")}));
    CHECK(rowAt(scan, QStringLiteral("SURVEYS/DEC2001/D01")).date == QDate(2001, 12, 1));
    CHECK(rowAt(scan, QStringLiteral("SURVEYS/DEC2001/N01")).date == QDate(2001, 12, 3));
}

TEST_CASE("walls_two_caves' tree holds one unit under East and two under West",
          "[Scanner][Walls][SourcedFixtures]")
{
    const ScanResult scan = scanFixture(QStringLiteral("walls_two_caves/TWO.WPJ"));

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 WallsBook '' 'TWO' '' node 8"),
        QStringLiteral("1 WallsBook 'EAST' 'East Cave' '' node 3"),
        QStringLiteral("2 WallsBook 'EAST/E1' 'East Survey' '' unit 3"),
        QStringLiteral("1 WallsBook 'WEST' 'West Cave' '' node 5"),
        QStringLiteral("2 WallsBook 'WEST/W1' 'West Survey 1' '' unit 3"),
        QStringLiteral("2 WallsBook 'WEST/W2' 'West Survey 2' '' unit 3"),
    }));
    CHECK(rowAt(scan, QStringLiteral("WEST/W1")).stations.contains(QStringLiteral("W3")));
    CHECK(rowAt(scan, QStringLiteral("WEST/W2")).stations.contains(QStringLiteral("W3")));
    CHECK(rowAt(scan, QStringLiteral("WEST/W2")).date == QDate(2010, 6, 3));
}

TEST_CASE("walls_detached scans only the attached survey, as cavern does",
          "[Scanner][Walls][SourcedFixtures]")
{
    // The root book and SHEETS and FLOAT carry .STATUS bit 0x2 (detached).
    // Cavern processes the root anyway and skips the other two, so it places
    // E1..E3 only.
    const ScanResult scan = scanFixture(QStringLiteral("walls_detached/walls_detached.wpj"));

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 WallsBook '' 'walls_detached' '' node 3"),
        QStringLiteral("1 WallsBook 'CAVE' 'Main Cave' '' node 3"),
        QStringLiteral("2 WallsBook 'CAVE/ENTRANCE' 'Entrance Survey' '' unit 3"),
    }));
    CHECK(rowAt(scan, QStringLiteral("CAVE/ENTRANCE")).stations
          == QStringList({QStringLiteral("E1"), QStringLiteral("E2"), QStringLiteral("E3")}));

    CHECK(withoutLocation(scan.fixes)
          == QList<ScannedFix>{placedFix(QStringLiteral("E1"), QStringLiteral("520000 4040000 200"),
                                         QStringLiteral("NAD27 CONUS, UTM zone 16N"),
                                         QStringLiteral("EPSG:26716"))});
    CHECK(dependenciesUnder(scan, QStringLiteral("walls_detached"))
          == QStringList({QStringLiteral("walls_detached.wpj"), QStringLiteral("ENTRANCE.SRV")}));
    CHECK(scan.warnings.isEmpty());
    CHECK(scan.blocks.isEmpty());
    CHECK(scan.rootStationCount == 3);
}

TEST_CASE("A lone Walls .srv is a book root holding the file's unit",
          "[Scanner][Walls]")
{
    const ScanResult scan = scanFixture(QStringLiteral("MAIN.SRV"));

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 WallsBook '' 'MAIN' '' node 4"),
        QStringLiteral("1 WallsBook 'MAIN' 'MAIN' '' unit 4"),
    }));
    CHECK(scan.rows.at(1).date == QDate(2023, 5, 10));
}

TEST_CASE("survex_master's tree is a file root with one block trip per included cave",
          "[Scanner][Survex][SourcedFixtures]")
{
    const ScanResult scan = scanFixture(QStringLiteral("survex_master/master.svx"));

    // The included files are transparent: each block holds shots and no
    // nested block, so it is a unit only. The master writes no shots of its own.
    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'master' '' node 9"),
        QStringLiteral("1 SurvexBlock 'alpha' 'alpha' 'alpha' unit 2"),
        QStringLiteral("1 SurvexBlock 'bravo' 'bravo' 'bravo' unit 3"),
        QStringLiteral("1 SurvexBlock 'charlie' 'charlie' 'charlie' unit 4"),
    }));
    CHECK(rowAt(scan, QStringLiteral("alpha")).stations
          == QStringList({QStringLiteral("alpha.a1"), QStringLiteral("alpha.a2")}));
}

TEST_CASE("survex_nested's tree nests blocks across *include boundaries",
          "[Scanner][Survex][SourcedFixtures]")
{
    const ScanResult scan = scanFixture(QStringLiteral("survex_nested.svx"));

    // Nested writes no shots of its own, so it is a node only; Passage nests
    // no block, so it is a unit only. Entrance's "Passage.P1" tie is a name
    // Entrance writes, so it counts there and in Passage alike.
    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'survex_nested' '' node 5"),
        QStringLiteral("1 SurvexBlock 'Nested' 'Nested' 'Nested' node 5"),
        QStringLiteral("2 SurvexBlock 'Nested/Entrance' 'Entrance' 'Entrance' node+unit 5"),
        QStringLiteral("3 SurvexBlock 'Nested/Entrance/Passage' 'Passage' 'Passage' unit 3"),
    }));
    CHECK(rowAt(scan, QStringLiteral("Nested/Entrance")).stations
          == QStringList({QStringLiteral("Nested.Entrance.E1"), QStringLiteral("Nested.Entrance.E2"),
                          QStringLiteral("Nested.Entrance.Passage.P1")}));
    CHECK(rowAt(scan, QStringLiteral("Nested/Entrance/Passage")).stations
          == QStringList({QStringLiteral("Nested.Entrance.Passage.P1"),
                          QStringLiteral("Nested.Entrance.Passage.P2"),
                          QStringLiteral("Nested.Entrance.Passage.P3")}));
}

TEST_CASE("A Survex root's own shots are the root's unit; an empty block is gray",
          "[Scanner][Survex]")
{
    {
        const ScanResult scan = scanFixture(QStringLiteral("survex_root_and_block.svx"));
        // The root writes r1..r3 and the side.s1 tie, which names a station
        // of side's and stays out of the root's unit. side is a leaf, a unit.
        CHECK(describe(scan.rows) == QStringList({
            QStringLiteral("0 SurvexFile '' 'survex_root_and_block' '' node+unit 5"),
            QStringLiteral("1 SurvexBlock 'side' 'side' 'side' unit 2"),
        }));
        CHECK(scan.rows.at(0).stations
              == QStringList({QStringLiteral("r1"), QStringLiteral("r2"), QStringLiteral("r3")}));
    }
    {
        const ScanResult scan = scanFixture(QStringLiteral("survex_blocks.svx"));
        const cwScanRow sump = rowAt(scan, QStringLiteral("doghill/big-passage/sump"));
        CHECK(sump.isNode);
        CHECK_FALSE(sump.isUnit);
        CHECK_FALSE(sump.hasSurveyContent);
        CHECK(sump.date == QDate(2025, 3, 15));
        // A range's start date, as the block takes it.
        CHECK(rowAt(scan, QStringLiteral("doghill/big-passage")).date == QDate(2024, 1, 5));
    }
}

TEST_CASE("scanSurvex reads a *fix's reference keyword as a flag, not as coordinate text",
          "[ExternalCenterlineScanner][ReferenceFix]")
{
    auto result = cwExternalCenterlineScanner::scan(
        datasetExternalCenterlinePath(QStringLiteral("survex_reference_fix/reference_fix.svx")));
    INFO("scan: " << result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    const QList<ScannedFix>& fixes = result.value().fixes;
    REQUIRE(fixes.size() == 2);

    const ScannedFix& marked = fixes.at(0);
    CHECK(marked.station == QStringLiteral("ref.A"));
    INFO("A's coordinate: " << marked.coordinate.toStdString());
    CHECK(marked.coordinate == QStringLiteral("10 20 30"));
    CHECK(marked.reference);

    const ScannedFix& plain = fixes.at(1);
    CHECK(plain.station == QStringLiteral("ref.B"));
    CHECK(plain.coordinate == QStringLiteral("1 2 3"));
    CHECK_FALSE(plain.reference);
}

TEST_CASE("scanSurvex matches the reference keyword in any case, as cavern does",
          "[ExternalCenterlineScanner][ReferenceFix]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString path = writeUtf8File(tempPath(tempDir, QStringLiteral("upper.svx")),
                                       QByteArrayLiteral("*fix a REFERENCE 1 2 3\n"
                                                         "*data normal from to tape compass clino\n"
                                                         "a b 1.0 0 0\n"));
    const QList<ScannedFix> fixes = scannedFixes(path);
    REQUIRE(fixes.size() == 1);
    INFO("coordinate: " << fixes.first().coordinate.toStdString());
    CHECK(fixes.first().coordinate == QStringLiteral("1 2 3"));
    CHECK(fixes.first().reference);
}

namespace {

const QString kBlockRowsDir = QStringLiteral("survex_block_rows");

ScanResult scanBlockRows(const QString& entry)
{
    return scanFixture(kBlockRowsDir + QLatin1Char('/') + entry);
}

//! Each fix as "<station> node '<nodePath>' trip '<tripPath>'".
QStringList describeFixHomes(const ScanResult& scan)
{
    QStringList homes;
    for (const ScannedFix& fix : scan.fixes) {
        homes.append(QStringLiteral("%1 node '%2' trip '%3'")
                         .arg(fix.station, fix.nodePath, fix.tripPath));
    }
    return homes;
}

} // namespace

TEST_CASE("A leaf Survex block with shots is one unit row, and its fix is the unit's",
          "[Scanner][Survex][SurvexBlockRows]")
{
    const ScanResult scan = scanBlockRows(QStringLiteral("leaf.svx"));

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'leaf' '' node 3"),
        QStringLiteral("1 SurvexBlock 'simmons_hall' 'simmons_hall' 'simmons_hall' unit 3"),
    }));
    CHECK(describeFixHomes(scan) == QStringList({
        QStringLiteral("simmons_hall.st1 node '' trip 'simmons_hall'"),
    }));
}

TEST_CASE("A Survex block with its own shots and a nested block is a node and a unit",
          "[Scanner][Survex][SurvexBlockRows]")
{
    const ScanResult scan = scanBlockRows(QStringLiteral("mixed.svx"));

    // cave keeps its node to hold inner; inner is a leaf, a unit only.
    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'mixed' '' node 4"),
        QStringLiteral("1 SurvexBlock 'cave' 'cave' 'cave' node+unit 4"),
        QStringLiteral("2 SurvexBlock 'cave/inner' 'inner' 'inner' unit 2"),
    }));
    CHECK(describeFixHomes(scan) == QStringList({
        QStringLiteral("cave.c1 node 'cave' trip ''"),
        QStringLiteral("cave.inner.i2 node 'cave' trip 'cave/inner'"),
    }));
}

TEST_CASE("One Survex block name under two parent scopes is two unit rows",
          "[Scanner][Survex][SurvexBlockRows]")
{
    const ScanResult scan = scanBlockRows(QStringLiteral("twoscopes.svx"));

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'twoscopes' '' node 4"),
        QStringLiteral("1 SurvexBlock 'east' 'east' 'east' node 2"),
        QStringLiteral("2 SurvexBlock 'east/higharch' 'higharch' 'higharch' unit 2"),
        QStringLiteral("1 SurvexBlock 'west' 'west' 'west' node 2"),
        QStringLiteral("2 SurvexBlock 'west/higharch' 'higharch' 'higharch' unit 2"),
    }));
    CHECK(describeFixHomes(scan) == QStringList({
        QStringLiteral("east.higharch.h1 node 'east' trip 'east/higharch'"),
        QStringLiteral("west.higharch.h1 node 'west' trip 'west/higharch'"),
    }));
}

TEST_CASE("A Survex block opened twice in one scope, with a file included twice, is one unit row",
          "[Scanner][Survex][SurvexBlockRows]")
{
    const ScanResult scan = scanBlockRows(QStringLiteral("repeat.svx"));

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'repeat' '' node 4"),
        QStringLiteral("1 SurvexBlock 'higharch' 'higharch' 'higharch' unit 4"),
    }));
    CHECK(rowAt(scan, QStringLiteral("higharch")).stations
          == QStringList({QStringLiteral("higharch.h1"), QStringLiteral("higharch.h2"),
                          QStringLiteral("higharch.h3"), QStringLiteral("higharch.h4")}));
    CHECK(describeFixHomes(scan) == QStringList({
        QStringLiteral("higharch.h1 node '' trip 'higharch'"),
    }));
}

TEST_CASE("A Survex block holding only an *include is a node over the included file's blocks",
          "[Scanner][Survex][SurvexBlockRows]")
{
    const ScanResult scan = scanBlockRows(QStringLiteral("includeonly.svx"));

    // Two real scopes, higharch and higharch.higharch; the inner one is the leaf.
    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'includeonly' '' node 2"),
        QStringLiteral("1 SurvexBlock 'higharch' 'higharch' 'higharch' node 2"),
        QStringLiteral("2 SurvexBlock 'higharch/higharch' 'higharch' 'higharch' unit 2"),
    }));
    CHECK(describeFixHomes(scan) == QStringList({
        QStringLiteral("higharch.higharch.x1 node 'higharch' trip 'higharch/higharch'"),
    }));
}

TEST_CASE("A Survex file included from two scopes is scanned in each of them",
          "[Scanner][Survex][SurvexBlockRows]")
{
    const ScanResult scan = scanBlockRows(QStringLiteral("shared_scopes.svx"));

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'shared_scopes' '' node 4"),
        QStringLiteral("1 SurvexBlock 'one' 'one' 'one' unit 2"),
        QStringLiteral("1 SurvexBlock 'two' 'two' 'two' unit 2"),
    }));
    CHECK(rowAt(scan, QStringLiteral("two")).stations
          == QStringList({QStringLiteral("two.s1"), QStringLiteral("two.s2")}));
    CHECK(describeFixHomes(scan) == QStringList({
        QStringLiteral("one.s1 node '' trip 'one'"),
        QStringLiteral("two.s1 node '' trip 'two'"),
    }));
    // The second scope reads the file again and lists it once.
    CHECK(dependenciesUnder(scan, kBlockRowsDir)
          == QStringList({QStringLiteral("shared_scopes.svx"), QStringLiteral("shared.svx")}));
    CHECK(scan.warnings.isEmpty());
}

namespace {

const QString kDeclinationDir = QStringLiteral("survex_declination");

//! A unit's declination in a form a comparison prints readably: the kind, then
//! the degrees for a Value and the location and system for an Auto.
QString describe(const cwFileDeclination& declination)
{
    using DeclinationKind = cwFileDeclination::Kind;
    switch (declination.kind) {
    case DeclinationKind::Unknown:
        return QStringLiteral("Unknown");
    case DeclinationKind::Inherited:
        return QStringLiteral("Inherited");
    case DeclinationKind::None:
        return QStringLiteral("None");
    case DeclinationKind::Value:
        return QStringLiteral("Value %1").arg(declination.degrees);
    case DeclinationKind::Auto:
        return QStringLiteral("Auto %1 %2 %3 %4")
            .arg(declination.autoLocation.x, 0, 'f', 0)
            .arg(declination.autoLocation.y, 0, 'f', 0)
            .arg(declination.autoLocation.z, 0, 'f', 0)
            .arg(declination.autoCoordinateSystem);
    }
    return QString();
}

std::string unitDeclination(const ScanResult& scan, const QString& sourcePath)
{
    const cwScanRow row = rowAt(scan, sourcePath);
    REQUIRE(row.isUnit);
    return describe(row.declination).toStdString();
}

//! The sourcePaths of the units whose declination the scan cannot tell, joined with ", ".
std::string unknownUnits(const ScanResult& scan)
{
    QStringList sourcePaths;
    for (const cwScanRow& row : scan.rows) {
        if (row.isUnit && row.declination.kind == cwFileDeclination::Kind::Unknown) {
            sourcePaths.append(row.sourcePath);
        }
    }
    return sourcePaths.join(QStringLiteral(", ")).toStdString();
}

} // namespace

TEST_CASE("Each Survex unit carries the declination cavern applies to its shots",
          "[Scanner][Survex][Declination]")
{
    const ScanResult scan = scanFixture(kDeclinationDir + QStringLiteral("/declination.svx"));

    // *calibrate declination X corrects bearings by -X; *declination D by +D.
    CHECK(unitDeclination(scan, QStringLiteral("fileset")) == "Value -2.5");
    CHECK(unitDeclination(scan, QStringLiteral("modern")) == "Value 3.25");
    // *declination auto reads its point in the *cs in force, as written.
    CHECK(unitDeclination(scan, QStringLiteral("autoset"))
          == "Auto 600000 4100000 200 EPSG:32616");
    // A block that sets nothing gets cavern's default of 0.
    CHECK(unitDeclination(scan, QStringLiteral("silent")) == "None");
    // A nested block inherits its parent's directive.
    CHECK(unitDeclination(scan, QStringLiteral("parent/child")) == "Value -1");
    // Only the unit whose shots take two declinations has no one value.
    CHECK(unknownUnits(scan) == "mixed");
}

TEST_CASE("A Compass base location with a named datum makes each section IGRF auto",
          "[Scanner][Compass][Declination]")
{
    const ScanResult scan = scanFixture(kDeclinationDir + QStringLiteral("/compass_declination/PROJ.MAK"));

    // Cavern ignores the DECLINATION headers once '@' and '&' are both in force.
    const std::string expected = "Auto 600000 4100000 200 EPSG:32616";
    CHECK(unitDeclination(scan, QStringLiteral("A.DAT/A1")) == expected);
    CHECK(unitDeclination(scan, QStringLiteral("A.DAT/A2")) == expected);
}

TEST_CASE("A Compass base location without a datum leaves each section its header's declination",
          "[Scanner][Compass][Declination]")
{
    const ScanResult scan = scanFixture(kDeclinationDir + QStringLiteral("/compass_declination/NODATUM.MAK"));

    CHECK(unitDeclination(scan, QStringLiteral("A.DAT/A1")) == "Value 4");
    CHECK(unitDeclination(scan, QStringLiteral("A.DAT/A2")) == "Value 5.5");
}

TEST_CASE("A Compass .dat on its own carries each section's header declination",
          "[Scanner][Compass][Declination]")
{
    const ScanResult scan = scanFixture(kDeclinationDir + QStringLiteral("/compass_declination/A.DAT"));

    CHECK(unitDeclination(scan, QStringLiteral("A1")) == "Value 4");
    CHECK(unitDeclination(scan, QStringLiteral("A2")) == "Value 5.5");
}

TEST_CASE("A Walls unit carries its #UNITS DECL",
          "[Scanner][Walls][Declination]")
{
    const ScanResult scan = scanFixture(kDeclinationDir + QStringLiteral("/walls_declination/DECL.SRV"));

    CHECK(unitDeclination(scan, QStringLiteral("DECL")) == "Value 3.5");
}

TEST_CASE("A Walls #UNITS RESTORE brings back the declination SAVE kept",
          "[Scanner][Walls][Declination]")
{
    const ScanResult scan = scanFixture(kDeclinationDir + QStringLiteral("/walls_declination/SAVE.SRV"));

    CHECK(unitDeclination(scan, QStringLiteral("SAVE")) == "Value 3.5");
}

TEST_CASE("A Survex passage line leaves the unit's declination to its shots",
          "[Scanner][Survex][Declination]")
{
    const ScanResult scan = scanFixture(kDeclinationDir + QStringLiteral("/passage.svx"));

    CHECK(unitDeclination(scan, QStringLiteral("lrud")) == "Value -2");
}

namespace {

const QString kAnonymousTripsFixture = QStringLiteral("survex_anonymous_trips.svx");

} // namespace

TEST_CASE("Each anonymous Survex block with shots is a unit-only trip row under its named block",
          "[Scanner][Survex][AnonymousTrips]")
{
    const ScanResult scan = scanFixture(kAnonymousTripsFixture);

    // alpha's distinct names: a1 a2 tie hub (trip 1), b9 b1 (trip 2),
    // c1 c2 (trip 3) and z1 (its own shot), so tie and hub count once.
    // The shot-less block between trips 2 and 3 makes no row, so the
    // last trip is #3.
    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'survex_anonymous_trips' '' node 9"),
        QStringLiteral("1 SurvexBlock 'alpha' 'alpha' 'alpha' node+unit 9"),
        QStringLiteral("2 SurvexBlock 'alpha/#1' 'Trip 1' '' unit 4"),
        QStringLiteral("2 SurvexBlock 'alpha/#2' 'Trip 2' '' unit 4"),
        QStringLiteral("2 SurvexBlock 'alpha/#3' 'Trip 3' '' unit 3"),
    }));

    CHECK(rowAt(scan, QStringLiteral("alpha")).stations
          == QStringList({QStringLiteral("alpha.c2"), QStringLiteral("alpha.z1")}));
    CHECK(rowAt(scan, QStringLiteral("alpha/#1")).stations
          == QStringList({QStringLiteral("alpha.a1"), QStringLiteral("alpha.a2"),
                          QStringLiteral("alpha.hub"), QStringLiteral("alpha.tie")}));
    CHECK(rowAt(scan, QStringLiteral("alpha/#3")).stations
          == QStringList({QStringLiteral("alpha.c1"), QStringLiteral("alpha.c2"),
                          QStringLiteral("alpha.hub")}));

    // Anonymous blocks build no cavern naming level, so alpha stays the
    // only block and folds every trip's stations into its count.
    REQUIRE(scan.blocks.size() == 1);
    CHECK(scan.blocks.first().path == QStringLiteral("alpha"));
    CHECK(scan.blocks.first().depth == 0);
    CHECK(scan.blocks.first().stationCount == 9);
    CHECK(scan.blocks.first().date == QDate(2020, 1, 11));
}

TEST_CASE("Each anonymous Survex trip row carries its own *date",
          "[Scanner][Survex][AnonymousTrips]")
{
    const ScanResult scan = scanFixture(kAnonymousTripsFixture);

    CHECK(rowAt(scan, QStringLiteral("alpha/#1")).date == QDate(2020, 1, 11));
    CHECK(rowAt(scan, QStringLiteral("alpha/#2")).date == QDate(2020, 2, 22));
    CHECK(rowAt(scan, QStringLiteral("alpha/#3")).date == QDate(2020, 3, 30));
    // The named block keeps the first date in its scope, as its cwScanBlock does.
    CHECK(rowAt(scan, QStringLiteral("alpha")).date == QDate(2020, 1, 11));
}

TEST_CASE("Each anonymous Survex trip row carries its own *calibrate declination",
          "[Scanner][Survex][AnonymousTrips][Declination]")
{
    const ScanResult scan = scanFixture(kAnonymousTripsFixture);

    CHECK(unitDeclination(scan, QStringLiteral("alpha/#1")) == "Value -1.5");
    CHECK(unitDeclination(scan, QStringLiteral("alpha/#2")) == "Value -2.5");
    CHECK(unitDeclination(scan, QStringLiteral("alpha/#3")) == "Value -3.5");
    // alpha's own shot sits outside every trip's *calibrate.
    CHECK(unitDeclination(scan, QStringLiteral("alpha")) == "None");
    CHECK(unknownUnits(scan).empty());
}

TEST_CASE("A nested anonymous block's shots belong to the anonymous trip around it",
          "[Scanner][Survex][AnonymousTrips]")
{
    const ScanResult scan = scanFixture(kAnonymousTripsFixture);

    // Trip 2's first shot is inside its nested *flags duplicate block,
    // yet the trip is still the second row and holds that shot's b9.
    const cwScanRow trip2 = rowAt(scan, QStringLiteral("alpha/#2"));
    CHECK(trip2.displayName == QStringLiteral("Trip 2"));
    CHECK(trip2.cavernLabel.isEmpty());
    CHECK(trip2.isUnit);
    CHECK_FALSE(trip2.isNode);
    CHECK(trip2.stations
          == QStringList({QStringLiteral("alpha.b1"), QStringLiteral("alpha.b9"),
                          QStringLiteral("alpha.hub"), QStringLiteral("alpha.tie")}));
}

TEST_CASE("A *fix inside an anonymous Survex block is homed to that block's trip",
          "[Scanner][Survex][AnonymousTrips]")
{
    const ScanResult scan = scanFixture(kAnonymousTripsFixture);

    // The *fix comes before trip 1's first shot.
    CHECK(describeFixHomes(scan) == QStringList({
        QStringLiteral("alpha.a1 node 'alpha' trip 'alpha/#1'"),
    }));
}

TEST_CASE("a *date inside an anonymous *begin dates its trip row and the enclosing block",
          "[Scanner][Blocks][AnonymousTrips]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("anon.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin Outer\n"
                                    "*begin\n"
                                    "*date 2024.02.03\n"
                                    "a1 a2 1.0 0 0\n"
                                    "*end\n"
                                    "*end Outer\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();
    REQUIRE(scan.blocks.size() == 1);
    CHECK(scan.blocks.first().path == QStringLiteral("Outer"));
    CHECK(scan.blocks.first().date == QDate(2024, 2, 3));

    const cwScanRow trip = rowAt(scan, QStringLiteral("Outer/#1"));
    CHECK(trip.date == QDate(2024, 2, 3));
    CHECK(trip.cavernLabel.isEmpty());
    CHECK(trip.isUnit);
    CHECK_FALSE(trip.isNode);
}

TEST_CASE("an anonymous *begin is a trip row while its shots still count toward the parent block",
          "[Scanner][Blocks][AnonymousTrips]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("anonymous.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin Named\n"
                                    "*data normal from to tape compass clino\n"
                                    "N1 N2 5.0 0 0\n"
                                    "*begin\n"
                                    "N2 N3 5.0 0 0\n"
                                    "*end\n"
                                    "N3 N4 5.0 0 0\n"
                                    "*end Named\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    REQUIRE(scan.blocks.size() == 1);
    CHECK(scan.blocks.first().path == QStringLiteral("Named"));
    CHECK(scan.blocks.first().depth == 0);
    CHECK(scan.blocks.first().stationCount == 4);  // N1 .. N4
    CHECK(scan.entryHasOwnShots);

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'anonymous' '' node 4"),
        QStringLiteral("1 SurvexBlock 'Named' 'Named' 'Named' node+unit 4"),
        QStringLiteral("2 SurvexBlock 'Named/#1' 'Trip 1' '' unit 2"),
    }));
    CHECK(rowAt(scan, QStringLiteral("Named/#1")).stations
          == QStringList({QStringLiteral("Named.N2"), QStringLiteral("Named.N3")}));
}

TEST_CASE("An anonymous Survex block at the file root is a trip row under the root",
          "[Scanner][Survex][AnonymousTrips]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // side.s1 names a station inside the side block, so the root trip
    // leaves it out, as the root's own unit does.
    const QString path = tempPath(tempDir, QStringLiteral("root_anonymous.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin\n"
                                    "*fix r1 0 0 0\n"
                                    "r1 r2 5.0 0 0\n"
                                    "r2 side.s1 5.0 90 0\n"
                                    "*end\n"
                                    "*begin side\n"
                                    "s1 s2 5.0 0 0\n"
                                    "*end side\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'root_anonymous' '' node 4"),
        QStringLiteral("1 SurvexBlock '#1' 'Trip 1' '' unit 2"),
        QStringLiteral("1 SurvexBlock 'side' 'side' 'side' unit 2"),
    }));
    CHECK(rowAt(scan, QStringLiteral("#1")).stations
          == QStringList({QStringLiteral("r1"), QStringLiteral("r2")}));
    CHECK(scan.rootStationCount == 2);
    CHECK(describeFixHomes(scan) == QStringList({
        QStringLiteral("r1 node '' trip '#1'"),
    }));
}

TEST_CASE("A named Survex block inside an anonymous one hangs from the named container",
          "[Scanner][Survex][AnonymousTrips]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Cavern skips the anonymous level, so Inner is Outer.Inner; the
    // anonymous trip keeps only the shots outside Inner.
    const QString path = tempPath(tempDir, QStringLiteral("named_in_anonymous.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin Outer\n"
                                    "*begin\n"
                                    "o1 o2 5.0 0 0\n"
                                    "*begin Inner\n"
                                    "i1 i2 5.0 0 0\n"
                                    "*end Inner\n"
                                    "o2 o3 5.0 0 0\n"
                                    "*end\n"
                                    "*end Outer\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'named_in_anonymous' '' node 5"),
        QStringLiteral("1 SurvexBlock 'Outer' 'Outer' 'Outer' node 5"),
        QStringLiteral("2 SurvexBlock 'Outer/#1' 'Trip 1' '' unit 3"),
        QStringLiteral("2 SurvexBlock 'Outer/Inner' 'Inner' 'Inner' unit 2"),
    }));
    CHECK(rowAt(scan, QStringLiteral("Outer/#1")).stations
          == QStringList({QStringLiteral("Outer.o1"), QStringLiteral("Outer.o2"),
                          QStringLiteral("Outer.o3")}));

    REQUIRE(scan.blocks.size() == 2);
    CHECK(scan.blocks.at(0).path == QStringLiteral("Outer"));
    CHECK(scan.blocks.at(0).stationCount == 3);
    CHECK(scan.blocks.at(1).path == QStringLiteral("Outer.Inner"));
    CHECK(scan.blocks.at(1).stationCount == 2);
}

TEST_CASE("Shots an *include adds inside an anonymous Survex block belong to its trip",
          "[Scanner][Survex][AnonymousTrips]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    writeUtf8File(tempPath(tempDir, QStringLiteral("part.svx")),
                  QByteArrayLiteral("p1 p2 5.0 0 0\n"
                                    "p2 p3 5.0 0 0\n"));
    const QString path = tempPath(tempDir, QStringLiteral("include_in_anonymous.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin Outer\n"
                                    "*begin\n"
                                    "*include part.svx\n"
                                    "*end\n"
                                    "*end Outer\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 SurvexFile '' 'include_in_anonymous' '' node 3"),
        QStringLiteral("1 SurvexBlock 'Outer' 'Outer' 'Outer' node 3"),
        QStringLiteral("2 SurvexBlock 'Outer/#1' 'Trip 1' '' unit 3"),
    }));
}

TEST_CASE("Each anonymous Survex trip row carries its own *team names",
          "[Scanner][Survex][AnonymousTrips]")
{
    const ScanResult scan = scanFixture(kAnonymousTripsFixture);

    CHECK(rowAt(scan, QStringLiteral("alpha/#1")).team
          == QStringList({QStringLiteral("Ann Example")}));
    CHECK(rowAt(scan, QStringLiteral("alpha/#2")).team
          == QStringList({QStringLiteral("Bob Sample")}));
    CHECK(rowAt(scan, QStringLiteral("alpha/#3")).team
          == QStringList({QStringLiteral("Cy Placeholder")}));
    // alpha's own scope holds no *team line.
    CHECK(rowAt(scan, QStringLiteral("alpha")).team.isEmpty());
}

TEST_CASE("A *team in a nested anonymous block, before the trip's first shot, belongs to the trip",
          "[Scanner][Survex][AnonymousTrips]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempPath(tempDir, QStringLiteral("nested_team.svx"));
    writeUtf8File(path,
                  QByteArrayLiteral("*begin Outer\n"
                                    "*team \"Dee Invented\" compass\n"
                                    "o0 o1 2.0 0 0\n"
                                    "*begin\n"
                                    "*team Eve tape\n"
                                    "*begin\n"
                                    "*team \"Fay Madeup\" notes\n"
                                    "*flags duplicate\n"
                                    "o1 o2 5.0 0 0\n"
                                    "*end\n"
                                    "o2 o3 5.0 0 0\n"
                                    "*end\n"
                                    "*end Outer\n"));

    auto result = cwExternalCenterlineScanner::scanSurvex(path);
    REQUIRE_FALSE(result.hasError());
    const ScanResult scan = result.value();

    CHECK(rowAt(scan, QStringLiteral("Outer/#1")).team
          == QStringList({QStringLiteral("Eve"), QStringLiteral("Fay Madeup")}));
    // The named block keeps only its own scope's names.
    CHECK(rowAt(scan, QStringLiteral("Outer")).team
          == QStringList({QStringLiteral("Dee Invented")}));
}
