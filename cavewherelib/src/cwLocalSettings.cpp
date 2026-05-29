/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLocalSettings.h"

//Our includes
#include "cavewhere.pb.h"

//Protobuf includes
#include <google/protobuf/util/json_util.h>

//Qt includes
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace {

constexpr const char* kLoadErrorPrefix = "cwLocalSettings::load: ";
constexpr const char* kSaveErrorPrefix = "cwLocalSettings::save: ";

// QUuid is stored "WithoutBraces" (lowercase with hyphens) to match
// the existing Cave/Trip id serialization in cwSaveLoad. QUuid::fromString
// accepts both with-hyphens and no-hyphens, so loaders never trip over
// formatting drift.
QString uuidToLocalSettingsString(const QUuid& uuid)
{
    return uuid.toString(QUuid::WithoutBraces);
}

QUuid uuidFromLocalSettingsString(const std::string& value)
{
    return QUuid::fromString(QString::fromStdString(value));
}

} // namespace

QString cwLocalSettings::sourcePathFor(const QUuid& ownerId) const
{
    if (ownerId.isNull()) {
        return QString();
    }
    for (const auto& source : m_externalCenterlineSources) {
        if (source.ownerId == ownerId) {
            return source.sourcePath;
        }
    }
    return QString();
}

bool cwLocalSettings::hasSource(const QUuid& ownerId) const
{
    if (ownerId.isNull()) {
        return false;
    }
    for (const auto& source : m_externalCenterlineSources) {
        if (source.ownerId == ownerId) {
            return true;
        }
    }
    return false;
}

void cwLocalSettings::setSourcePath(const QUuid& ownerId, const QString& sourcePath)
{
    if (ownerId.isNull()) {
        return;
    }
    for (auto& source : m_externalCenterlineSources) {
        if (source.ownerId == ownerId) {
            source.sourcePath = sourcePath;
            return;
        }
    }
    m_externalCenterlineSources.append({ownerId, sourcePath});
}

void cwLocalSettings::clearSourcePath(const QUuid& ownerId)
{
    if (ownerId.isNull()) {
        return;
    }
    m_externalCenterlineSources.removeIf([&ownerId](const ExternalCenterlineSource& source) {
        return source.ownerId == ownerId;
    });
}

Monad::Result<cwLocalSettings> cwLocalSettings::load(const QString& filePath)
{
    if (filePath.isEmpty()) {
        return Monad::Result<cwLocalSettings>(
            QStringLiteral("%1empty filePath").arg(QLatin1String(kLoadErrorPrefix)));
    }

    if (!QFileInfo::exists(filePath)) {
        // Missing file == empty settings. Every owner falls back to
        // import mode (per master plan section 5.3).
        return Monad::Result<cwLocalSettings>(cwLocalSettings{});
    }

    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return Monad::Result<cwLocalSettings>(
            QStringLiteral("%1cannot open %2: %3")
                .arg(QLatin1String(kLoadErrorPrefix), filePath, file.errorString()));
    }
    const QByteArray contents = file.readAll();
    file.close();

    CavewhereProto::LocalSettings proto;
    const auto status = google::protobuf::util::JsonStringToMessage(
        std::string(contents.constData(), static_cast<size_t>(contents.size())),
        &proto);
    if (!status.ok()) {
        return Monad::Result<cwLocalSettings>(
            QStringLiteral("%1parse error in %2: %3")
                .arg(QLatin1String(kLoadErrorPrefix),
                     filePath,
                     QString::fromUtf8(status.ToString().c_str())));
    }

    cwLocalSettings settings;
    for (const auto& protoSource : proto.external_centerline_sources()) {
        ExternalCenterlineSource source;
        source.ownerId = uuidFromLocalSettingsString(protoSource.owner_id());
        source.sourcePath = QString::fromStdString(protoSource.source_path());
        // Skip rows with unparseable owner_id - they're junk that
        // would also confuse the sourcePathFor lookup.
        if (source.ownerId.isNull()) {
            continue;
        }
        settings.m_externalCenterlineSources.append(source);
    }
    return Monad::Result<cwLocalSettings>(settings);
}

Monad::ResultBase cwLocalSettings::save(const QString& filePath) const
{
    if (filePath.isEmpty()) {
        return Monad::ResultBase(
            QStringLiteral("%1empty filePath").arg(kSaveErrorPrefix));
    }

    const QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        return Monad::ResultBase(
            QStringLiteral("%1cannot create parent directory: %2")
                .arg(QLatin1String(kSaveErrorPrefix), info.absolutePath()));
    }

    CavewhereProto::LocalSettings proto;
    for (const auto& source : m_externalCenterlineSources) {
        if (source.ownerId.isNull()) {
            continue;
        }
        auto* protoSource = proto.add_external_centerline_sources();
        *(protoSource->mutable_owner_id()) =
            uuidToLocalSettingsString(source.ownerId).toStdString();
        if (!source.sourcePath.isEmpty()) {
            *(protoSource->mutable_source_path()) = source.sourcePath.toStdString();
        }
    }

    std::string json;
    google::protobuf::util::JsonPrintOptions printOptions;
    printOptions.add_whitespace = true;
    // Keep the JSON hand-editable - emit defaulted fields (empty
    // sourcePath, etc.) so users can see the schema without guessing.
    printOptions.always_print_fields_with_no_presence = true;
    const auto status =
        google::protobuf::util::MessageToJsonString(proto, &json, printOptions);
    if (!status.ok()) {
        return Monad::ResultBase(
            QStringLiteral("%1serialize error: %2")
                .arg(QLatin1String(kSaveErrorPrefix),
                     QString::fromUtf8(status.ToString().c_str())));
    }

    QSaveFile file(filePath);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        return Monad::ResultBase(
            QStringLiteral("%1cannot open %2 for write: %3")
                .arg(QLatin1String(kSaveErrorPrefix), filePath, file.errorString()));
    }
    if (file.write(json.data(), static_cast<qint64>(json.size()))
        != static_cast<qint64>(json.size())) {
        return Monad::ResultBase(
            QStringLiteral("%1short write to %2").arg(QLatin1String(kSaveErrorPrefix), filePath));
    }
    if (!file.commit()) {
        return Monad::ResultBase(
            QStringLiteral("%1commit failed on %2: %3")
                .arg(QLatin1String(kSaveErrorPrefix), filePath, file.errorString()));
    }
    return Monad::ResultBase();
}
