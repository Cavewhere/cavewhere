//Catch includes
#include <QtCore/qeventloop.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwGitHubDeviceAuth.h"

//Qt includes
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>

//This seems to work!
// TEST_CASE("Test github oAuth device", "[cwGitHubDeviceAuth]") {

//     // Example usage inside some QObject-based controller
//     auto clientId = QStringLiteral("Ov23ctOCCgOdD9y2mSs9");
//     auto* auth = new cwGitHubDeviceAuth(clientId);

//     QEventLoop loop;

//     QObject::connect(auth, &cwGitHubDeviceAuth::deviceCodeReceived,
//                      [auth](const cwGitHubDeviceAuth::DeviceCodeInfo& info) {

//                          // Debug log all fields
//                          qDebug() << "DeviceCodeInfo:";
//                          qDebug() << "  deviceCode =" << info.deviceCode;
//                          qDebug() << "  userCode =" << info.userCode;
//                          qDebug() << "  verificationWebAddress =" << info.verificationWebAddress.toString();
//                          qDebug() << "  expiresInSeconds =" << info.expiresInSeconds;
//                          qDebug() << "  pollIntervalSeconds =" << info.pollIntervalSeconds;

//                          if (info.deviceCode.isEmpty()) {
//                              // handle failure

//                              return;
//                          }

//         // 1) Show info.userCode to the user in your UI
//         // 2) Optionally open the verification page:
//         QDesktopServices::openUrl(info.verificationWebAddress);

//         auth->startPollingForAccessToken(info);
//     });

//     QObject::connect(auth, &cwGitHubDeviceAuth::accessTokenReceived,
//                      [&loop](const cwGitHubDeviceAuth::AccessTokenResult& result) {

//                          qDebug() << "AccessTokenResult:";
//                          qDebug() << "  accessToken =" << !result.accessToken.isEmpty();
//                          qDebug() << "  tokenType =" << result.tokenType;
//                          qDebug() << "  scope =" << result.scope;

//                          if (!result.success) {
//                              // Handle errors: authorization_pending, slow_down, access_denied, expired_token, etc.
//                              return;
//                          }

//                          QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/user/repos")));
//                          request.setRawHeader("Accept", "application/vnd.github+json");
//                          request.setRawHeader("User-Agent", "CaveWhere-Qt"); // any identifier is fine
//                          request.setRawHeader("Authorization", QByteArray("Bearer ") + result.accessToken.toLocal8Bit());

//                          QNetworkAccessManager* manager = new QNetworkAccessManager();
//                          QNetworkReply* reply = manager->get(request);

//                          QObject::connect(reply, &QNetworkReply::finished,  [reply, &loop]() {
//                              const QByteArray data = reply->readAll();
//                              reply->deleteLater();

//                              QJsonParseError err{};
//                              QJsonDocument doc = QJsonDocument::fromJson(data, &err);

//                              if (err.error != QJsonParseError::NoError) {
//                                  qDebug() << "Failed to parse response:" << err.errorString();
//                                  return;
//                              }

//                              if (!doc.isArray()) {
//                                  qDebug() << "Unexpected response:" << data;
//                                  return;
//                              }

//                              QJsonArray repos = doc.array();
//                              for (const QJsonValue& v : repos) {
//                                  QJsonObject repo = v.toObject();
//                                  qDebug() << "Repo name:" << repo.value("name").toString()
//                                           << "URL:" << repo.value("html_url").toString()
//                                           << "Private:" << repo.value("private").toBool();
//                              }
//                              loop.exit(0);
//                          });

//                          // Save result.accessToken securely and start calling GitHub APIs
//                      });

//     // Kick it off:
//     auth->requestDeviceCode({QStringLiteral("repo"), QStringLiteral("read:user")});

//     loop.exec();
// }
