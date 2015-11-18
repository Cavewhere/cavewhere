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
#include "cwErrorListModel.h"

TEST_CASE("Fatal and warning count changing changing") {


    cwErrorModel* parent = new cwErrorModel();
    cwErrorModel* child = new cwErrorModel();
    cwErrorModel* grandChild = new cwErrorModel();

    grandChild->setParentModel(child);
    child->setParentModel(parent);

    QSignalSpy parentFatalSpy(parent, SIGNAL(fatalCountChanged()));
    QSignalSpy parentWarningSpy(parent, SIGNAL(warningCountChanged()));

    int size = 10;
    for(int i = 0; i < size; i++) {
        cwError error;
        error.setMessage(QString("Error: %1").arg(i));
        error.setType(cwError::Fatal);
        grandChild->errors()->append(error);

        CHECK(grandChild->fatalCount() == i + 1);
        CHECK(child->fatalCount() == i + 1);
        CHECK(parent->fatalCount() == i + 1);
    }

    for(int i = 0; i < size; i++) {
        cwError error;
        error.setMessage(QString("Warning: %1").arg(i));
        error.setType(cwError::Warning);
        grandChild->errors()->append(error);

        CHECK(grandChild->warningCount() == i + 1);
        CHECK(child->warningCount() == i + 1);
        CHECK(parent->warningCount() == i + 1);
    }

    CHECK(grandChild->fatalCount() == 10);
    CHECK(child->fatalCount() == 10);
    CHECK(parent->fatalCount() == 10);

    CHECK(grandChild->warningCount() == 10);
    CHECK(child->warningCount() == 10);
    CHECK(parent->warningCount() == 10);

    CHECK(parentFatalSpy.count() == 20);
    CHECK(parentWarningSpy.count() == 20);

    SECTION("Check suppresion changed") {
        grandChild->errors()->setData(grandChild->errors()->index(11), true, grandChild->errors()->roleForName("suppressed"));

        CHECK(grandChild->warningCount() == 9);
        CHECK(grandChild->fatalCount() == 10);

        CHECK(child->warningCount() == 9);
        CHECK(child->fatalCount() == 10);

        CHECK(parent->warningCount() == 9);
        CHECK(parent->fatalCount() == 10);

        CHECK(parentFatalSpy.count() == 21);
        CHECK(parentWarningSpy.count() == 21);
    }
}



///**
// * @brief TEST_CASE
// * If this test falses, errors can't be added and queried from the model
// * correctly
// */
//TEST_CASE("Adding and Quering errors", "[ErrorModel]")
//{
//    QObject* parent = new QObject();
//    QObject* child = new QObject(parent);
//    QObject* grandChild = new QObject(child);

//    cwErrorModel model;

//    //These signal spies record when signals are emitted
//    QSignalSpy parentErrorChangedSpy(&model, SIGNAL(parentErrorsChanged(QObject*)));
//    QSignalSpy errorChangedSpy(&model, SIGNAL(errorsChanged(QObject*,int,int)));

//    int index = 1;
//    int role = 2;

//    cwError error;
//    error.setParent(grandChild);
//    error.setMessage("Best error ever");
//    error.setIndex(index);
//    error.setRole(role);

//    SECTION("Simple add error") {
//        model.addError(error);

//        CHECK(parentErrorChangedSpy.count() == 1);
//        CHECK(errorChangedSpy.count() == 1);

//        QVariantList args;
//        args.append(QVariant::fromValue(grandChild));

//        CHECK(parentErrorChangedSpy.takeFirst() == args);

//        //Append index and role to the args, total args should be parent=grandChild, index=1, role=2
//        args.append(index);
//        args.append(role);

//        CHECK(errorChangedSpy.takeFirst() == args);

//        //Fetch added error
//        QVariantList list1 = model.errors(grandChild);
//        QVariantList list2 = model.errors(grandChild, index);
//        QVariantList list3 = model.errors(grandChild, index, role);

//        CHECK(list1 == list2);
//        CHECK(list2 == list3);
//        CHECK(list1.size() == 1);
//        CHECK(list1.first() == QVariant::fromValue(error));
//        CHECK(list1.first().value<cwError>() == error);
//    }

//    SECTION("Add grandparent") {
//        model.addParent(parent);
//        model.addError(error);

//        QVariantList args1;
//        args1.append(QVariant::fromValue(grandChild));

//        QVariantList args2;
//        args2.append(QVariant::fromValue(parent));

//        REQUIRE(parentErrorChangedSpy.count() == 2);
//        CHECK(parentErrorChangedSpy.at(0) == args1);
//        CHECK(parentErrorChangedSpy.at(1) == args2);

//        CHECK(model.errors(parent).count() == 1);
//    }

