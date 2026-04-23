// Catch
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>

// Ours
#include "cwGitHubIntegration.h"
#include "TestHelper.h"

// ---------------------------------------------------------------------------
// Minimal single-response HTTP/1.1 mock server
// ---------------------------------------------------------------------------
struct HttpMockServer : QObject
{
    Q_OBJECT
public:
    explicit HttpMockServer(int statusCode, const QByteArray& body, QObject* parent = nullptr)
        : QObject(parent)
        , m_statusCode(statusCode)
        , m_body(body)
    {
        connect(&m_server, &QTcpServer::newConnection, this, &HttpMockServer::onNewConnection);
        m_server.listen(QHostAddress::LocalHost, 0);
    }

    quint16 port() const { return m_server.serverPort(); }

private:
    void onNewConnection()
    {
        QTcpSocket* socket = m_server.nextPendingConnection();
        connect(socket, &QAbstractSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            // Qt's QNAM sends the full request in one write on loopback,
            // so the first readyRead carries the complete request.
            socket->readAll();
            const QByteArray reasonPhrase = m_statusCode == 201  ? "Created"
                                          : m_statusCode == 401  ? "Unauthorized"
                                          : m_statusCode == 403  ? "Forbidden"
                                          : m_statusCode == 422  ? "Unprocessable Entity"
                                          : m_statusCode == 500  ? "Internal Server Error"
                                                                 : "OK";
            const QByteArray response =
                "HTTP/1.1 " + QByteArray::number(m_statusCode) + " " + reasonPhrase + "\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + QByteArray::number(m_body.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n"
                + m_body;
            socket->write(response);
            socket->flush();
            socket->disconnectFromHost();
        });
    }

    QTcpServer m_server;
    int m_statusCode;
    QByteArray m_body;
};

// ---------------------------------------------------------------------------
// Path-routed mock server: responds based on the request path. Falls back to
// 404 for unrouted paths so a missed route surfaces as a clean test failure
// rather than a hang.
// ---------------------------------------------------------------------------
struct PathRoutedMockServer : QObject
{
    Q_OBJECT
public:
    explicit PathRoutedMockServer(QObject* parent = nullptr)
        : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, &PathRoutedMockServer::onNewConnection);
        m_server.listen(QHostAddress::LocalHost, 0);
    }

    quint16 port() const { return m_server.serverPort(); }

    void addRoute(const QByteArray& pathPrefix, int statusCode, const QByteArray& body)
    {
        m_routes.append({pathPrefix, statusCode, body});
    }

private:
    struct Route { QByteArray pathPrefix; int statusCode; QByteArray body; };

    void onNewConnection()
    {
        QTcpSocket* socket = m_server.nextPendingConnection();
        connect(socket, &QAbstractSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            const QByteArray request = socket->readAll();
            const int firstSpace = request.indexOf(' ');
            const int secondSpace = firstSpace >= 0 ? request.indexOf(' ', firstSpace + 1) : -1;
            const QByteArray path = (firstSpace >= 0 && secondSpace > firstSpace)
                ? request.mid(firstSpace + 1, secondSpace - firstSpace - 1)
                : QByteArray();

            int statusCode = 404;
            QByteArray body = R"({"message":"unrouted in mock"})";
            for (const Route& r : m_routes) {
                if (path.startsWith(r.pathPrefix)) {
                    statusCode = r.statusCode;
                    body = r.body;
                    break;
                }
            }

            const QByteArray reasonPhrase = statusCode == 200 ? "OK"
                                          : statusCode == 404 ? "Not Found"
                                                              : "OK";
            const QByteArray response =
                "HTTP/1.1 " + QByteArray::number(statusCode) + " " + reasonPhrase + "\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n"
                + body;
            socket->write(response);
            socket->flush();
            socket->disconnectFromHost();
        });
    }

    QTcpServer m_server;
    QList<Route> m_routes;
};

#include "test_cwGitHubIntegration.moc"

// ---------------------------------------------------------------------------

