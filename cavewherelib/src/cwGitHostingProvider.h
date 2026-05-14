/**************************************************************************
**
**    Copyright (C) 2026 by CaveWhere
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGITHOSTINGPROVIDER_H
#define CWGITHOSTINGPROVIDER_H

//Qt includes
#include <QString>
#include <QStringList>
#include <QUrl>

//Our includes
#include "CaveWhereLibExport.h"

/**
 * @brief Per-host strings and URL templates for a Git hosting service.
 *
 * Empty string fields denote the generic fallback entry: no canonical
 * host, no display name, and no collaborator settings URL.
 */
struct cwGitHostingProviderInfo {
    QString host;                   // canonical lowercase, e.g. "github.com" (empty for generic fallback)
    QString displayName;            // "GitHub" (empty for generic)
    QString collaboratorPathSuffix; // "/settings/access" / "/-/project_members" / etc. (empty for generic)
    QString openLinkLabel;          // "Open repository on GitHub" / etc.
    QString authMessage;            // shown when an auth error happens during clone
    QString notFoundMessage;        // shown when a 404 happens during clone (no <a>; link added by helper)
};

namespace cwGitHostingProvider {
    /**
     * @brief Lookup by canonical host. Case-insensitive.
     *
     * Returns a reference to the matching entry, or to the generic
     * fallback entry if no host matches. Never returns a dangling reference.
     */
    CAVEWHERE_LIB_EXPORT const cwGitHostingProviderInfo& forHost(const QString& host);
    CAVEWHERE_LIB_EXPORT const cwGitHostingProviderInfo& forUrl(const QUrl& cloneUrl);

    /// Host strings of non-generic entries (used by the deep-link allowlist).
    CAVEWHERE_LIB_EXPORT QStringList knownHosts();

    /**
     * @brief Derived https web URL for the cloned repository.
     *
     * Works for both https:// and ssh:// clone URLs. Returns an empty
     * QUrl on failure, for the generic provider when the input is not
     * https, or if @p info.host does not match @p cloneUrl host (defensive
     * — prevents silently rewriting one host to another).
     */
    CAVEWHERE_LIB_EXPORT QUrl repositoryWebUrl(const cwGitHostingProviderInfo& info,
                                               const QUrl& cloneUrl);

    /**
     * @brief https URL for the collaborator/access settings page of @p repoUrl.
     *
     * Empty for the generic provider or when @p repoUrl is invalid.
     */
    CAVEWHERE_LIB_EXPORT QUrl collaboratorSettingsUrl(const cwGitHostingProviderInfo& info,
                                                      const QUrl& repoUrl);

    /**
     * @brief Friendly RichText for a 404 "doesn't exist / no access" failure.
     *
     * Returns @p info.notFoundMessage with an embedded `<a href="...">` link
     * suffix when repositoryWebUrl can be derived; otherwise the plain
     * notFoundMessage.
     */
    CAVEWHERE_LIB_EXPORT QString notFoundOrAccessMessage(const cwGitHostingProviderInfo& info,
                                                         const QUrl& cloneUrl);
}

#endif // CWGITHOSTINGPROVIDER_H
