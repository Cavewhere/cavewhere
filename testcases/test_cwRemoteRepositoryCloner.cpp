//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRemoteRepositoryCloner.h"
#include "cwRecentProjectModel.h"

//QQuickGit includes
#include "Account.h"
#include "GitFutureWatcher.h"
#include "GitRepository.h"

//Qt includes
#include <QEventLoop>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

TEST_CASE("cwRemoteRepositoryCloner clones a remote repository", "[cwRemoteRepositoryCloner]") {
    QSettings settings;
    settings.clear();

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwRecentProjectModel recentProjectModel;
    recentProjectModel.setDefaultRepositoryDir(QUrl::fromLocalFile(tempDir.path()));

    QQuickGit::GitFutureWatcher watcher;
    QQuickGit::Account account;

    cwRemoteRepositoryCloner cloner;
    cloner.setRecentProjectModel(&recentProjectModel);
    cloner.setCloneWatcher(&watcher);
    cloner.setAccount(&account);

    const QString cloneUrl = QStringLiteral("git@github.com:Cavewhere/PhakeCave3000.git");
    cloner.clone(cloneUrl);

    QSignalSpy stateSpy(&watcher, &QQuickGit::GitFutureWatcher::stateChanged);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&watcher, &QQuickGit::GitFutureWatcher::stateChanged, &loop, [&watcher, &loop]() {
        if (watcher.state() == QQuickGit::GitFutureWatcher::Ready) {
            loop.quit();
        }
    });
    timeout.start(180000);
    loop.exec();

    INFO(cloner.cloneErrorMessage().toStdString());
    CHECK(watcher.state() == QQuickGit::GitFutureWatcher::Ready);
    CHECK(stateSpy.count() > 0);
    CHECK_FALSE(watcher.hasError());
    CHECK(cloner.cloneErrorMessage().isEmpty());
    CHECK(recentProjectModel.rowCount() == 1);
    auto projectFile = recentProjectModel.repositoryProjectFile(0);
    INFO(projectFile.errorMessage().toStdString());
    CHECK_FALSE(projectFile.hasError());
    CHECK(QFileInfo::exists(projectFile.value()));
}

TEST_CASE("cwRemoteRepositoryCloner::classifyCloneError maps libgit2 errors to kinds",
          "[cwRemoteRepositoryCloner]")
{
    using Kind = cwRemoteRepositoryCloner::CloneErrorKind;
    constexpr int kHttpAuthFailedCode =
        static_cast<int>(QQuickGit::GitRepository::GitErrorCode::HttpAuthFailed);

    SECTION("HttpAuthFailed error code -> Auth (regardless of message)") {
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  kHttpAuthFailedCode, QStringLiteral("anything")) == Kind::Auth);
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  kHttpAuthFailedCode, QString()) == Kind::Auth);
    }

    SECTION("Message containing '404' -> NotFoundOrAccess") {
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  0, QStringLiteral("unexpected http status code: 404")) == Kind::NotFoundOrAccess);
    }

    SECTION("Auth wins over 404 when both apply") {
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  kHttpAuthFailedCode, QStringLiteral("unexpected http status code: 404"))
              == Kind::Auth);
    }

    SECTION("DNS / connection / timeout substrings -> HostUnreachable") {
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  0, QStringLiteral("failed to resolve address for nosuchhost.invalid"))
              == Kind::HostUnreachable);
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  0, QStringLiteral("could not resolve host: codeberg.org"))
              == Kind::HostUnreachable);
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  0, QStringLiteral("failed to connect to github.com: Connection refused"))
              == Kind::HostUnreachable);
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  0, QStringLiteral("operation timed out after 30 seconds"))
              == Kind::HostUnreachable);
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  0, QStringLiteral("TIMEOUT")) == Kind::HostUnreachable);
    }

    SECTION("Unrecognized error -> Other") {
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  0, QStringLiteral("SSL certificate problem: unable to get local issuer certificate"))
              == Kind::Other);
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(
                  0, QStringLiteral("destination path is not empty")) == Kind::Other);
        CHECK(cwRemoteRepositoryCloner::classifyCloneError(0, QString()) == Kind::Other);
    }
}