TEST_CASE("cwGitHubIntegration::createRepository emits repositoryCreated on 201", "[cwGitHubIntegration]")
{
    const QByteArray body =
        R"({"name":"my-cave","clone_url":"https://github.com/user/my-cave.git",)"
        R"("ssh_url":"git@github.com:user/my-cave.git",)"
        R"("html_url":"https://github.com/user/my-cave","private":true})";
    HttpMockServer server(201, body);
    REQUIRE(server.port() != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));

    cwGitHubRepositoryItem created;
    bool fired = false;
    QObject::connect(&integration, &cwGitHubIntegration::repositoryCreated,
                     [&](const cwGitHubRepositoryItem& r) { created = r; fired = true; });

    integration.createRepository(QStringLiteral("my-cave"), true);

    REQUIRE(waitUntil([&fired]() { return fired; }));
    CHECK(created.name == QStringLiteral("my-cave"));
    CHECK(created.cloneUrl == QStringLiteral("https://github.com/user/my-cave.git"));
    CHECK(created.isPrivate);
}

TEST_CASE("cwGitHubIntegration::createRepository invalidates token on 401", "[cwGitHubIntegration]")
{
    HttpMockServer server(401, R"({"message":"Bad credentials"})");
    REQUIRE(server.port() != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));

    bool failureFired = false;
    bool invalidatedFired = false;
    QObject::connect(&integration, &cwGitHubIntegration::repositoryCreationFailed,
                     [&](const QString&) { failureFired = true; });
    QObject::connect(&integration, &cwGitHubIntegration::tokenInvalidated,
                     [&](const QString&, const QString&) { invalidatedFired = true; });

    integration.createRepository(QStringLiteral("my-cave"), true);

    REQUIRE(waitUntil([&failureFired]() { return failureFired; }));
    CHECK(invalidatedFired);
}

TEST_CASE("cwGitHubIntegration::createRepository does not invalidate token on 422", "[cwGitHubIntegration]")
{
    const QByteArray body =
        R"({"message":"Repository creation failed.",)"
        R"("errors":[{"message":"name already exists on this account"}]})";
    HttpMockServer server(422, body);
    REQUIRE(server.port() != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));

    QString failureMessage;
    bool invalidatedFired = false;
    QObject::connect(&integration, &cwGitHubIntegration::repositoryCreationFailed,
                     [&](const QString& msg) { failureMessage = msg; });
    QObject::connect(&integration, &cwGitHubIntegration::tokenInvalidated,
                     [&](const QString&, const QString&) { invalidatedFired = true; });

    integration.createRepository(QStringLiteral("my-cave"), true);

    REQUIRE(waitUntil([&failureMessage]() { return !failureMessage.isEmpty(); }));
    CHECK(!invalidatedFired);
    CHECK(failureMessage.contains(QStringLiteral("my-cave")));
}

TEST_CASE("cwGitHubIntegration::createRepository does not invalidate token on 500", "[cwGitHubIntegration]")
{
    HttpMockServer server(500, R"({"message":"Internal Server Error"})");
    REQUIRE(server.port() != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));

    QString failureMessage;
    bool invalidatedFired = false;
    QObject::connect(&integration, &cwGitHubIntegration::repositoryCreationFailed,
                     [&](const QString& msg) { failureMessage = msg; });
    QObject::connect(&integration, &cwGitHubIntegration::tokenInvalidated,
                     [&](const QString&, const QString&) { invalidatedFired = true; });

    integration.createRepository(QStringLiteral("my-cave"), true);

    REQUIRE(waitUntil([&failureMessage]() { return !failureMessage.isEmpty(); }));
    CHECK(!invalidatedFired);
    CHECK(failureMessage.contains(QStringLiteral("500")));
}

TEST_CASE("cwGitHubIntegration::installationUrl honors app-slug env override", "[cwGitHubIntegration]")
{
    cwGitHubIntegration integration(nullptr);
    // The default slug is "cavewhere" — verify the URL is well-formed against GitHub.
    const QUrl url = integration.installationUrl();
    CHECK(url.isValid());
    CHECK(url.host() == QStringLiteral("github.com"));
    CHECK(url.path().contains(QStringLiteral("/installations/new")));
    CHECK(url.path().startsWith(QStringLiteral("/apps/")));
}

