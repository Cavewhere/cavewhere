/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalSourceSettings.h"

//Our includes
#include "cwSaveLoad.h"

//Qt includes
#include <QDir>
#include <QFileInfo>

cwExternalSourceSettings::cwExternalSourceSettings(QObject* parent)
    : QObject(parent)
{
}

void cwExternalSourceSettings::setSaveLoad(cwSaveLoad* saveLoad)
{
    if (m_saveLoad == saveLoad) {
        return;
    }

    if (!m_saveLoad.isNull()) {
        disconnect(m_saveLoad.data(), &cwSaveLoad::externalSourceRecordsLoaded,
                   this, &cwExternalSourceSettings::installRecords);
    }
    m_saveLoad = saveLoad;
    if (!m_saveLoad.isNull()) {
        connect(m_saveLoad.data(), &cwSaveLoad::externalSourceRecordsLoaded,
                this, &cwExternalSourceSettings::installRecords);
    }

    // A new cwSaveLoad is a new project, which starts with no records.
    installRecords(cwExternalSourceRecord::Records());
}

void cwExternalSourceSettings::installRecords(const cwExternalSourceRecord::Records& records)
{
    if (m_records == records) {
        return;
    }
    m_records = records;
    emit breadcrumbsChanged();
}

QString cwExternalSourceSettings::breadcrumbPath(const QUuid& ownerId) const
{
    const auto it = m_records.constFind(ownerId);
    return it == m_records.cend() ? QString() : it->path;
}

bool cwExternalSourceSettings::hasBreadcrumb(const QUuid& ownerId) const
{
    return m_records.contains(ownerId);
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

QList<cwExternalSourceRecord::Breadcrumb> cwExternalSourceSettings::breadcrumbs() const
{
    QList<cwExternalSourceRecord::Breadcrumb> breadcrumbs;
    breadcrumbs.reserve(m_records.size());
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        breadcrumbs.append({it.key(), it->path});
    }
    return breadcrumbs;
}

cwExternalSourceRecord::SourceFingerprint
cwExternalSourceSettings::fingerprint(const QUuid& ownerId) const
{
    const auto it = m_records.constFind(ownerId);
    return it == m_records.cend() ? cwExternalSourceRecord::SourceFingerprint() : it->fingerprint;
}

void cwExternalSourceSettings::setBreadcrumbPath(const QUuid& ownerId, const QString& path)
{
    setBreadcrumb(ownerId, path, cwExternalSourceRecord::SourceFingerprint());
}

void cwExternalSourceSettings::setBreadcrumb(const QUuid& ownerId,
                                             const QString& path,
                                             const cwExternalSourceRecord::SourceFingerprint& fingerprint)
{
    if (ownerId.isNull()) {
        return;
    }

    const cwExternalSourceRecord::Record record{path, fingerprint};
    const auto it = m_records.constFind(ownerId);
    if (it != m_records.cend() && *it == record) {
        return;
    }

    m_records.insert(ownerId, record);
    writeThrough(ownerId, record);
    emit breadcrumbsChanged();
}

void cwExternalSourceSettings::refreshFingerprintStats(const QUuid& ownerId,
                                                       const cwExternalSourceRecord::SourceFingerprint& fingerprint)
{
    if (ownerId.isNull() || fingerprint.isEmpty()) {
        return;
    }

    // Only an owner that already has a fingerprint has stats to refresh;
    // writing one for an owner without an entry would invent a record of
    // a copy that never happened.
    const auto it = m_records.find(ownerId);
    if (it == m_records.end() || it->fingerprint.isEmpty() || it->fingerprint == fingerprint) {
        return;
    }
    it->fingerprint = fingerprint;
    writeThrough(ownerId, *it);
}

void cwExternalSourceSettings::clearBreadcrumb(const QUuid& ownerId)
{
    clearBreadcrumbs({ownerId});
}

void cwExternalSourceSettings::clearBreadcrumbs(const QList<QUuid>& ownerIds)
{
    bool changed = false;
    for (const QUuid& ownerId : ownerIds) {
        if (m_records.remove(ownerId)) {
            removeThrough(ownerId);
            changed = true;
        }
    }
    if (changed) {
        emit breadcrumbsChanged();
    }
}

void cwExternalSourceSettings::writeThrough(const QUuid& ownerId, const cwExternalSourceRecord::Record& record)
{
    if (!m_saveLoad.isNull()) {
        m_saveLoad->enqueueExternalSourceRecordWrite(ownerId, record);
    }
}

void cwExternalSourceSettings::removeThrough(const QUuid& ownerId)
{
    if (!m_saveLoad.isNull()) {
        m_saveLoad->enqueueExternalSourceRecordRemove(ownerId);
    }
}
