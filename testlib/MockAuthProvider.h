#ifndef CAVEWHERE_TESTLIB_MOCKAUTHPROVIDER_H
#define CAVEWHERE_TESTLIB_MOCKAUTHPROVIDER_H

#include "CaveWhereTestLibExport.h"
#include "cwRemoteAuthProvider.h"

#include <QObject>
#include <QString>

/**
 * Controllable concrete cwRemoteAuthProvider for tests.
 *
 * Covers both flows:
 *   - credentials-loaded gate: public m_hasLoaded / m_token fields plus
 *     completeLoad() and setToken() helpers
 *   - install-check gate: supportsInstallationCheck() / verifyInstallation()
 *     overrides driven by setInstalled() / setSupportsInstallationCheck();
 *     verifyInstallation() emits installationVerified() asynchronously via
 *     QMetaObject::invokeMethod(..., Qt::QueuedConnection) to mirror a real
 *     network-backed provider, so cwProject::sync()'s SingleShotConnection
 *     observes the signal through the event loop.
 */
class CAVEWHERE_TESTLIB_EXPORT MockAuthProvider : public cwRemoteAuthProvider
{
    Q_OBJECT
public:
    explicit MockAuthProvider(QObject* parent = nullptr);

    // cwRemoteAuthProvider
    bool hasLoadedCredentials() const override { return m_hasLoaded; }
    QString accessToken() const override { return m_token; }
    void ensureCredentialsLoaded() override {}
    bool supportsInstallationCheck() const override { return m_supportsInstallationCheck; }
    void verifyInstallation() override;

    // Credentials flow
    void completeLoad(const QString& token = QString());
    void setToken(const QString& token);

    // Install-check flow
    void setSupportsInstallationCheck(bool v) { m_supportsInstallationCheck = v; }
    void setInstalled(bool v) { m_installed = v; }
    void setCredentialsLoaded(bool v) { m_hasLoaded = v; }
    int verifyCalls() const { return m_verifyCalls; }

    // Public for direct assignment in existing tests (preserve legacy API).
    bool m_hasLoaded = false;
    QString m_token;

private:
    bool m_supportsInstallationCheck = true;
    bool m_installed = false;
    int m_verifyCalls = 0;
};

#endif
