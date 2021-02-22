/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch.hpp>

//Our includes
#include "cwError.h"
#include "cwErrorListModel.h"

//Qt includes
#include <QSignalSpy>
#include <QQmlGadgetListModel.h>

void checkError(const cwError& error, cwErrorListModel& model, int idx) {
    REQUIRE(idx >= 0);
    REQUIRE(idx < model.count());

    CHECK(error == model.at(idx));
    CHECK(error == model.get(idx).value<cwError>());

    CHECK(QVariant(error.message()) == model.data(model.index(idx), model.roleForName("message")));
    CHECK(QVariant(error.errorTypeId()) == model.data(model.index(idx), model.roleForName("errorTypeId")));
    CHECK(QVariant(error.suppressed()) == model.data(model.index(idx), model.roleForName("suppressed")));
    CHECK(QVariant(error.type()) == model.data(model.index(idx), model.roleForName("type")));

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
            CHECK(model.contains(QVariant::fromValue(errors.first())) == true);

            CHECK(model.contains(errors.last()) == true);
            CHECK(model.contains(QVariant::fromValue(errors.last())) == true);
        }

        SECTION("Check indexOf") {
            CHECK(model.indexOf(errors.first()) == 0);
            CHECK(model.indexOf(QVariant::fromValue(errors.first())) == 0);

            CHECK(model.indexOf(errors.last()) == errors.size() - 1);
            CHECK(model.indexOf(QVariant::fromValue(errors.last())) == errors.size() - 1);
        }

        SECTION("Check prepend") {
            QList<cwError> prependErrors;

            for(int i = 0; i < size; i++) {
                cwError error;
                error.setMessage(QString("Prepend Error %1").arg(i));
                error.setType(cwError::Fatal);
                error.setErrorTypeId(i*i);

                prependErrors.append(error);
            }

            SECTION("Prepend list") {

                model.prepend(prependErrors);

                REQUIRE(model.size() == size * 2);

                for(int i = 0; i < size; i++) {
                    checkError(prependErrors.at(i), model, i);
                }
            }

            SECTION("Prepend element") {
                model.prepend(prependErrors.first());

                REQUIRE(model.size() == size + 1);

                checkError(prependErrors.first(), model, 0);
            }

            SECTION("Prepend variant") {
                model.prepend(QVariant::fromValue(prependErrors.first()));

                REQUIRE(model.size() == size + 1);

                checkError(prependErrors.first(), model, 0);
            }
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

            SECTION("Insert variant") {
                model.insert(10, QVariant::fromValue(insertErrors.first()));

                REQUIRE(model.size() == size + 1);

                checkError(insertErrors.first(), model, model.count() - 1);
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

            SECTION("Remove by item") {
                int removeIndex = 2;
                model.remove(QVariant::fromValue(errors.at(removeIndex)));
                CHECK(model.contains(errors.at(removeIndex)) == false);

                for(int i = 0; i < size - 1; i++) {
                    int skipIndex = i < removeIndex ? i : i + 1;
                    checkError(errors.at(skipIndex), model, i);
                }
            }

        }

        SECTION("Check first") {
            CHECK(errors.first() == model.first());
        }

        SECTION("Check last") {
            CHECK(errors.last() == model.last());
        }

        SECTION("Check toVarArray") {
            CHECK(errors == model.toList());

            QVariantList list;
            foreach(cwError error, errors) {
                list.append(QVariant::fromValue(error));
            }

            CHECK(list == model.toVarArray());
        }

        SECTION("Get / Set Data") {
            QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &, QVector<int>)));

            for(int i = 0; i < size; i++) {
                model.setData(model.index(i), true, model.roleForName("suppressed"));

                CHECK(model.data(model.index(i), model.roleForName("suppressed")) == QVariant(true));
                CHECK(model.data(i, "suppressed") == true);
                CHECK(model.at(i).suppressed() == true);


                int errorTypeId = i*5;
                model.setData(i, errorTypeId, "errorTypeId");

                CHECK(model.data(model.index(i), model.roleForName("errorTypeId")) == errorTypeId);
                CHECK(model.data(i, "errorTypeId") == errorTypeId);
                CHECK(model.at(i).errorTypeId() == errorTypeId);
            }

            CHECK(dataChangedSpy.count() == size * 2);
            CHECK(dataChangedSpy.first().last().value<QVector<int>>().first() == model.roleForName("suppressed"));
        }

        SECTION("Check Replace") {
            QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &, QVector<int>)));

            cwError replaceError;
            replaceError.setMessage("Replace error 1");
            replaceError.setType(cwError::Warning);

            SECTION("C++ replace") {
                model.replace(2, replaceError);

                CHECK(model.at(2) == replaceError);
                REQUIRE(dataChangedSpy.count() == 1);

                auto roles = dataChangedSpy.first().last().value<QVector<int>>();
                REQUIRE(roles.size() == model.roleNames().size()); //Changes to all 4 properties

                foreach(int role, roles) {
                    CHECK(model.roleNames().contains(role) == true);
                }
            }

            SECTION("QML replace") {
                model.replace(2, QVariant::fromValue(replaceError));

                CHECK(model.at(2) == replaceError);
                REQUIRE(dataChangedSpy.count() == 1);
                auto roles = dataChangedSpy.first().last().value<QVector<int>>();
                REQUIRE(roles.size() == model.roleNames().size()); //Changes to all 4 properties

                foreach(int role, roles) {
                    CHECK(model.roleNames().contains(role) == true);
                }
            }
        }

        SECTION("roleNames") {
            auto roleNames = model.roleNames().values();
            CHECK(roleNames.contains("type") == true);
            CHECK(roleNames.contains("errorTypeId") == true);
            CHECK(roleNames.contains("message") == true);
            CHECK(roleNames.contains("type") == true);
        }
    }
}

