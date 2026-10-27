/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The record store's value types and file functions, called directly
// through the plain cwExternalSourceRecord header (plan SB.4).

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwExternalSourceRecord.h"
#include "cavewhere.pb.h"

// Qt
#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUuid>

namespace {

constexpr qint64 kMissingFileSize = -1;
constexpr qint64 kEntrySize = 42;
constexpr qint64 kEntryLastModified = 1700000000000;
constexpr qint64 kPartSize = 7;
constexpr qint64 kPartLastModified = 1700000001000;

QString canonicalTempPath(const QTemporaryDir& dir)
{
    return QFileInfo(dir.path()).canonicalFilePath();
}

void writeBytes(const QString& path, const QByteArray& bytes)
{
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    REQUIRE(file.open(QFile::WriteOnly | QFile::Truncate));
    REQUIRE(file.write(bytes) == bytes.size());
}

QString sha256Hex(const QByteArray& bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

cwExternalSourceRecord::Record makeRecord(const QString& root)
{
    cwExternalSourceRecord::Record record;
    record.path = root + QStringLiteral("/survey/entry.svx");

    cwExternalSourceRecord::SourceFileFingerprint entry;
    entry.path = root + QStringLiteral("/survey/entry.svx");
    entry.size = kEntrySize;
    entry.lastModified = kEntryLastModified;
    entry.sha256 = QStringLiteral("ab12");

    cwExternalSourceRecord::SourceFileFingerprint included;
    included.path = root + QStringLiteral("/survey/sub/part.svx");
    included.size = kPartSize;
    included.lastModified = kPartLastModified;
    included.sha256 = QStringLiteral("cd34");

    record.fingerprint.files = {entry, included};
    return record;
}

} // namespace

TEST_CASE("cwExternalSourceRecord names the records directory and each owner's file",
          "[ExternalSourceRecord]")
{
    const QUuid owner = QUuid::createUuid();
    CHECK(cwExternalSourceRecord::recordsDirName() == QStringLiteral("external-sources"));
    CHECK(cwExternalSourceRecord::recordFileName(owner)
          == owner.toString(QUuid::WithoutBraces) + QStringLiteral(".pb"));
}

TEST_CASE("cwExternalSourceRecord::writeRecord round-trips through readRecords",
          "[ExternalSourceRecord]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = canonicalTempPath(temp);
    const QDir recordsDir(root + QStringLiteral("/records/")
                          + cwExternalSourceRecord::recordsDirName());

    const QUuid firstOwner = QUuid::createUuid();
    const QUuid secondOwner = QUuid::createUuid();
    const cwExternalSourceRecord::Record firstRecord = makeRecord(root);
    cwExternalSourceRecord::Record secondRecord;
    secondRecord.path = root + QStringLiteral("/other/entry.dat");

    // The directory does not exist yet; writeRecord creates it.
    const QString firstPath =
        recordsDir.filePath(cwExternalSourceRecord::recordFileName(firstOwner));
    const QString secondPath =
        recordsDir.filePath(cwExternalSourceRecord::recordFileName(secondOwner));
    CHECK_FALSE(cwExternalSourceRecord::writeRecord(firstPath, firstRecord).hasError());
    CHECK_FALSE(cwExternalSourceRecord::writeRecord(secondPath, secondRecord).hasError());
    CHECK(QFileInfo::exists(firstPath));
    CHECK(QFileInfo::exists(secondPath));

    const cwExternalSourceRecord::Records records =
        cwExternalSourceRecord::readRecords(recordsDir);
    REQUIRE(records.size() == 2);
    CHECK(records.value(firstOwner) == firstRecord);
    CHECK(records.value(secondOwner) == secondRecord);
}

TEST_CASE("cwExternalSourceRecord::writeRecord stores paths relative to the closure's common directory",
          "[ExternalSourceRecord]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = canonicalTempPath(temp);
    const QString path = root + QStringLiteral("/records/")
                         + cwExternalSourceRecord::recordFileName(QUuid::createUuid());
    REQUIRE_FALSE(cwExternalSourceRecord::writeRecord(path, makeRecord(root)).hasError());

    QFile file(path);
    REQUIRE(file.open(QFile::ReadOnly));
    const QByteArray bytes = file.readAll();
    CavewhereProto::ExternalSourceRecord proto;
    REQUIRE(proto.ParseFromArray(bytes.constData(), static_cast<int>(bytes.size())));
    CHECK(QString::fromStdString(proto.base_dir()) == root + QStringLiteral("/survey"));
    REQUIRE(proto.files_size() == 2);
    CHECK(QString::fromStdString(proto.files(0).path()) == QStringLiteral("entry.svx"));
    CHECK(QString::fromStdString(proto.files(1).path()) == QStringLiteral("sub/part.svx"));
}

TEST_CASE("cwExternalSourceRecord::readRecords skips files that name no owner or do not parse",
          "[ExternalSourceRecord]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = canonicalTempPath(temp);
    const QDir recordsDir(root);

    const QUuid validOwner = QUuid::createUuid();
    cwExternalSourceRecord::Record validRecord;
    validRecord.path = QStringLiteral("/path/valid.svx");
    REQUIRE_FALSE(cwExternalSourceRecord::writeRecord(
                      recordsDir.filePath(validOwner.toString(QUuid::WithoutBraces)
                                          + QStringLiteral(".pb")),
                      validRecord)
                      .hasError());

    // A name that is no owner id, an owner id in a spelling a write never
    // produces, and an owner's file cut short inside a length-delimited field.
    writeBytes(recordsDir.filePath(QStringLiteral("not-a-uuid.pb")), QByteArray("\x0a\x05/path"));
    writeBytes(recordsDir.filePath(QUuid::createUuid().toString(QUuid::WithBraces)
                                   + QStringLiteral(".pb")),
               QByteArray("\x0a\x05/path"));
    writeBytes(recordsDir.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)
                                   + QStringLiteral(".pb")),
               QByteArray("\x0a\xff"));

    const cwExternalSourceRecord::Records records =
        cwExternalSourceRecord::readRecords(recordsDir);
    REQUIRE(records.size() == 1);
    CHECK(records.value(validOwner) == validRecord);
}

