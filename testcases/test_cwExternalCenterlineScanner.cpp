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
