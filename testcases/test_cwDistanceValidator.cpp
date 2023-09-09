//Catch includes
#include <catch2/catch.hpp>

//Our includes
#include "cwDistanceValidator.h"

TEST_CASE("cwDistanceValidator should work correctly", "[cwDistanceValidator]") {

    cwDistanceValidator validator;
    CHECK(validator.validate("1.0") == QValidator::Acceptable);
    CHECK(validator.validate("1") == QValidator::Acceptable);
    CHECK(validator.validate("oue") == QValidator::Invalid);
    CHECK(validator.validate("-1.0") == QValidator::Invalid);

//    SECTION("Localization") {
//        QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
//        CHECK(validator.validate("1.0") == QValidator::Acceptable);
//        CHECK(validator.validate("1,0") == QValidator::Acceptable);
//        QLocale::setDefault(QLocale::system());
//    }

}
