//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QString>
#include <QRegularExpression>

//Our includes
#include "cavewhereVersion.h"
#include "TestHelper.h"

TEST_CASE("Cavewhere Version should be generated correctly", "[version]") {
    CHECK(CavewhereVersion.isEmpty() == false);
    CHECK(CavewhereVersion.toStdString() != "Unknown Version");

    QRegularExpression regex("^\\d+\\.\\d+(-\\w+)?(-\\d+-g[a-f0-9]{7})?$");
    QRegularExpressionMatch results = regex.match(CavewhereVersion);

    INFO("Cavewhere version:" << CavewhereVersion);
    CHECK(results.hasMatch());
}