//    SECTION("Adding Multiple errors") {
//        int numIndex = 10;
//        int numRoles = 5;
//        int numTypeId = 3;

//        for(int i = 0; i < numIndex; i++) {
//            for(int r = 0; r < numRoles; r++) {
//                for(int t = 0; t < numTypeId; t++) {
//                    cwError error;
//                    error.setParent(grandChild);
//                    error.setMessage(QString("Best error ever index=%1 role=%2 typeId=%3").arg(i).arg(r).arg(t));
//                    error.setIndex(i);
//                    error.setRole(r);
//                    error.setErrorTypeId(t);
//                    model.addError(error);
//                }
//            }
//        }

//        //Fetch added errors
//        QVariantList list1 = model.errors(grandChild);
//        QVariantList list2 = model.errors(grandChild, 4);
//        QVariantList list3 = model.errors(grandChild, 4, 2);

//        REQUIRE(list1.count() == numIndex * numRoles * numTypeId);
//        REQUIRE(list2.count() == numRoles * numTypeId);
//        REQUIRE(list3.count() == numTypeId);

//        //Check list1 content
//        int count = 0;
//        for(int i = 0; i < numIndex; i++) {
//            for(int r = 0; r < numRoles; r++) {
//                for(int t = 0; t < numTypeId; t++) {
//                    cwError error;
//                    error.setParent(grandChild);
//                    error.setMessage(QString("Best error ever index=%1 role=%2 typeId=%3").arg(i).arg(r).arg(t));
//                    error.setIndex(i);
//                    error.setRole(r);
//                    error.setErrorTypeId(t);

//                    CHECK(error == list1.at(count).value<cwError>());
//                    count++;
//                }
//            }
//        }

//        //Check list2 contents
//        count = 0;
//        for(int r = 0; r < numRoles; r++) {
//            for(int t = 0; t < numTypeId; t++) {
//                cwError error;
//                error.setParent(grandChild);
//                error.setMessage(QString("Best error ever index=%1 role=%2 typeId=%3").arg(4).arg(r).arg(t));
//                error.setIndex(4);
//                error.setRole(r);
//                error.setErrorTypeId(t);

//                CHECK(error == list2.at(count).value<cwError>());
//                count++;
//            }
//        }

//        //Check list3 contents
//        count = 0;
//        for(int t = 0; t < numTypeId; t++) {
//            cwError error;
//            error.setParent(grandChild);
//            error.setMessage(QString("Best error ever index=%1 role=%2 typeId=%3").arg(4).arg(2).arg(t));
//            error.setIndex(4);
//            error.setRole(2);
//            error.setErrorTypeId(t);

//            CHECK(error == list3.at(count).value<cwError>());
//            count++;
//        }
//    }

//    delete parent;
//}

///**
//  Tests the error suppression in the model
//  */
//TEST_CASE("Setting the suppression for a error in the ErrorModel", "[ErrorModel]") {
//    QObject* parent = new QObject();
//    QObject* child = new QObject(parent);
//    QObject* grandChild = new QObject(child);

//    cwErrorModel model;

//    int index = 1;
//    int role = 2;

//    cwError error;
//    error.setParent(grandChild);
//    error.setMessage("Best error ever");
//    error.setIndex(index);
//    error.setRole(role);
//    error.setType(cwError::Fatal);

//    cwError error2;
//    error2.setParent(grandChild);
//    error2.setMessage("Best error ever");
//    error2.setIndex(2);
//    error2.setRole(3);
//    error2.setType(cwError::Warning);

//    model.addError(error);
//    model.addError(error2);

//    //These signal spies record when signals are emitted
//    QSignalSpy parentErrorChangedSpy(&model, SIGNAL(parentErrorsChanged(QObject*)));
//    QSignalSpy errorChangedSpy(&model, SIGNAL(errorsChanged(QObject*,int,int)));

//    model.setSuppressForError(error2, true);
//    REQUIRE(model.errors(grandChild, 2, 3).size() == 1);
//    CHECK(model.errors(grandChild, 2, 3).first().value<cwError>().suppressed() == true);

//    //Can't suppress a fatal error
//    model.setSuppressForError(error, true);
//    REQUIRE(model.errors(grandChild, 1, 2).size() == 1);
//    CHECK(model.errors(grandChild, 1, 2).first().value<cwError>().suppressed() == false);

//    CHECK(parentErrorChangedSpy.count() == 1);
//    REQUIRE(errorChangedSpy.count() == 1);
//    REQUIRE(errorChangedSpy.first().count() == 3);
//    CHECK(errorChangedSpy.first().at(0) == QVariant::fromValue(grandChild));
//    CHECK(errorChangedSpy.first().at(1) == QVariant::fromValue(2));
//    CHECK(errorChangedSpy.first().at(2) == QVariant::fromValue(3));
//}

