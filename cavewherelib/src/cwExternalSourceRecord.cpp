/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalSourceRecord.h"

//Our includes
#include "cwExternalCenterlineSync.h"
#include "cavewhere.pb.h"

//Qt includes
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace {

constexpr auto kRecordSuffix = QLatin1StringView(".pb");

//! base with exactly one trailing separator, so a relative path appends to it.
QString basePrefix(const QString& base)
{
    return base.endsWith(QLatin1Char('/')) ? base : base + QLatin1Char('/');
}

CavewhereProto::ExternalSourceRecord toProto(const cwExternalSourceRecord::Record& record)
{
    QStringList paths;
    paths.reserve(record.fingerprint.files.size());
    for (const auto& file : record.fingerprint.files) {
        paths.append(file.path);
    }
    // The base is the closure's common ancestor, the directory the attach
    // mirrors from, so every dependency sits below it. Paths on different
    // volumes share no base and stay absolute.
    const QString base = cwExternalCenterlineSync::commonAncestorDir(paths);
    const QString prefix = base.isEmpty() ? QString() : basePrefix(base);

    CavewhereProto::ExternalSourceRecord proto;
    proto.set_breadcrumb_path(record.path.toStdString());
    proto.set_base_dir(base.toStdString());
    proto.mutable_files()->Reserve(static_cast<int>(record.fingerprint.files.size()));
    for (const auto& file : record.fingerprint.files) {
        const bool belowBase = !prefix.isEmpty() && file.path.startsWith(prefix);
        auto* protoFile = proto.add_files();
        protoFile->set_path((belowBase ? file.path.mid(prefix.size()) : file.path).toStdString());
        protoFile->set_size(file.size);
        protoFile->set_last_modified(file.lastModified);
        protoFile->set_sha256(file.sha256.toStdString());
    }
    return proto;
}

cwExternalSourceRecord::Record fromProto(const CavewhereProto::ExternalSourceRecord& proto)
{
    const QString base = QString::fromStdString(proto.base_dir());
    const QString prefix = base.isEmpty() ? QString() : basePrefix(base);

    cwExternalSourceRecord::Record record;
    record.path = QString::fromStdString(proto.breadcrumb_path());
    record.fingerprint.files.reserve(proto.files_size());
    for (const auto& protoFile : proto.files()) {
        const QString path = QString::fromStdString(protoFile.path());
        cwExternalSourceRecord::SourceFileFingerprint file;
        file.path = QDir::isAbsolutePath(path) ? path : prefix + path;
        file.size = protoFile.size();
        file.lastModified = protoFile.last_modified();
        file.sha256 = QString::fromStdString(protoFile.sha256());
        record.fingerprint.files.append(file);
    }
    return record;
}

} // namespace

namespace cwExternalSourceRecord {

SourceFingerprint computeFingerprint(const QStringList& files)
{
    SourceFingerprint fingerprint;
    fingerprint.files.reserve(files.size());
    for (const QString& path : files) {
        SourceFileFingerprint record;
        record.path = path;

        QFile file(path);
        if (file.open(QFile::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            // A read that fails partway leaves the whole record in its
            // unreadable state, so size == -1 stays the single test for
            // "this file could not be read".
            if (hash.addData(&file)) {
                record.sha256 = QString::fromLatin1(hash.result().toHex());

                const QFileInfo info(file);
                record.size = info.size();
                record.lastModified = info.lastModified().toMSecsSinceEpoch();
            }
        }

        fingerprint.files.append(record);
    }
    return fingerprint;
}

QString recordsDirName()
{
    return QStringLiteral("external-sources");
}

// Ids are written "WithoutBraces" (lowercase with hyphens) to match the
// Cave/Trip id serialization in cwSaveLoad.
QString recordFileName(const QUuid& ownerId)
{
    return ownerId.toString(QUuid::WithoutBraces) + kRecordSuffix;
}

Records readRecords(const QDir& recordsDir)
{
    Records records;
    const QFileInfoList files =
        recordsDir.entryInfoList({QStringLiteral("*") + kRecordSuffix}, QDir::Files);
    for (const QFileInfo& info : files) {
        // Only the name a write would produce, so clearBreadcrumb can remove
        // every record this reads.
        const QUuid ownerId = QUuid::fromString(info.completeBaseName());
        if (ownerId.isNull() || recordFileName(ownerId) != info.fileName()) {
            continue;
        }

        QFile file(info.absoluteFilePath());
        if (!file.open(QFile::ReadOnly)) {
            qWarning() << "Could not open external source record" << info.absoluteFilePath();
            continue;
        }
        const QByteArray bytes = file.readAll();
        CavewhereProto::ExternalSourceRecord proto;
        if (!proto.ParseFromArray(bytes.constData(), static_cast<int>(bytes.size()))) {
            qWarning() << "Skipping unreadable external source record" << info.absoluteFilePath();
            continue;
        }
        records.insert(ownerId, fromProto(proto));
    }
    return records;
}

Monad::ResultBase writeRecord(const QString& filePath, const Record& record)
{
    const QDir dir = QFileInfo(filePath).absoluteDir();
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        return Monad::ResultBase(
            QStringLiteral("Couldn't create directory: %1").arg(dir.absolutePath()));
    }

    const std::string bytes = toProto(record).SerializeAsString();
    QSaveFile file(filePath);
    if (!file.open(QFile::WriteOnly)) {
        return Monad::ResultBase(QStringLiteral("Failed to open file for writing: %1").arg(filePath));
    }
    file.write(bytes.data(), static_cast<qint64>(bytes.size()));
    if (!file.commit()) {
        return Monad::ResultBase(QStringLiteral("Failed to write file: %1").arg(filePath));
    }
    return Monad::ResultBase();
}

} // namespace cwExternalSourceRecord
