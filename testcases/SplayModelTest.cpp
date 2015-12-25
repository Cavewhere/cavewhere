/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include "catch.hpp"

//Qt includes
#include <QSignalSpy>

//Our includes
#include "cwSplayModel.h"

TEST_CASE("Splay models should be able to add and remove splays") {

    cwSplayModel* model = new cwSplayModel();

    QSignalSpy* insertSpy = new QSignalSpy(model, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy* removeSpy = new QSignalSpy(model, SIGNAL(rowsRemoved(QModelIndex,int,int)));
    QSignalSpy* dataChangedSpy = new QSignalSpy(model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));

    cwShot shot1;
    shot1.setDistance(3);
    shot1.setCompass(100);
    shot1.setClino(10);

    cwShot shot2 = shot1;
    shot2.setCompass(101);

    cwShot shot3 = shot1;
    shot3.setClino(-5);

    model->appendSplay("A1", shot1);
    model->appendSplay("A1", shot2);
    model->appendSplay("a1", shot3);

    CHECK(model->rowCount() == 1);
    CHECK(insertSpy->size() == 4);

    insertSpy->clear();

    model->appendSplay("b2", shot1);
    model->appendSplay("b2", shot2);
    model->appendSplay("b3", shot3);

    CHECK(model->rowCount() == 3); //3 station
    CHECK(model->rowCount(model->index(0)) == 3);
    CHECK(model->rowCount(model->index(1)) == 2);
    CHECK(model->rowCount(model->index(2)) == 1);
    CHECK(insertSpy->size() == 5);
}

