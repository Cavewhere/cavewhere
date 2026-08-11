/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalSourceSettings.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QSettings>

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

void cwExternalSourceSettings::setBreadcrumbPath(const QUuid& ownerId, const QString& path)
{
    if (ownerId.isNull()) {
        return;
    }
    QSettings settings;
    const QString key = breadcrumbKey(ownerId);
    if (settings.contains(key) && settings.value(key).toString() == path) {
        return;
    }
    settings.setValue(key, path);
    emit breadcrumbsChanged();
}

void cwExternalSourceSettings::clearBreadcrumb(const QUuid& ownerId)
{
    if (ownerId.isNull()) {
        return;
    }
    QSettings settings;
    const QString key = breadcrumbKey(ownerId);
    if (!settings.contains(key)) {
        return;
    }
    settings.remove(key);
    emit breadcrumbsChanged();
}
