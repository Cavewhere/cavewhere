/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include "catch.hpp"

//Qt includes
#include <QObject>
#include <QMetaProperty>

//Our includes
#include "cwError.h"


/**
 * @brief TEST_CASE
 * If this test falses, getting and setting proprt
 */
TEST_CASE("Getting and setting cwError should work", "[Error]")
{
    cwError error;

    SECTION("cwError init") {
        CHECK(error.suppressed() == false);
        CHECK(error.errorTypeId() == -1);
//        CHECK(error.role() == -1);
//        CHECK(error.index() == -1);
        CHECK(error.message() == QString());
//        CHECK(error.parent() == nullptr);
        CHECK(error.type() == cwError::NoError);
    }

    SECTION("Get and Set") {
        QObject object;

        error.setSupressed(true);
        error.setErrorTypeId(1);
//        error.setIndex(2);
//        error.setRole(3);
        error.setMessage("My error message");
        error.setType(cwError::Fatal);
//        error.setParent(&object);


        CHECK(error.suppressed() == true);
        CHECK(error.errorTypeId() == 1);
//        CHECK(error.role() == 3);
//        CHECK(error.index() == 2);
        CHECK(error.message() == QString("My error message"));
        CHECK(error.type() == cwError::Fatal);
//        CHECK(error.parent() == &object);

        SECTION("Equals operator") {
            cwError otherError;

            otherError.setSupressed(true);
            otherError.setErrorTypeId(1);
//            otherError.setIndex(2);
//            otherError.setRole(3);
            otherError.setMessage("My error message");
            otherError.setType(cwError::Fatal);
//            otherError.setParent(&object);

            CHECK(error == otherError);

            SECTION("Not equals operator - suppressed") {
                otherError.setSupressed(false);
                CHECK(error != otherError);
            }

            SECTION("Not equals operator - errorTypeId") {
                otherError.setErrorTypeId(-2);
                CHECK(error != otherError);
            }

//            SECTION("Not equals operator - index") {
//                otherError.setIndex(-3);
//                CHECK(error != otherError);
//            }

//            SECTION("Not equals operator - role") {
//                otherError.setRole(-4);
//                CHECK(error != otherError);
//            }

            SECTION("Not equals operator - message") {
                otherError.setMessage("Worst message ever");
                CHECK(error != otherError);
            }

            SECTION("Not equals operator - Type") {
                otherError.setType(cwError::Warning);
                CHECK(error != otherError);
            }

//            SECTION("Not equals operator - parent") {
//                otherError.setParent(nullptr);
//                CHECK(error != otherError);
//            }
        }
    }
}

/**
  If this test false, cwError won't get and set correctly
  in QML. QML uses Q_GADGET meta system to extract data
  */
TEST_CASE("Q_GADGET Q_PROPERTY work for cwError")
{
    QObject object;

    cwError error;
    int suppressedIndex = error.staticMetaObject.indexOfProperty("suppressed");
//    int indexIndex = error.staticMetaObject.indexOfProperty("index");
//    int roleIndex = error.staticMetaObject.indexOfProperty("role");
    int errorTypeIdIndex = error.staticMetaObject.indexOfProperty("errorTypeId");
    int messageIndex = error.staticMetaObject.indexOfProperty("message");
    int typeIndex = error.staticMetaObject.indexOfProperty("type");
//    int parentIndex = error.staticMetaObject.indexOfProperty("parent");

    REQUIRE(suppressedIndex != -1);
//    REQUIRE(indexIndex != -1);
//    REQUIRE(roleIndex != -1);
    REQUIRE(errorTypeIdIndex != -1);
    REQUIRE(messageIndex != -1);
    REQUIRE(typeIndex != -1);
//    REQUIRE(parentIndex != -1);

    error.staticMetaObject.property(suppressedIndex).writeOnGadget(&error, true);
//    error.staticMetaObject.property(indexIndex).writeOnGadget(&error, 1);
//    error.staticMetaObject.property(roleIndex).writeOnGadget(&error, 2);
    error.staticMetaObject.property(errorTypeIdIndex).writeOnGadget(&error, 3);
    error.staticMetaObject.property(messageIndex).writeOnGadget(&error, "I like rusty spoons");
    error.staticMetaObject.property(typeIndex).writeOnGadget(&error, cwError::Warning);
//    error.staticMetaObject.property(parentIndex).writeOnGadget(&error, QVariant::fromValue(&object));

    CHECK(error.suppressed() == true);
    CHECK(error.errorTypeId() == 3);
//    CHECK(error.role() == 2);
//    CHECK(error.index() == 1);
    CHECK(error.message() == QString("I like rusty spoons"));
    CHECK(error.type() == cwError::Warning);
//    CHECK(error.parent() == &object);
}





