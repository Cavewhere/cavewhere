#pragma once

#include "CaveWhereLibExport.h"

#include <QObject>
#include <QString>

/**
 * @brief Abstract interface for remote authentication providers.
 *
 * Decouples cwProject and cwSaveLoad from any concrete auth implementation
 * (e.g. GitHub OAuth) so other providers can be added in the future.
 */
class CAVEWHERE_LIB_EXPORT cwRemoteAuthProvider : public QObject
{
    Q_OBJECT

public:
    explicit cwRemoteAuthProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~cwRemoteAuthProvider() override = default;

    /**
     * Returns true once credentials have been loaded from persistent storage
     * (e.g. the system keychain). False means a load is still pending or
     * has not yet been triggered.
     */
    virtual bool hasLoadedCredentials() const = 0;

    /**
     * Triggers an asynchronous credential load if one has not already
     * started or completed. Emits credentialsLoaded() when done.
     */
    virtual void ensureCredentialsLoaded() = 0;

    /**
     * Returns the current access token (may be empty if not yet loaded
     * or if the user is not authenticated).
     */
    virtual QString accessToken() const = 0;

    /**
     * Returns true if this provider can verify that the remote application
     * is installed for the active account before a sync runs. Providers that
     * return true must override verifyInstallation().
     */
    virtual bool supportsInstallationCheck() const { return false; }

    /**
     * Starts an asynchronous check that the remote application is installed.
     * Concrete providers must emit installationVerified(bool) exactly once
     * per call (true when installed, false otherwise). Default is a no-op.
     */
    virtual void verifyInstallation() {}

signals:
    /**
     * Emitted once after ensureCredentialsLoaded() completes, regardless
     * of whether a valid token was found.
     */
    void credentialsLoaded();

    /**
     * Emitted whenever the access token value changes (login, logout,
     * token refresh, etc.).
     */
    void accessTokenChanged();

    /**
     * Emitted after verifyInstallation() completes. `installed` is true
     * when the remote application is installed on at least one account.
     */
    void installationVerified(bool installed);
};
