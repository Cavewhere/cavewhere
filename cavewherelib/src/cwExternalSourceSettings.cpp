/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalSourceSettings.h"

//Qt includes
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLatin1StringView>
#include <QSettings>

namespace {

constexpr auto kFingerprintPathKey = QLatin1StringView("path");
constexpr auto kFingerprintSizeKey = QLatin1StringView("size");
constexpr auto kFingerprintModifiedKey = QLatin1StringView("modified");
constexpr auto kFingerprintSha256Key = QLatin1StringView("sha256");

constexpr qint64 kUnknownFileStat = -1;

} // namespace

cwExternalSourceSettings::cwExternalSourceSettings(QObject* parent)
    : QObject(parent)
{
}

// The "externalCenterlineSources" group name predates the demotion to
// breadcrumbs and stays as it is, so settings files written by older
// versions keep resolving. QUuid keys are stored "WithoutBraces"
// (lowercase with hyphens) to match the Cave/Trip id serialization in
// cwSaveLoad. QUuid::fromString accepts both with-hyphens and
// no-hyphens, so lookups never trip over formatting drift.
QString cwExternalSourceSettings::breadcrumbKey(const QUuid& ownerId)
{
    return QStringLiteral("externalCenterlineSources/")
        + ownerId.toString(QUuid::WithoutBraces);
}

// Fingerprints live in a subgroup of the same group, one QSettings
// array per owner, so the INI reads as a section per owner with one
// numbered record per source file. childKeys() skips subgroups, so the
// breadcrumb listing above is untouched by them, and an entry written
// before fingerprints existed simply has no such section.
QString cwExternalSourceSettings::fingerprintKey(const QUuid& ownerId)
{
    return QStringLiteral("externalCenterlineSources/fingerprints/")
        + ownerId.toString(QUuid::WithoutBraces);
}

QString cwExternalSourceSettings::breadcrumbPath(const QUuid& ownerId) const
{
    if (ownerId.isNull()) {
        return QString();
    }
    QSettings settings;
    return settings.value(breadcrumbKey(ownerId)).toString();
}

bool cwExternalSourceSettings::hasBreadcrumb(const QUuid& ownerId) const
{
    if (ownerId.isNull()) {
        return false;
    }
    QSettings settings;
    return settings.contains(breadcrumbKey(ownerId));
}

QUrl cwExternalSourceSettings::breadcrumbFolder(const QUuid& ownerId) const
{
    const QString path = breadcrumbPath(ownerId);
    if (path.isEmpty()) {
        return QUrl();
    }
    const QDir folder = QFileInfo(path).absoluteDir();
    if (!folder.exists()) {
        return QUrl();
    }
    return QUrl::fromLocalFile(folder.absolutePath());
}

QList<cwExternalSourceSettings::Breadcrumb> cwExternalSourceSettings::breadcrumbs() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("externalCenterlineSources"));
    const QStringList keys = settings.childKeys();

    QList<Breadcrumb> breadcrumbs;
    breadcrumbs.reserve(keys.size());
    for (const QString& key : keys) {
        // Skip keys that aren't owner UUIDs - they're junk that would
        // also confuse the breadcrumbPath lookup.
        const QUuid ownerId = QUuid::fromString(key);
        if (ownerId.isNull()) {
            continue;
        }
        breadcrumbs.append({ownerId, settings.value(key).toString()});
    }
    return breadcrumbs;
}

cwExternalSourceSettings::SourceFingerprint
cwExternalSourceSettings::readFingerprint(QSettings& settings, const QUuid& ownerId)
{
    const int count = settings.beginReadArray(fingerprintKey(ownerId));

    SourceFingerprint fingerprint;
    fingerprint.files.reserve(count);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        SourceFileFingerprint file;
        file.path = settings.value(kFingerprintPathKey).toString();
        file.size = settings.value(kFingerprintSizeKey, kUnknownFileStat).toLongLong();
        file.lastModified =
            settings.value(kFingerprintModifiedKey, kUnknownFileStat).toLongLong();
        file.sha256 = settings.value(kFingerprintSha256Key).toString();
        fingerprint.files.append(file);
    }
    settings.endArray();
    return fingerprint;
}

// A rewrite of the array has to start from an empty one: writing fewer
// records than the last stamp would otherwise leave the tail of the old
// fingerprint behind.
void cwExternalSourceSettings::writeFingerprint(QSettings& settings,
                                                const QUuid& ownerId,
                                                const SourceFingerprint& fingerprint)
{
    const QString arrayKey = fingerprintKey(ownerId);
    settings.remove(arrayKey);
    if (fingerprint.isEmpty()) {
        return;
    }

    settings.beginWriteArray(arrayKey, fingerprint.files.size());
    for (int i = 0; i < fingerprint.files.size(); ++i) {
        const SourceFileFingerprint& file = fingerprint.files.at(i);
        settings.setArrayIndex(i);
        settings.setValue(kFingerprintPathKey, file.path);
        settings.setValue(kFingerprintSizeKey, file.size);
        settings.setValue(kFingerprintModifiedKey, file.lastModified);
        settings.setValue(kFingerprintSha256Key, file.sha256);
    }
    settings.endArray();
}

cwExternalSourceSettings::SourceFingerprint
cwExternalSourceSettings::fingerprint(const QUuid& ownerId) const
{
    if (ownerId.isNull()) {
        return SourceFingerprint();
    }

    QSettings settings;
    return readFingerprint(settings, ownerId);
}

cwExternalSourceSettings::SourceFingerprint
cwExternalSourceSettings::computeFingerprint(const QStringList& files)
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

void cwExternalSourceSettings::setBreadcrumbPath(const QUuid& ownerId, const QString& path)
{
    setBreadcrumb(ownerId, path, SourceFingerprint());
}

void cwExternalSourceSettings::setBreadcrumb(const QUuid& ownerId,
                                             const QString& path,
                                             const SourceFingerprint& fingerprint)
{
    if (ownerId.isNull()) {
        return;
    }

    QSettings settings;
    const QString key = breadcrumbKey(ownerId);
    if (settings.contains(key) && settings.value(key).toString() == path
        && readFingerprint(settings, ownerId) == fingerprint) {
        return;
    }

    settings.setValue(key, path);
    writeFingerprint(settings, ownerId, fingerprint);

    emit breadcrumbsChanged();
}

void cwExternalSourceSettings::refreshFingerprintStats(const QUuid& ownerId,
                                                       const SourceFingerprint& fingerprint)
{
    if (ownerId.isNull() || fingerprint.isEmpty()) {
        return;
    }

    QSettings settings;
    // Only an owner that already has a fingerprint has stats to refresh;
    // writing one for an owner without an entry would invent a record of
    // a copy that never happened.
    if (readFingerprint(settings, ownerId).isEmpty()) {
        return;
    }
    writeFingerprint(settings, ownerId, fingerprint);
}

void cwExternalSourceSettings::clearBreadcrumb(const QUuid& ownerId)
{
    if (ownerId.isNull()) {
        return;
    }
    QSettings settings;
    const QString key = breadcrumbKey(ownerId);
    if (!settings.contains(key) && readFingerprint(settings, ownerId).isEmpty()) {
        return;
    }
    settings.remove(key);
    settings.remove(fingerprintKey(ownerId));
    emit breadcrumbsChanged();
}