TEST_CASE("cwExternalSourceRecord::computeFingerprint records each file in order, unreadable ones as missing",
          "[ExternalSourceRecord]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = canonicalTempPath(temp);
    const QString entryPath = root + QStringLiteral("/entry.svx");
    const QString partPath = root + QStringLiteral("/part.svx");
    const QString missingPath = root + QStringLiteral("/missing.svx");
    const QByteArray entryBytes("*begin entry\n*include part\n*end entry\n");
    const QByteArray partBytes("1 2 10.0 0 0\n");
    writeBytes(entryPath, entryBytes);
    writeBytes(partPath, partBytes);

    const cwExternalSourceRecord::SourceFingerprint fingerprint =
        cwExternalSourceRecord::computeFingerprint({entryPath, missingPath, partPath});
    REQUIRE(fingerprint.files.size() == 3);

    const auto& entry = fingerprint.files.at(0);
    CHECK(entry.path == entryPath);
    CHECK(entry.size == entryBytes.size());
    CHECK(entry.sha256 == sha256Hex(entryBytes));
    CHECK(entry.lastModified == QFileInfo(entryPath).lastModified().toMSecsSinceEpoch());

    const auto& missing = fingerprint.files.at(1);
    CHECK(missing.path == missingPath);
    CHECK(missing.size == kMissingFileSize);
    CHECK(missing.sha256.isEmpty());

    const auto& part = fingerprint.files.at(2);
    CHECK(part.path == partPath);
    CHECK(part.size == partBytes.size());
    CHECK(part.sha256 == sha256Hex(partBytes));
}
