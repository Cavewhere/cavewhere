// Catch
#include <catch2/catch_test_macros.hpp>

// Qt
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
