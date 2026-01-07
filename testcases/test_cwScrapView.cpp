//Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

//Qt includes
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <memory>

//Our includes
#include "cwScrapView.h"
#include "cwScrapItem.h"
#include "cwNote.h"
#include "cwScrap.h"

namespace {
cwScrapView* createScrapView(QQmlEngine& engine, std::unique_ptr<QObject>& rootOut) {
    qmlRegisterType<cwScrapView>("Test", 1, 0, "ScrapView");

    const char* rootQml =
        "import QtQuick 2.15\n"
        "import Test 1.0\n"
        "Item {\n"
        "  id: root\n"
        "  width: 200; height: 200\n"
        "  ScrapView { id: view; objectName: \"scrapView\"; targetItem: root }\n"
        "}\n";

    QQmlComponent component(&engine);
    component.setData(QByteArray(rootQml), QUrl());
    QObject* root = component.create();
    REQUIRE(root != nullptr);

    auto view = root->findChild<cwScrapView*>("scrapView");
    REQUIRE(view != nullptr);

    rootOut.reset(root);

    return view;
}
} // namespace

TEST_CASE("ScrapView clears selection when note changes", "[cwScrapView]") {
    QQmlEngine engine;
    std::unique_ptr<QObject> root;
    cwScrapView* view = createScrapView(engine, root);

    auto note1 = std::make_unique<cwNote>();
    auto note2 = std::make_unique<cwNote>();
    auto scrap1 = new cwScrap();
    auto scrap2 = new cwScrap();
    note1->addScrap(scrap1);
    note2->addScrap(scrap2);

    view->setNote(note1.get());
    view->setSelectScrapIndex(0);

    REQUIRE(view->selectedScrapItem() != nullptr);
    CHECK(view->selectedScrapItem()->scrap() == scrap1);

    view->setNote(note2.get());

    CHECK(view->selectedScrapItem() == nullptr);

}

TEST_CASE("ScrapView doesn't retain old scrap after note data reset", "[cwScrapView]") {
    QQmlEngine engine;
    std::unique_ptr<QObject> root;
    cwScrapView* view = createScrapView(engine, root);

    auto note1 = std::make_unique<cwNote>();
    auto note2 = std::make_unique<cwNote>();
    auto scrap1 = new cwScrap();
    auto scrap2 = new cwScrap();
    note1->addScrap(scrap1);
    note2->addScrap(scrap2);

    view->setNote(note1.get());
    view->setSelectScrapIndex(0);

    REQUIRE(view->selectedScrapItem() != nullptr);
    CHECK(view->selectedScrapItem()->scrap() == scrap1);

    note1->setData(note2->data());

    CHECK(view->selectedScrapItem() == nullptr);
    if(view->selectedScrapItem() != nullptr) {
        CHECK(view->selectedScrapItem()->scrap() != scrap1);
    }

}
