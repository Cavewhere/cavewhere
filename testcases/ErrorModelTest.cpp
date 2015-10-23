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
#include <QSignalSpy>
#include <QDebug>

//Our includes
#include "cwError.h"
#include "cwErrorModel.h"


/**
 * @brief TEST_CASE
 * If this test falses, errors can't be added and queried from the model
 * correctly
 */
TEST_CASE("Adding and Quering errors", "[ErrorModel]")
{
    QObject* parent = new QObject();
    QObject* child = new QObject(parent);
    QObject* grandChild = new QObject(child);

    cwErrorModel model;

    //These signal spies record when signals are emitted
    QSignalSpy parentErrorChangedSpy(&model, SIGNAL(parentErrorsChanged(QObject*)));
    QSignalSpy errorChangedSpy(&model, SIGNAL(errorsChanged(QObject*,int,int)));

    int index = 1;
    int role = 2;

    cwError error;
    error.setParent(grandChild);
    error.setMessage("Best error ever");
    error.setIndex(index);
    error.setRole(role);

    SECTION("Simple add error") {
        model.addError(error);

        CHECK(parentErrorChangedSpy.count() == 1);
        CHECK(errorChangedSpy.count() == 1);

        QVariantList args;
        args.append(QVariant::fromValue(grandChild));

        CHECK(parentErrorChangedSpy.takeFirst() == args);

        //Append index and role to the args, total args should be parent=grandChild, index=1, role=2
        args.append(index);
        args.append(role);

        CHECK(errorChangedSpy.takeFirst() == args);

        //Fetch added error
        QVariantList list1 = model.errors(grandChild);
        QVariantList list2 = model.errors(grandChild, index);
        QVariantList list3 = model.errors(grandChild, index, role);

        CHECK(list1 == list2);
        CHECK(list2 == list3);
        CHECK(list1.size() == 1);
        CHECK(list1.first() == QVariant::fromValue(error));
        CHECK(list1.first().value<cwError>() == error);
    }

    SECTION("Add grandparent") {
        model.addParent(parent);
        model.addError(error);

        QVariantList args1;
        args1.append(QVariant::fromValue(grandChild));

        QVariantList args2;
        args2.append(QVariant::fromValue(parent));

        REQUIRE(parentErrorChangedSpy.count() == 2);
        CHECK(parentErrorChangedSpy.at(0) == args1);
        CHECK(parentErrorChangedSpy.at(1) == args2);
    }

    SECTION("Adding Multiple errors") {
        int numIndex = 10;
        int numRoles = 5;
        int numTypeId = 3;

        for(int i = 0; i < numIndex; i++) {
            for(int r = 0; r < numRoles; r++) {
                for(int t = 0; t < numTypeId; t++) {
                    cwError error;
                    error.setParent(grandChild);
                    error.setMessage(QString("Best error ever index=%1 role=%2 typeId=%3").arg(i).arg(r).arg(t));
                    error.setIndex(i);
                    error.setRole(r);
                    error.setErrorTypeId(t);
                    model.addError(error);
                }
            }
        }

        //Fetch added errors
        QVariantList list1 = model.errors(grandChild);
        QVariantList list2 = model.errors(grandChild, 4);
        QVariantList list3 = model.errors(grandChild, 4, 2);

        REQUIRE(list1.count() == numIndex * numRoles * numTypeId);
        REQUIRE(list2.count() == numRoles * numTypeId);
        REQUIRE(list3.count() == numTypeId);

        //Check list1 content
        int count = 0;
        for(int i = 0; i < numIndex; i++) {
            for(int r = 0; r < numRoles; r++) {
                for(int t = 0; t < numTypeId; t++) {
                    cwError error;
                    error.setParent(grandChild);
                    error.setMessage(QString("Best error ever index=%1 role=%2 typeId=%3").arg(i).arg(r).arg(t));
                    error.setIndex(i);
                    error.setRole(r);
                    error.setErrorTypeId(t);

                    CHECK(error == list1.at(count).value<cwError>());
                    count++;
                }
            }
        }

        //Check list2 contents
        count = 0;
        for(int r = 0; r < numRoles; r++) {
            for(int t = 0; t < numTypeId; t++) {
                cwError error;
                error.setParent(grandChild);
                error.setMessage(QString("Best error ever index=%1 role=%2 typeId=%3").arg(4).arg(r).arg(t));
                error.setIndex(4);
                error.setRole(r);
                error.setErrorTypeId(t);

                CHECK(error == list2.at(count).value<cwError>());
                count++;
            }
        }

        //Check list3 contents
        count = 0;
        for(int t = 0; t < numTypeId; t++) {
            cwError error;
            error.setParent(grandChild);
            error.setMessage(QString("Best error ever index=%1 role=%2 typeId=%3").arg(4).arg(2).arg(t));
            error.setIndex(4);
            error.setRole(2);
            error.setErrorTypeId(t);

            CHECK(error == list3.at(count).value<cwError>());
            count++;
        }
    }


    delete parent;
}


