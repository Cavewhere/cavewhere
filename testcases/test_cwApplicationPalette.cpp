//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwApplicationPalette.h"

//Qt includes
#include <QCoreApplication>
#include <QGuiApplication>
#include <QPalette>

TEST_CASE("cwApplicationPalette should apply its colors to the application palette", "[cwApplicationPalette]") {
    const QPalette savedPalette = QGuiApplication::palette();

    const QColor windowColor(0x12, 0x34, 0x56);
    const QColor laterWindowColor(0x65, 0x43, 0x21);
    const QColor disabledTextColor(0xAB, 0xCD, 0xEF, 0x66);
    REQUIRE(savedPalette.color(QPalette::Window) != windowColor);
    REQUIRE(savedPalette.color(QPalette::Window) != laterWindowColor);
    REQUIRE(savedPalette.color(QPalette::Disabled, QPalette::WindowText) != disabledTextColor);

    {
        cwApplicationPalette palette;
        palette.classBegin();
        palette.setWindow(windowColor);
        palette.setDisabledText(disabledTextColor);
        palette.componentComplete();

        const QPalette applied = QGuiApplication::palette();
        CHECK(applied.color(QPalette::Active, QPalette::Window) == windowColor);
        CHECK(applied.color(QPalette::Inactive, QPalette::Window) == windowColor);
        CHECK(applied.color(QPalette::Disabled, QPalette::WindowText) == disabledTextColor);
        CHECK(applied.color(QPalette::Disabled, QPalette::Text) == disabledTextColor);
        CHECK(applied.color(QPalette::Disabled, QPalette::ButtonText) == disabledTextColor);

        // A role the object leaves unset keeps the application's color
        CHECK(applied.color(QPalette::Active, QPalette::Base) == savedPalette.color(QPalette::Active, QPalette::Base));
        CHECK(applied.color(QPalette::Active, QPalette::Highlight) == savedPalette.color(QPalette::Active, QPalette::Highlight));

        // Changes after completion apply on the next event-loop turn
        palette.setWindow(laterWindowColor);
        QCoreApplication::processEvents();
        CHECK(QGuiApplication::palette().color(QPalette::Active, QPalette::Window) == laterWindowColor);
    }

    QGuiApplication::setPalette(savedPalette);
    CHECK(QGuiApplication::palette().color(QPalette::Active, QPalette::Window) == savedPalette.color(QPalette::Active, QPalette::Window));
}
