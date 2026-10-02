/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwExternalCenterline.h"
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
#include <QHash>
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

TEST_CASE("formatName maps formats to display names, through the gadget too",
          "[Scanner]")
{
    using namespace cwExternalCenterlineScanner;
    CHECK(formatName(Format::Survex) == QStringLiteral("Survex"));
    CHECK(formatName(Format::Compass) == QStringLiteral("Compass"));
    CHECK(formatName(Format::Walls) == QStringLiteral("Walls"));
    CHECK(formatName(Format::Unknown).isEmpty());

    // cwExternalCenterline::format is the QML-facing wrapper the
    // attached header binds to.
    CHECK(cwExternalCenterline(QStringLiteral("cave.svx")).format()
          == QStringLiteral("Survex"));
    CHECK(cwExternalCenterline(QStringLiteral("cave.MAK")).format()
          == QStringLiteral("Compass"));
    CHECK(cwExternalCenterline(QStringLiteral("cave.wpj")).format()
          == QStringLiteral("Walls"));
    CHECK(cwExternalCenterline(QStringLiteral("cave.txt")).format().isEmpty());
    CHECK(cwExternalCenterline().format().isEmpty());
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

TEST_CASE("a *date inside an anonymous *begin stamps the enclosing block",
          "[Scanner][Blocks]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // An anonymous *begin makes no block of its own, so its date belongs
    // to the named block around it - the same fold recordStation makes.
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

TEST_CASE("an anonymous *begin folds its shots into the parent block",
          "[Scanner][Blocks]")
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

namespace {

using cwExternalCenterlineScanner::ScannedFix;

QList<ScannedFix> scannedFixes(const QString& entryFile)
{
    auto result = cwExternalCenterlineScanner::scan(entryFile);
    INFO("scan: " << result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    return result.value().fixes;
}

} // namespace

TEST_CASE("scanSurvex records each *fix's coordinate and the input *cs in force", "[Scanner][Attach]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    SECTION("a bare *fix has none")
    {
        const QList<ScannedFix> fixes =
            scannedFixes(datasetExternalCenterlinePath(QStringLiteral("survex_blocks.svx")));
        CHECK(fixes == QList<ScannedFix>{{QStringLiteral("d1"), QStringLiteral("0 0 0"), QString()}});
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
                                                      {QStringLiteral("a"), QStringLiteral("1 2 3"), utm16},
                                                      {QStringLiteral("b"), QStringLiteral("1 2 3"), utm16},
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
              == QList<ScannedFix>{{QStringLiteral("A1"), QStringLiteral("m,0,0,0"), QString()},
                                   {QStringLiteral("B1"), QStringLiteral("M,580661.57,4113846.34,219"),
                                    QStringLiteral("North American 1983, UTM zone 16N")}});
    }

    SECTION("a base location's zone stands in for a missing zone line")
    {
        const QString makPath = writeUtf8File(
            tempPath(tempDir, QStringLiteral("base.mak")),
            QByteArrayLiteral("@580661.57,4113846.34,219,16,0.549;\n"
                              "&North American 1983;\n"
                              "#cave.dat,A1[m,580661.57,4113846.34,219];\n"));
        CHECK(scannedFixes(makPath)
              == QList<ScannedFix>{{QStringLiteral("A1"), QStringLiteral("m,580661.57,4113846.34,219"),
                                    QStringLiteral("North American 1983, UTM zone 16N")}});
    }

    SECTION("a lone .dat fixes nothing")
    {
        CHECK(scannedFixes(tempPath(tempDir, QStringLiteral("cave.dat"))).isEmpty());
    }
}

TEST_CASE("scanWalls records #FIX lines and the system their .wpj's .REF names",
          "[Scanner][Attach]")
{
    const QList<ScannedFix> georeferenced =
        scannedFixes(testcasesDatasetSourcePath(QStringLiteral("walls/georef_cave.wpj")));
    const QString nad83Utm16 = QStringLiteral("NAD83, UTM zone 16N");
    const QString a1 = QStringLiteral("580661.570 4113846.340 219");
    const QString a3 = QStringLiteral("W86:5:29.254 N37:10:3.195 219");
    CHECK(georeferenced == QList<ScannedFix>{{QStringLiteral("A1"), a1, nad83Utm16},
                                             {QStringLiteral("A3"), a3, nad83Utm16}});

    const QList<ScannedFix> alone =
        scannedFixes(testcasesDatasetSourcePath(QStringLiteral("walls/GEOREF.SRV")));
    CHECK(alone == QList<ScannedFix>{{QStringLiteral("A1"), a1, QString()},
                                     {QStringLiteral("A3"), a3, QString()}});
}
