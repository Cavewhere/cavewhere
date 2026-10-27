// Catch includes
#include <catch2/catch_test_macros.hpp>

// Our includes
#include "cwPage.h"
#include "cwPageSelectionModel.h"

// Qt includes
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>

namespace
{
QQmlComponent* makePageComponent(QQmlEngine& engine, QObject* parent)
{
    static const char* pageQml =
        "import QtQuick 2.15\n"
        "Item {}\n";

    auto* component = new QQmlComponent(&engine, parent);
    component->setData(QByteArray(pageQml), QUrl());
    REQUIRE(component->status() == QQmlComponent::Ready);
    return component;
}
}

TEST_CASE("cwPage destroys a child that setName evicted from the parent's lookup", "[cwPage]")
{
    QQmlEngine engine;
    QPointer<cwPage> evicted;
    QPointer<cwPage> renamed;

    {
        cwPageSelectionModel model;
        auto* component = makePageComponent(engine, &model);

        auto* parent = model.registerPage(nullptr, QStringLiteral("Data"), component);
        auto* alpha = model.registerPage(parent, QStringLiteral("Cave=Alpha"), component);
        auto* beta = model.registerPage(parent, QStringLiteral("Cave=Beta"), component);

        evicted = alpha;
        renamed = beta;

        //ChildPages is keyed by name, so renaming Beta onto Alpha's name
        //drops Alpha's entry while Alpha stays a live QObject child.
        beta->setName(QStringLiteral("Cave=Alpha"));

        REQUIRE(parent->childPage(QStringLiteral("Cave=Alpha")) == beta);
        REQUIRE(!evicted.isNull());
    }

    //Tearing the tree down must not leave the evicted page reading its
    //already-destroyed parent's lookup table.
    REQUIRE(evicted.isNull());
    REQUIRE(renamed.isNull());
}
