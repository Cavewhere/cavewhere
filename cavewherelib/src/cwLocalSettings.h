/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLOCALSETTINGS_H
#define CWLOCALSETTINGS_H

//Our includes
#include "cwGlobals.h"
#include <Monad/Result.h>

//Qt includes
#include <QList>
#include <QString>
#include <QUuid>

/**
 * In-memory mirror of the .cwproj/local_settings.json file
 * (gitignored, per-user, per-machine). v1 carries only external-
 * centerline source paths for live-link attachments; future per-
 * machine state (recent files, cached credentials pointers, last-
 * opened sub-page, ...) lands here as new fields rather than
 * spinning up another sibling JSON file.
 *
 * Persistence is round-tripped through the LocalSettings protobuf
 * message via google::protobuf::util::MessageToJsonString /
 * JsonStringToMessage, so we get the same versioning and forward-
 * compat story as the rest of the project format.
 */
class CAVEWHERE_LIB_EXPORT cwLocalSettings
{
public:
    /**
     * Per-machine source path for a single Attached / Scope
     * external-centerline. ownerId is the cwCave or cwTrip QUuid
     * (matches cwCave::id() / cwTrip::id()). Empty sourcePath means
     * "import mode" - the user attached without live-link.
     */
    struct ExternalCenterlineSource {
        QUuid ownerId;
        QString sourcePath;

        bool operator==(const ExternalCenterlineSource& other) const = default;
        bool operator!=(const ExternalCenterlineSource& other) const = default;
    };

    cwLocalSettings() = default;

    /**
     * Returns the source path stored for ownerId, or an empty string
     * when no entry exists (import mode). The path is returned as-is
     * even when it no longer exists on disk; surfacing the missing-
     * source banner is the consumer's responsibility.
     */
    QString sourcePathFor(const QUuid& ownerId) const;

    /** True if any entry (with or without sourcePath) is recorded for ownerId. */
    bool hasSource(const QUuid& ownerId) const;

    /** All ExternalCenterlineSource entries (live-link + import-mode). */
    QList<ExternalCenterlineSource> externalCenterlineSources() const
    {
        return m_externalCenterlineSources;
    }

    /**
     * Upserts the ExternalCenterlineSource for ownerId. Setting an
     * empty sourcePath records "import mode" - the entry still exists
     * but has no path. To remove the entry entirely use
     * clearSourcePath.
     */
    void setSourcePath(const QUuid& ownerId, const QString& sourcePath);

    /** Removes any ExternalCenterlineSource entry for ownerId. */
    void clearSourcePath(const QUuid& ownerId);

    bool isEmpty() const { return m_externalCenterlineSources.isEmpty(); }

    bool operator==(const cwLocalSettings& other) const
    {
        return m_externalCenterlineSources == other.m_externalCenterlineSources;
    }
    bool operator!=(const cwLocalSettings& other) const { return !(*this == other); }

    /**
     * Loads from JSON at filePath. Returns an empty cwLocalSettings
     * (no entries) when the file does not exist - per the spec,
     * missing local_settings.json puts every owner into import mode.
     * Returns an error when the file exists but cannot be read or
     * parsed.
     */
    static Monad::Result<cwLocalSettings> load(const QString& filePath);

    /**
     * Writes to JSON at filePath. Creates intermediate directories
     * as needed. The output is hand-editable (camelCase field names,
     * no required fields the user cannot see).
     */
    Monad::ResultBase save(const QString& filePath) const;

private:
    QList<ExternalCenterlineSource> m_externalCenterlineSources;
};

#endif // CWLOCALSETTINGS_H
