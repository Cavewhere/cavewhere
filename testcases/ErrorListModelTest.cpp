/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwError.h"
#include "cwErrorListModel.h"

//Qt includes
#include "cwSignalSpy.h"

void checkError(const cwError& error, cwErrorListModel& model, int idx) {
    REQUIRE(idx >= 0);
    REQUIRE(idx < model.count());

    CHECK(error == model.at(idx));
    // CHECK(error == model.get(idx).value<cwError>());

    CHECK(QVariant(error.message()) == model.data(model.index(idx), model.roleForName("message")));
    CHECK(QVariant(error.errorTypeId()) == model.data(model.index(idx), model.roleForName("errorTypeId")));
    CHECK(QVariant(error.suppressed()) == model.data(model.index(idx), model.roleForName("suppressed")));
    CHECK(QVariant(error.type()) == model.data(model.index(idx), model.roleForName("errorType")));

}

TEST_CASE("Basic QML Gadget List operations") {
    cwErrorListModel model;

    CHECK(model.isEmpty() == true);

    QList<cwError> errors;
    int size = 10;
    for(int i = 0; i < size; i++) {
        cwError error;
        error.setMessage(QString("Error %1").arg(i));
        error.setType(cwError::Fatal);
        error.setErrorTypeId(i);
        errors.append(error);
    }


    SECTION("Check append") {
        foreach(cwError error, errors) {
            model.append(error);
        }

        REQUIRE(size == model.size());
        CHECK(model.count() == model.size());

        auto roleNames = model.roleNames();

        for(int i = 0; i < size; i++) {
            checkError(errors.at(i), model, i);
        }

        SECTION("Check clear") {
            CHECK(model.isEmpty() == false);

            model.clear();

            CHECK(model.size() == 0);
            CHECK(model.isEmpty() == true);
        }

        SECTION("Check contains") {
            CHECK(model.contains(errors.first()) == true);
            // CHECK(model.contains(QVariant::fromValue(errors.first())) == true);

            CHECK(model.contains(errors.last()) == true);
            // CHECK(model.contains(QVariant::fromValue(errors.last())) == true);
        }

        SECTION("Check indexOf") {
            CHECK(model.indexOf(errors.first()) == 0);
            // CHECK(model.indexOf(QVariant::fromValue(errors.first())) == 0);

            CHECK(model.indexOf(errors.last()) == errors.size() - 1);
            // CHECK(model.indexOf(QVariant::fromValue(errors.last())) == errors.size() - 1);
        }


        SECTION("Check insert") {
            QList<cwError> insertErrors;

            for(int i = 0; i < size; i++) {
                cwError error;
                error.setMessage(QString("Prepend Error %1").arg(i));
                error.setType(cwError::Fatal);
                error.setErrorTypeId(i*i);

                insertErrors.append(error);
            }

            SECTION("Insert list") {

                model.insert(3, insertErrors);

                REQUIRE(model.size() == size * 2);

                for(int i = 3; i < 3 + size; i++) {
                    checkError(insertErrors.at(i - 3), model, i);
                }
            }

            SECTION("Insert element") {
                model.insert(4, insertErrors.first());

                REQUIRE(model.size() == size + 1);

                checkError(insertErrors.first(), model, 4);
            }
        }

        SECTION("Check remove") {

            SECTION("Remove by index") {
                int removeIndex = 6;
                model.remove(removeIndex);
                CHECK(model.contains(errors.at(removeIndex)) == false);

                for(int i = 0; i < size - 1; i++) {
                    int skipIndex = i < removeIndex ? i : i + 1;
                    checkError(errors.at(skipIndex), model, i);
                }
            }

            SECTION("Remove by variant") {
                int removeIndex = 3;
                model.remove(errors.at(removeIndex));
                CHECK(model.contains(errors.at(removeIndex)) == false);

                for(int i = 0; i < size - 1; i++) {
                    int skipIndex = i < removeIndex ? i : i + 1;
                    checkError(errors.at(skipIndex), model, i);
                }
            }

        }

        SECTION("Get / Set Data") {
            cwSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &, QVector<int>)));

            for(int i = 0; i < size; i++) {
                //Suppression is the only thing that's supported to set
                model.setData(model.index(i), true, model.roleForName("suppressed"));
                CHECK(model.data(model.index(i), model.roleForName("suppressed")) == QVariant(true));
                CHECK(model.at(i).suppressed() == true);


                int errorTypeId = i*5;
                auto index = model.index(i);
                CHECK(model.setData(index, errorTypeId, static_cast<int>(cwErrorListModel::ErrorRoles::ErrorTypeIdRole)) == false);

                CHECK(model.data(model.index(i), model.roleForName("errorTypeId")).toInt() == i);
                CHECK(model.at(i).errorTypeId() == i);
            }

            CHECK(dataChangedSpy.count() == size);
            CHECK(dataChangedSpy.first().last().value<QVector<int>>().first() == model.roleForName("suppressed"));
        }

        SECTION("roleNames") {
            auto roleNames = model.roleNames().values();
            CHECK(roleNames.contains("suppressed") == true);
            CHECK(roleNames.contains("errorTypeId") == true);
            CHECK(roleNames.contains("message") == true);
            CHECK(roleNames.contains("errorType") == true);
        }
    }
}