TEST_CASE("cwRemoteRepositoryCloner::friendlyCloneErrorMessage produces the expected text",
          "[cwRemoteRepositoryCloner]")
{
    using Kind = cwRemoteRepositoryCloner::CloneErrorKind;
    const QUrl githubUrl(QStringLiteral("https://github.com/owner/repo.git"));
    const QUrl gitlabUrl(QStringLiteral("https://gitlab.com/owner/repo"));
    const QUrl unknownHostUrl(QStringLiteral("https://codeberg.org/owner/repo"));

    SECTION("None and Other return an empty message") {
        CHECK(cwRemoteRepositoryCloner::friendlyCloneErrorMessage(Kind::None, githubUrl).isEmpty());
        CHECK(cwRemoteRepositoryCloner::friendlyCloneErrorMessage(Kind::Other, githubUrl).isEmpty());
    }

    SECTION("Auth uses the provider's vocabulary") {
        const QString github =
            cwRemoteRepositoryCloner::friendlyCloneErrorMessage(Kind::Auth, githubUrl);
        CHECK_FALSE(github.isEmpty());
        CHECK(github.contains(QStringLiteral("GitHub")));

        const QString gitlab =
            cwRemoteRepositoryCloner::friendlyCloneErrorMessage(Kind::Auth, gitlabUrl);
        CHECK(gitlab.contains(QStringLiteral("GitLab")));

        const QString unknown =
            cwRemoteRepositoryCloner::friendlyCloneErrorMessage(Kind::Auth, unknownHostUrl);
        CHECK_FALSE(unknown.isEmpty());
        // Generic provider has no host-specific name in its message.
        CHECK_FALSE(unknown.contains(QStringLiteral("GitHub")));
    }

    SECTION("NotFoundOrAccess embeds an <a href> to the web URL") {
        const QString msg =
            cwRemoteRepositoryCloner::friendlyCloneErrorMessage(Kind::NotFoundOrAccess, githubUrl);
        CHECK(msg.contains(QStringLiteral("<a href=\"https://github.com/owner/repo\">")));
        CHECK(msg.contains(QStringLiteral("GitHub")));
    }

    SECTION("HostUnreachable includes the host name") {
        const QString msg = cwRemoteRepositoryCloner::friendlyCloneErrorMessage(
            Kind::HostUnreachable, QUrl(QStringLiteral("https://nosuchhost.invalid/repo.git")));
        CHECK(msg.contains(QStringLiteral("nosuchhost.invalid")));
        CHECK(msg.contains(QStringLiteral("Couldn't reach")));
    }

    SECTION("HostUnreachable with empty host falls back to a generic phrase") {
        const QString msg =
            cwRemoteRepositoryCloner::friendlyCloneErrorMessage(Kind::HostUnreachable, QUrl());
        CHECK(msg.contains(QStringLiteral("the server")));
    }
}

TEST_CASE("cwRemoteRepositoryCloner::shouldAutoRetryClone gates the auth retry path",
          "[cwRemoteRepositoryCloner]")
{
    using Kind = cwRemoteRepositoryCloner::CloneErrorKind;
    const QString token = QStringLiteral("ghp_abc123");
    const QString url = QStringLiteral("https://github.com/owner/repo.git");

    SECTION("Retries when Auth + token + pending URL all present") {
        CHECK(cwRemoteRepositoryCloner::shouldAutoRetryClone(Kind::Auth, token, url));
    }

    SECTION("Does not retry when kind is not Auth") {
        CHECK_FALSE(cwRemoteRepositoryCloner::shouldAutoRetryClone(Kind::None, token, url));
        CHECK_FALSE(cwRemoteRepositoryCloner::shouldAutoRetryClone(Kind::NotFoundOrAccess, token, url));
        CHECK_FALSE(cwRemoteRepositoryCloner::shouldAutoRetryClone(Kind::HostUnreachable, token, url));
        CHECK_FALSE(cwRemoteRepositoryCloner::shouldAutoRetryClone(Kind::Other, token, url));
    }

    SECTION("Does not retry when token is empty") {
        CHECK_FALSE(cwRemoteRepositoryCloner::shouldAutoRetryClone(Kind::Auth, QString(), url));
    }

    SECTION("Does not retry when pending URL is empty") {
        CHECK_FALSE(cwRemoteRepositoryCloner::shouldAutoRetryClone(Kind::Auth, token, QString()));
    }
}
