/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalSourceSettings.h"

//Qt includes
#include <QSettings>

cwExternalSourceSettings::cwExternalSourceSettings(QObject* parent)
    : QObject(parent)
{
}

// QUuid keys are stored "WithoutBraces" (lowercase with hyphens) to
// match the Cave/Trip id serialization in cwSaveLoad. QUuid::fromString
// accepts both with-hyphens and no-hyphens, so lookups never trip over
// formatting drift.
QString cwExternalSourceSettings::sourceKey(const QUuid& ownerId)
{
    return QStringLiteral("externalCenterlineSources/")
        + ownerId.toString(QUuid::WithoutBraces);
}

QString cwExternalSourceSettings::sourcePathFor(const QUuid& ownerId) const
{
    if (ownerId.isNull()) {
        return QString();
    }
    QSettings settings;
    return settings.value(sourceKey(ownerId)).toString();
}

bool cwExternalSourceSettings::hasSource(const QUuid& ownerId) const
{
    if (ownerId.isNull()) {
        return false;
    }
    QSettings settings;
    return settings.contains(sourceKey(ownerId));
}

bool cwExternalSourceSettings::isLiveLink(const QUuid& ownerId) const
{
    return !sourcePathFor(ownerId).isEmpty();
}

QList<cwExternalSourceSettings::ExternalCenterlineSource> cwExternalSourceSettings::externalCenterlineSources() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("externalCenterlineSources"));
    const QStringList keys = settings.childKeys();

    QList<ExternalCenterlineSource> sources;
    sources.reserve(keys.size());
    for (const QString& key : keys) {
        // Skip keys that aren't owner UUIDs - they're junk that would
        // also confuse the sourcePathFor lookup.
        const QUuid ownerId = QUuid::fromString(key);
        if (ownerId.isNull()) {
            continue;
        }
        sources.append({ownerId, settings.value(key).toString()});
    }
    return sources;
}

void cwExternalSourceSettings::setSourcePath(const QUuid& ownerId, const QString& sourcePath)
{
    if (ownerId.isNull()) {
        return;
    }
    QSettings settings;
    const QString key = sourceKey(ownerId);
    if (settings.contains(key) && settings.value(key).toString() == sourcePath) {
        return;
    }
    settings.setValue(key, sourcePath);
    emit externalCenterlineSourcesChanged();
}

void cwExternalSourceSettings::clearSourcePath(const QUuid& ownerId)
{
    if (ownerId.isNull()) {
        return;
    }
    QSettings settings;
    const QString key = sourceKey(ownerId);
    if (!settings.contains(key)) {
        return;
    }
    settings.remove(key);
    emit externalCenterlineSourcesChanged();
}
