// File: test_cwZip_roundtrip.cpp

#include <catch2/catch_test_macros.hpp>

// Qt
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QByteArray>
#include <QString>
#include <QDebug>

// Under test
#include "cwZip.h"

// -----------------------------
// Helpers (anonymous namespace)
// -----------------------------
namespace {

struct FileEntry {
    QString relativePath;   // always uses forward slashes
    QByteArray bytes;       // empty for directories
    bool isDirectory = false;
};

static QString normalizeRelPath(const QString& relPath) {
    QString p = relPath;
    p.replace('\\', '/');
    while (p.startsWith("./")) {
        p.remove(0, 2);
    }
    return p;
}

static bool writeFile(const QString& absolutePath, const QByteArray& data) {
    QDir().mkpath(QFileInfo(absolutePath).path());
    QFile f(absolutePath);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    const auto written = f.write(data);
    f.close();
    return written == data.size();
}

static QList<FileEntry> snapshotTree(const QString& rootDir) {
    QList<FileEntry> entries;

    // Collect directories first, then files, both with paths relative to root
    // Directories (explicit) – ensures we verify directory structure as well
    QDirIterator dit(rootDir, QDir::Dirs | QDir::NoDotAndDotDot,
                     QDirIterator::Subdirectories);
    while (dit.hasNext()) {
        const QString abs = dit.next();
        const QString rel = QDir(rootDir).relativeFilePath(abs);
        FileEntry e;
        e.relativePath = normalizeRelPath(rel);
        e.isDirectory = true;
        entries.append(e);
    }

    // Files
    QDirIterator fit(rootDir, QDir::Files, QDirIterator::Subdirectories);
    while (fit.hasNext()) {
        const QString abs = fit.next();
        const QString rel = QDir(rootDir).relativeFilePath(abs);
        QFile f(abs);
        QByteArray bytes;
        if (f.open(QIODevice::ReadOnly)) {
            bytes = f.readAll();
            f.close();
        }
        FileEntry e;
        e.relativePath = normalizeRelPath(rel);
        e.bytes = bytes;
        e.isDirectory = false;
        entries.append(e);
    }

    // Sort deterministically: directories before files for identical names, then lexicographically
    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.relativePath == b.relativePath) {
            return a.isDirectory && !b.isDirectory;
        }
        return a.relativePath < b.relativePath;
    });

    return entries;
}

struct CompareResult {
    bool equal = false;
    QString message;
};

static CompareResult compareTrees(const QString& srcRoot, const QString& dstRoot) {
    const QList<FileEntry> src = snapshotTree(srcRoot);
    const QList<FileEntry> dst = snapshotTree(dstRoot);

    if (src.size() != dst.size()) {
        CompareResult r;
        r.equal = false;
        r.message = QString("Different entry counts: src=%1 dst=%2").arg(src.size()).arg(dst.size());
        return r;
    }

    for (int i = 0; i < src.size(); i++) {
        const FileEntry& a = src[i];
        const FileEntry& b = dst[i];

        if (a.relativePath != b.relativePath) {
            CompareResult r;
            r.equal = false;
            r.message = QString("Path mismatch at index %1: '%2' vs '%3'")
                            .arg(i).arg(a.relativePath, b.relativePath);
            return r;
        }

        if (a.isDirectory != b.isDirectory) {
            CompareResult r;
            r.equal = false;
            r.message = QString("Type mismatch for '%1': directory=%2 vs directory=%3")
                            .arg(a.relativePath)
                            .arg(a.isDirectory ? "true" : "false")
                            .arg(b.isDirectory ? "true" : "false");
            return r;
        }

        if (!a.isDirectory) {
            if (a.bytes.size() != b.bytes.size()) {
                CompareResult r;
                r.equal = false;
                r.message = QString("Size mismatch for file '%1': %2 vs %3 bytes")
                                .arg(a.relativePath)
                                .arg(a.bytes.size())
                                .arg(b.bytes.size());
                return r;
            }
            if (a.bytes != b.bytes) {
                CompareResult r;
                r.equal = false;
                r.message = QString("Content mismatch for file '%1'").arg(a.relativePath);
                return r;
            }
        }
    }

    CompareResult ok;
    ok.equal = true;
    ok.message = "Trees match";
    return ok;
}

} // namespace

