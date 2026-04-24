#include <catch2/catch_test_macros.hpp>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "cwGitHubCredentials.h"

TEST_CASE("cwGitHubCredentials toJson/fromJson round-trip", "[GitHubCredentials]")
{
    cwGitHubCredentials original;
    original.accessToken = QStringLiteral("ghs_abc123");
    original.refreshToken = QStringLiteral("ghr_def456");
    original.accessTokenExpiresAt = 1800000000LL;

    const QJsonObject json = original.toJson();
    CHECK(json.value(QStringLiteral("v")).toInt() == cwGitHubCredentials::CurrentVersion);

    const cwGitHubCredentials parsed = cwGitHubCredentials::fromJson(json);
    CHECK(parsed.accessToken == original.accessToken);
    CHECK(parsed.refreshToken == original.refreshToken);
    CHECK(parsed.accessTokenExpiresAt == original.accessTokenExpiresAt);
}

TEST_CASE("cwGitHubCredentials fromJson treats missing version as v1", "[GitHubCredentials]")
{
    QJsonObject json;
    json.insert(QStringLiteral("accessToken"), QStringLiteral("ghs_noversion"));

    const cwGitHubCredentials parsed = cwGitHubCredentials::fromJson(json);
    CHECK(parsed.accessToken == QStringLiteral("ghs_noversion"));
    CHECK(parsed.refreshToken.isEmpty());
    CHECK(parsed.accessTokenExpiresAt == -1);
}

TEST_CASE("cwGitHubCredentials fromJson rejects unknown version", "[GitHubCredentials]")
{
    QJsonObject json;
    json.insert(QStringLiteral("v"), 2);
    json.insert(QStringLiteral("accessToken"), QStringLiteral("ghs_futureformat"));

    const cwGitHubCredentials parsed = cwGitHubCredentials::fromJson(json);
    CHECK(parsed.accessToken.isEmpty());
    CHECK(parsed.refreshToken.isEmpty());
    CHECK(parsed.accessTokenExpiresAt == -1);
}

TEST_CASE("cwGitHubCredentials fromJson tolerates missing optional fields", "[GitHubCredentials]")
{
    QJsonObject json;
    json.insert(QStringLiteral("v"), 1);
    json.insert(QStringLiteral("accessToken"), QStringLiteral("ghs_lonely"));

    const cwGitHubCredentials parsed = cwGitHubCredentials::fromJson(json);
    CHECK(parsed.accessToken == QStringLiteral("ghs_lonely"));
    CHECK(parsed.refreshToken.isEmpty());
    CHECK(parsed.accessTokenExpiresAt == -1);
}

TEST_CASE("cwGitHubCredentials fromJson ignores unknown extra keys", "[GitHubCredentials]")
{
    QJsonObject json;
    json.insert(QStringLiteral("v"), 1);
    json.insert(QStringLiteral("accessToken"), QStringLiteral("ghs_extra"));
    json.insert(QStringLiteral("nonsense"), QStringLiteral("ignored"));
    json.insert(QStringLiteral("another"), 42);

    const cwGitHubCredentials parsed = cwGitHubCredentials::fromJson(json);
    CHECK(parsed.accessToken == QStringLiteral("ghs_extra"));
}

TEST_CASE("cwGitHubCredentials fromJson rejects non-string token values", "[GitHubCredentials]")
{
    QJsonObject json;
    json.insert(QStringLiteral("v"), 1);
    json.insert(QStringLiteral("accessToken"), 42);
    json.insert(QStringLiteral("refreshToken"), QJsonValue::Null);

    const cwGitHubCredentials parsed = cwGitHubCredentials::fromJson(json);
    CHECK(parsed.accessToken.isEmpty());
    CHECK(parsed.refreshToken.isEmpty());
}

TEST_CASE("cwGitHubCredentials fromJson rejects control characters in tokens",
          "[GitHubCredentials]")
{
    const QStringList dangerousTokens = {
        QStringLiteral("ghs_abc\ndef"),
        QStringLiteral("ghs_abc\rdef"),
        QString(QStringLiteral("ghs_abc")).append(QChar(0x1F)),
        QString(QStringLiteral("ghs_abc")).append(QChar(0x7F)),
    };

    for (const QString& token : dangerousTokens) {
        QJsonObject json;
        json.insert(QStringLiteral("v"), 1);
        json.insert(QStringLiteral("accessToken"), token);
        json.insert(QStringLiteral("refreshToken"), token);

        const cwGitHubCredentials parsed = cwGitHubCredentials::fromJson(json);
        CHECK(parsed.accessToken.isEmpty());
        CHECK(parsed.refreshToken.isEmpty());
    }
}

TEST_CASE("cwGitHubCredentials fromJson rejects overflow and negative expiry",
          "[GitHubCredentials]")
{
    QJsonObject jsonOverflow;
    jsonOverflow.insert(QStringLiteral("v"), 1);
    jsonOverflow.insert(QStringLiteral("accessToken"), QStringLiteral("ghs_ok"));
    jsonOverflow.insert(QStringLiteral("accessTokenExpiresAt"), 1e20);

    CHECK(cwGitHubCredentials::fromJson(jsonOverflow).accessTokenExpiresAt == -1);

    QJsonObject jsonNegative;
    jsonNegative.insert(QStringLiteral("v"), 1);
    jsonNegative.insert(QStringLiteral("accessToken"), QStringLiteral("ghs_ok"));
    jsonNegative.insert(QStringLiteral("accessTokenExpiresAt"), -500.0);

    CHECK(cwGitHubCredentials::fromJson(jsonNegative).accessTokenExpiresAt == -1);
}

TEST_CASE("cwGitHubCredentials fromJson accepts numeric expiry encoded as string",
          "[GitHubCredentials]")
{
    QJsonObject json;
    json.insert(QStringLiteral("v"), 1);
    json.insert(QStringLiteral("accessToken"), QStringLiteral("ghs_strexp"));
    json.insert(QStringLiteral("accessTokenExpiresAt"), QStringLiteral("1800000000"));

    CHECK(cwGitHubCredentials::fromJson(json).accessTokenExpiresAt == 1800000000LL);

    QJsonObject jsonBad;
    jsonBad.insert(QStringLiteral("v"), 1);
    jsonBad.insert(QStringLiteral("accessToken"), QStringLiteral("ghs_strbad"));
    jsonBad.insert(QStringLiteral("accessTokenExpiresAt"), QStringLiteral("not-a-number"));

    CHECK(cwGitHubCredentials::fromJson(jsonBad).accessTokenExpiresAt == -1);
}