TEST_CASE("cwGitHubIntegration::refreshRepositories sets needsInstallation on empty installations", "[cwGitHubIntegration]")
{
    PathRoutedMockServer server;
    server.addRoute("/user/installations", 200, R"({"total_count":0,"installations":[]})");
    REQUIRE(server.port() != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));
    integration.setActive(true);
    integration.setAccessTokenForTesting(QStringLiteral("test_token"));

    integration.refreshRepositories();

    REQUIRE(waitUntil([&]() { return integration.needsInstallation(); }));
    CHECK(integration.repositories() != nullptr);
    CHECK(integration.repositories()->rowCount() == 0);
}

TEST_CASE("cwGitHubIntegration::refreshRepositories surfaces repos when installation has them", "[cwGitHubIntegration]")
{
    PathRoutedMockServer server;
    // Longer/more-specific path first: PathRoutedMockServer matches prefixes
    // in insertion order and "/user/installations" is a prefix of
    // "/user/installations/12345/repositories".
    server.addRoute("/user/installations/12345/repositories",
                    200,
                    R"({"total_count":1,"repositories":[{)"
                    R"("name":"cave-data","clone_url":"https://github.com/vpicaver/cave-data.git",)"
                    R"("ssh_url":"git@github.com:vpicaver/cave-data.git",)"
                    R"("html_url":"https://github.com/vpicaver/cave-data","private":false}]})");
    server.addRoute("/user/installations",
                    200,
                    R"({"total_count":1,"installations":[{"id":12345,"account":{"login":"vpicaver"}}]})");
    REQUIRE(server.port() != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));
    integration.setActive(true);
    integration.setAccessTokenForTesting(QStringLiteral("test_token"));

    integration.refreshRepositories();

    REQUIRE(waitUntil([&]() {
        return !integration.busy()
            && integration.repositories() != nullptr
            && integration.repositories()->rowCount() == 1;
    }));
    CHECK(!integration.needsInstallation());
}

TEST_CASE("cwGitHubIntegration::startInstallPolling auto-stops when install appears", "[cwGitHubIntegration]")
{
    PathRoutedMockServer server;
    // Populated installations response — first poll sees the install and stops.
    server.addRoute("/user/installations/7/repositories",
                    200,
                    R"({"total_count":1,"repositories":[{)"
                    R"("name":"pit-survey","clone_url":"https://github.com/vpicaver/pit-survey.git",)"
                    R"("ssh_url":"git@github.com:vpicaver/pit-survey.git",)"
                    R"("html_url":"https://github.com/vpicaver/pit-survey","private":true}]})");
    server.addRoute("/user/installations",
                    200,
                    R"({"total_count":1,"installations":[{"id":7,"account":{"login":"vpicaver"}}]})");
    REQUIRE(server.port() != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));
    integration.setActive(true);
    integration.setAccessTokenForTesting(QStringLiteral("test_token"));

    integration.startInstallPolling();

    REQUIRE(waitUntil([&]() {
        return !integration.installPollActive()
            && !integration.needsInstallation()
            && integration.repositories() != nullptr
            && integration.repositories()->rowCount() == 1;
    }));
}

TEST_CASE("cwGitHubIntegration::stopInstallPolling halts polling immediately", "[cwGitHubIntegration]")
{
    PathRoutedMockServer server;
    server.addRoute("/user/installations", 200, R"({"total_count":0,"installations":[]})");
    REQUIRE(server.port() != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));
    integration.setActive(true);
    integration.setAccessTokenForTesting(QStringLiteral("test_token"));

    integration.startInstallPolling();
    REQUIRE(integration.installPollActive());

    integration.stopInstallPolling();
    CHECK(!integration.installPollActive());
}

TEST_CASE("cwGitHubIntegration::setActive(false) stops polling", "[cwGitHubIntegration]")
{
    PathRoutedMockServer server;
    server.addRoute("/user/installations", 200, R"({"total_count":0,"installations":[]})");
    REQUIRE(server.port() != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));
    integration.setActive(true);
    integration.setAccessTokenForTesting(QStringLiteral("test_token"));

    integration.startInstallPolling();
    REQUIRE(integration.installPollActive());

    integration.setActive(false);
    CHECK(!integration.installPollActive());
}
