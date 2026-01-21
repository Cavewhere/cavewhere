//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRemoteRepositoryCloner.h"
#include "cwRepositoryModel.h"

//QQuickGit includes
#include "Account.h"
#include "GitFutureWatcher.h"

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

    cwRepositoryModel repositoryModel;
    repositoryModel.setDefaultRepositoryDir(QUrl::fromLocalFile(tempDir.path()));

    QQuickGit::GitFutureWatcher watcher;
    QQuickGit::Account account;

    cwRemoteRepositoryCloner cloner;
    cloner.setRepositoryModel(&repositoryModel);
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
    CHECK(repositoryModel.rowCount() == 1);
    // const QString legacyProject = tempDir.filePath("PhakeCave3000/Phake Cave 3000.cw");
    const QString newProject = tempDir.filePath("PhakeCave3000/Phake Cave 3000.cwproj");
    CHECK(QFileInfo::exists(newProject));
}