// -----------------------------
// The Test
// -----------------------------
TEST_CASE("cwZip round-trip: zipDirectory then extractAll reproduces tree", "[cwZip]") {
    // Create a temporary source tree
    QTemporaryDir sourceRoot;
    REQUIRE(sourceRoot.isValid());

    // A separate temporary directory for the output zip (to avoid including the zip inside the source)
    QTemporaryDir zipRoot;
    REQUIRE(zipRoot.isValid());

    // A separate temporary directory to extract into
    QTemporaryDir extractRoot;
    REQUIRE(extractRoot.isValid());

    const QString src = sourceRoot.path();
    const QString zipFilePath = QDir(zipRoot.path()).filePath("archive.zip");
    const QString out = extractRoot.path();

    // -----------------------------
    // Build a nested test tree
    // -----------------------------
    // root files
    REQUIRE(writeFile(QDir(src).filePath("readme.txt"),
                      QByteArray("This is a test.\nLine two.\n")));
    REQUIRE(writeFile(QDir(src).filePath("data.bin"),
                      QByteArray::fromHex("00010203040506070809AABBCCDDEEFF")));

    // nested directories with multiple files
    REQUIRE(writeFile(QDir(src).filePath("alpha/one.txt"),
                      QByteArray("alpha/one\n")));
    REQUIRE(writeFile(QDir(src).filePath("alpha/two.txt"),
                      QByteArray("alpha/two\n")));
    REQUIRE(writeFile(QDir(src).filePath("alpha/deeper/three.txt"),
                      QByteArray("alpha/deeper/three\n")));

    REQUIRE(writeFile(QDir(src).filePath("bravo/x.dat"),
                      QByteArray(1024, '\x7F'))); // 1 KB of 0x7F
    REQUIRE(writeFile(QDir(src).filePath("bravo/charlie/yaml.yml"),
                      QByteArray("name: example\ncount: 42\n")));

    // Optional: a file with non-ASCII to exercise UTF-8 paths
    REQUIRE(writeFile(QDir(src).filePath(QStringLiteral("unicodé/naïve.txt")),
                      QByteArray("accented path\n")));

    // Take a snapshot of the source tree for later comparison
    const auto sourceEntries = snapshotTree(src);
    REQUIRE(sourceEntries.size() > 0); // sanity check

    // -----------------------------
    // Zip it
    // -----------------------------
    {
        const auto zipResult = cwZip::zipDirectory(src, zipFilePath);
        // Assuming Monad::ResultBase has hasError()
        REQUIRE_FALSE(zipResult.hasError());
        REQUIRE(QFileInfo::exists(zipFilePath));
        REQUIRE(QFileInfo(zipFilePath).size() > 0);
    }

    // -----------------------------
    // Extract it
    // -----------------------------
    cwZip::ZipResult stats = {};
    {
        const auto extractResult = cwZip::extractAll(zipFilePath, out);
        REQUIRE_FALSE(extractResult.hasError());
        stats = extractResult.value();
        // Basic sanity: we created 7 files above (plus the UTF-8 one) = 8 files, 5 directories (alpha, alpha/deeper, bravo, bravo/charlie, unicodé)
        // We do not assert exact counts too strictly because different zip engines may record directory entries differently.
        REQUIRE(stats.filesExtracted >= 8);
        REQUIRE(stats.hadErrors == false);
    }

    // -----------------------------
    // Compare the trees
    // -----------------------------
    const auto comparison = compareTrees(src, out);
    INFO(comparison.message.toStdString());
    REQUIRE(comparison.equal);
}
