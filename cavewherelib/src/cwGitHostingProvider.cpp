/**************************************************************************
**
**    Copyright (C) 2026 by CaveWhere
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGitHostingProvider.h"

//Std includes
#include <array>

namespace {

constexpr int kGitSuffixLength = 4; // length of ".git"

const std::array<cwGitHostingProviderInfo, 4>& providers()
{
    static const std::array<cwGitHostingProviderInfo, 4> kProviders = {{
        {
            QStringLiteral("github.com"),
            QStringLiteral("GitHub"),
            QStringLiteral("/settings/access"),
            QStringLiteral("Open repository on GitHub"),
            QStringLiteral(
                "Your GitHub credentials are invalid or have expired. "
                "Sign in to GitHub to retry cloning."),
            QStringLiteral(
                "This repository doesn't exist, or your GitHub account "
                "doesn't have access to it. If you were invited as a "
                "collaborator, check that you've accepted the pending "
                "invitation.")
        },
        {
            QStringLiteral("gitlab.com"),
            QStringLiteral("GitLab"),
            QStringLiteral("/-/project_members"),
            QStringLiteral("Open project on GitLab"),
            QStringLiteral(
                "Your GitLab credentials are invalid or have expired. "
                "Sign in to GitLab to retry cloning."),
            QStringLiteral(
                "This project doesn't exist, or your GitLab account "
                "doesn't have access to it. If you were added as a "
                "project member, check that you've accepted any "
                "pending project invitations.")
        },
        {
            QStringLiteral("bitbucket.org"),
            QStringLiteral("Bitbucket"),
            QStringLiteral("/admin/access-keys"),
            QStringLiteral("Open repository on Bitbucket"),
            QStringLiteral(
                "Your Bitbucket credentials are invalid or have expired. "
                "Sign in to Bitbucket to retry cloning."),
            QStringLiteral(
                "This repository doesn't exist, or your Bitbucket "
                "account doesn't have access to it. If you were invited "
                "to a workspace or repository, check that you've "
                "accepted any pending invitations.")
        },
        {
            QString(),
            QString(),
            QString(),
            QStringLiteral("Open repository in browser"),
            QStringLiteral(
                "Your credentials are invalid or have expired. "
                "Sign in again to retry cloning."),
            QStringLiteral(
                "This repository doesn't exist, or your account "
                "doesn't have access to it. If you were invited, "
                "check that you've accepted any pending invitations.")
        }
    }};
    return kProviders;
}

const cwGitHostingProviderInfo& genericInfo()
{
    return providers().back();
}

QString trimRepoPath(QString path)
{
    if (path.endsWith(QStringLiteral(".git"))) {
        path.chop(kGitSuffixLength);
    }
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path;
}

} // namespace

const cwGitHostingProviderInfo& cwGitHostingProvider::forHost(const QString& host)
{
    const QString lower = host.toLower();
    for (const auto& info : providers()) {
        if (!info.host.isEmpty() && info.host == lower) {
            return info;
        }
    }
    return genericInfo();
}

const cwGitHostingProviderInfo& cwGitHostingProvider::forUrl(const QUrl& cloneUrl)
{
    return forHost(cloneUrl.host());
}

QStringList cwGitHostingProvider::knownHosts()
{
    static const QStringList hosts = [] {
        QStringList out;
        for (const auto& info : providers()) {
            if (!info.host.isEmpty()) {
                out << info.host;
            }
        }
        return out;
    }();
    return hosts;
}

QUrl cwGitHostingProvider::repositoryWebUrl(const cwGitHostingProviderInfo& info,
                                            const QUrl& cloneUrl)
{
    if (!cloneUrl.isValid()) {
        return {};
    }

    const QString path = trimRepoPath(cloneUrl.path());
    if (path.isEmpty()) {
        return {};
    }

    QUrl out;
    out.setScheme(QStringLiteral("https"));

    if (info.host.isEmpty()) {
        // Generic: best-effort, only safe for https inputs.
        if (cloneUrl.scheme() != QLatin1String("https")) {
            return {};
        }
        if (cloneUrl.host().isEmpty()) {
            return {};
        }
        out.setHost(cloneUrl.host());
    } else {
        // Defensive: never build a github URL for a gitlab clone URL.
        if (cloneUrl.host().toLower() != info.host) {
            return {};
        }
        out.setHost(info.host);
    }
    out.setPath(path);
    return out;
}

QUrl cwGitHostingProvider::collaboratorSettingsUrl(const cwGitHostingProviderInfo& info,
                                                   const QUrl& repoUrl)
{
    if (info.collaboratorPathSuffix.isEmpty()) {
        return {};
    }
    const QUrl webUrl = repositoryWebUrl(info, repoUrl);
    if (webUrl.isEmpty()) {
        return {};
    }
    QUrl out = webUrl;
    out.setPath(webUrl.path() + info.collaboratorPathSuffix);
    return out;
}

QString cwGitHostingProvider::notFoundOrAccessMessage(const cwGitHostingProviderInfo& info,
                                                     const QUrl& cloneUrl)
{
    const QUrl webUrl = repositoryWebUrl(info, cloneUrl);
    if (webUrl.isEmpty()) {
        return info.notFoundMessage;
    }

    return info.notFoundMessage
        + QStringLiteral(" <a href=\"%1\">%2</a>")
              .arg(webUrl.toString().toHtmlEscaped(),
                   info.openLinkLabel.toHtmlEscaped());
}
