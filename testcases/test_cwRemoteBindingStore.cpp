#include <catch2/catch_test_macros.hpp>

#include <QSettings>

#include "cwRemoteBindingStore.h"

TEST_CASE("cwRemoteBindingStore canonical key normalization and lookup", "[cwRemoteBindingStore]")
{
    {
        QSettings settings;
        settings.remove(QStringLiteral("RemoteAccountBindings"));
    }

    SECTION("Canonical remote key normalization")
    {
        CHECK(cwRemoteBindingStore::canonicalRemoteKey(QStringLiteral("https://github.com/Cavewhere/PhakeCave3000.git"))
              == QStringLiteral("github.com/cavewhere/phakecave3000"));
        CHECK(cwRemoteBindingStore::canonicalRemoteKey(QStringLiteral("git@github.com:Cavewhere/PhakeCave3000.git"))
              == QStringLiteral("github.com/cavewhere/phakecave3000"));
        CHECK(cwRemoteBindingStore::canonicalRemoteKey(QStringLiteral("ssh://git@github.com/Cavewhere/PhakeCave3000"))
              == QStringLiteral("github.com/cavewhere/phakecave3000"));
        CHECK(cwRemoteBindingStore::canonicalRemoteKey(QStringLiteral("api.github.com/Cavewhere/PhakeCave3000.git"))
              == QStringLiteral("api.github.com/cavewhere/phakecave3000"));
    }

    {
        cwRemoteBindingStore store;
        store.bindRemoteToAccount(QStringLiteral("git@github.com:Cavewhere/PhakeCave3000.git"),
                                  QStringLiteral("account-A"));

        CHECK(store.accountIdForRemote(QStringLiteral("https://github.com/Cavewhere/PhakeCave3000.git"))
              == QStringLiteral("account-A"));
        CHECK(store.accountIdForRemote(QStringLiteral("https://gitlab.com/group/repo.git")).isEmpty());
    }

    {
        cwRemoteBindingStore store;
        CHECK(store.accountIdForRemote(QStringLiteral("ssh://git@github.com/Cavewhere/PhakeCave3000.git"))
              == QStringLiteral("account-A"));

        store.removeBindingsForAccount(QStringLiteral("account-A"));
        CHECK(store.accountIdForRemote(QStringLiteral("https://github.com/Cavewhere/PhakeCave3000.git")).isEmpty());
    }
}

TEST_CASE("cwRemoteBindingStore binding resolution prefers latest and persists", "[cwRemoteBindingStore]")
{
    {
        QSettings settings;
        settings.remove(QStringLiteral("RemoteAccountBindings"));
    }

    const QString remoteA = QStringLiteral("git@github.com:Cavewhere/PhakeCave3000.git");
    const QString remoteB = QStringLiteral("https://github.com/Cavewhere/AnotherRepo.git");

    {
        cwRemoteBindingStore store;
        store.bindRemoteToAccount(remoteA, QStringLiteral("account-A"));
        CHECK(store.accountIdForRemote(remoteA) == QStringLiteral("account-A"));

        store.bindRemoteToAccount(remoteA, QStringLiteral("account-B"));
        CHECK(store.accountIdForRemote(remoteA) == QStringLiteral("account-B"));

        store.bindRemoteToAccount(remoteB, QStringLiteral("account-A"));
        CHECK(store.accountIdForRemote(remoteB) == QStringLiteral("account-A"));
    }

    {
        cwRemoteBindingStore reloadedStore;
        CHECK(reloadedStore.accountIdForRemote(QStringLiteral("https://github.com/Cavewhere/PhakeCave3000"))
              == QStringLiteral("account-B"));
        CHECK(reloadedStore.accountIdForRemote(QStringLiteral("ssh://git@github.com/Cavewhere/AnotherRepo.git"))
              == QStringLiteral("account-A"));

        reloadedStore.removeBindingsForAccount(QStringLiteral("account-B"));
        CHECK(reloadedStore.accountIdForRemote(remoteA).isEmpty());
        CHECK(reloadedStore.accountIdForRemote(remoteB) == QStringLiteral("account-A"));
    }
}
