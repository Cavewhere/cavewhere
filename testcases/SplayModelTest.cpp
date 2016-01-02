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

    SECTION("Check roleNames") {
        QHash<int, QByteArray> roles = model->roleNames();
        CHECK(QString::fromLocal8Bit(roles.value(cwSurveyChunk::StationNameRole)) == QString("nameRole"));
        CHECK(QString::fromLocal8Bit(roles.value(cwSurveyChunk::ShotDistanceRole)) == QString("distanceRole"));
        CHECK(QString::fromLocal8Bit(roles.value(cwSurveyChunk::ShotCompassRole)) == QString("compassRole"));
        CHECK(QString::fromLocal8Bit(roles.value(cwSurveyChunk::ShotBackCompassRole)) == QString("backCompassRole"));
        CHECK(QString::fromLocal8Bit(roles.value(cwSurveyChunk::ShotClinoRole)) == QString("clinoRole"));
        CHECK(QString::fromLocal8Bit(roles.value(cwSurveyChunk::ShotBackClinoRole)) == QString("backClinoRole"));
    }

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

    CHECK(model->columnCount() == 0);

    QHash<int, QVariant> nameLookup;
    nameLookup.insert(0, "A1");
    nameLookup.insert(1, "B2");
    nameLookup.insert(2, "B3");

    QHash<QString, QList<cwShot>> shotLookup;

    QList<cwShot> a1Shots;
    a1Shots.append(shot1);
    a1Shots.append(shot2);
    a1Shots.append(shot3);

    QList<cwShot> b2Shots;
    b2Shots.append(shot1);
    b2Shots.append(shot2);

    QList<cwShot> b3Shots;
    b3Shots.append(shot3);

    shotLookup.insert("A1", a1Shots);
    shotLookup.insert("B2", b2Shots);
    shotLookup.insert("B3", b3Shots);

    SECTION("Check index and parent index") {

        //Check invalid indexes
        CHECK(model->index(-1) == QModelIndex());
        CHECK(model->index(model->rowCount()+1) == QModelIndex());

        //Go through all the station roles
        for(int i = 0; i < model->rowCount(); i++) {
            QModelIndex index = model->index(i);
            CHECK(index != QModelIndex());
            CHECK(index.parent() == QModelIndex());

            QString stationName = index.data(cwSurveyChunk::StationNameRole).toString();

            CHECK(stationName == nameLookup.value(i).toString());
            QList<cwShot> shots = shotLookup.value(stationName);

            REQUIRE(shots.size() == model->rowCount(index));

            //Go through all the splay roles
            for(int ii = 0; ii < model->rowCount(index); ii++) {
                QModelIndex splayIndex = model->index(ii, 0, index);
                CHECK(splayIndex != QModelIndex());
                CHECK(splayIndex.parent() == index);

                cwShot shot = shots.at(ii);
                CHECK(splayIndex.data(cwSurveyChunk::ShotDistanceRole).toDouble() == shot.distance());
                CHECK(splayIndex.data(cwSurveyChunk::ShotCompassRole).toDouble() == shot.compass());
                CHECK(splayIndex.data(cwSurveyChunk::ShotBackCompassRole).toDouble() == shot.backCompass());
                CHECK(splayIndex.data(cwSurveyChunk::ShotClinoRole).toDouble() == shot.clino());
                CHECK(splayIndex.data(cwSurveyChunk::ShotBackClinoRole).toDouble() == shot.backClino());
            }
        }
    }

    SECTION("Check setData / Data") {
        QModelIndex index = model->index(1); //Station B3
        model->setData(index, "c2", cwSurveyChunk::StationNameRole);

        CHECK(index.data(cwSurveyChunk::StationNameRole).toString() == "C2");
        CHECK(index.data(cwSurveyChunk::ShotDistanceRole) == QVariant());
        CHECK(index.data(cwSurveyChunk::ShotCompassRole) == QVariant());
        CHECK(index.data(cwSurveyChunk::ShotBackCompassRole) == QVariant());
        CHECK(index.data(cwSurveyChunk::ShotClinoRole) == QVariant());
        CHECK(index.data(cwSurveyChunk::ShotBackClinoRole) == QVariant());

        QModelIndex shotIndex = model->index(1, 0, index);
        model->setData(shotIndex, 10.2, cwSurveyChunk::ShotDistanceRole);
        model->setData(shotIndex, 12, cwSurveyChunk::ShotCompassRole);
        model->setData(shotIndex, 13, cwSurveyChunk::ShotBackCompassRole);
        model->setData(shotIndex, 14, cwSurveyChunk::ShotClinoRole);
        model->setData(shotIndex, 15, cwSurveyChunk::ShotBackClinoRole);

        CHECK(shotIndex.data(cwSurveyChunk::StationNameRole) ==  QVariant());
        CHECK(shotIndex.data(cwSurveyChunk::ShotDistanceRole).toDouble() == 10.2);
        CHECK(shotIndex.data(cwSurveyChunk::ShotCompassRole).toDouble() == 12);
        CHECK(shotIndex.data(cwSurveyChunk::ShotBackCompassRole).toDouble() == 13);
        CHECK(shotIndex.data(cwSurveyChunk::ShotClinoRole).toDouble() == 14);
        CHECK(shotIndex.data(cwSurveyChunk::ShotBackClinoRole).toDouble() == 15);

        CHECK(dataChangedSpy->size() == 6);
    }

    SECTION("Clear") {
        model->clear();
        CHECK(model->rowCount() == 0);
    }
}

