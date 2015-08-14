/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include "catch.hpp"

//Our includes
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwProject.h"

//Qt includes
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

TEST_CASE( "cavewhere can be saved and loaded again", "[saveAndLoad]" ) {

    cwStation a1("a1");
    a1.setLeft(1.0);
    a1.setRight(2.0);
    a1.setUp(3.0);
    a1.setDown(4.0);

    cwStation a2("a2");
    a2.setLeft(5.0);
    a2.setRight(6.0);
    a2.setUp(7.0);
    a2.setDown(8.0);

    cwShot shot;
    shot.setClino(10);
    shot.setBackClino(-10);
    shot.setCompass(45);
    shot.setBackCompass(-45);
    shot.setDistance(50);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    chunk->appendShot(a1, a2, shot);

    cwTrip* trip = new cwTrip();
    trip->addChunk(chunk);

    cwCave* cave = new cwCave();
    cave->setName("Test Cave");
    cave->addTrip(trip);

    cwProject* project = new cwProject();
    project->cavingRegion()->addCave(cave);

    SECTION( "save caving region" ) {

        QString filename("LoadSaveTest.cw");
        QFile::remove(filename);

        project->saveAs(filename);

        QFileInfo fileInfo(filename);
        qDebug() << "Filename:" << fileInfo.absoluteFilePath() << fileInfo.isReadable();
        REQUIRE(fileInfo.isReadable() == true);
        REQUIRE(fileInfo.isFile() == true);

        QFile file(filename);
        REQUIRE(file.open(QFile::ReadOnly) == true);
        QByteArray fileContent = file.readAll();

        QJsonParseError parserError;
        QJsonDocument document = QJsonDocument::fromJson(fileContent, &parserError);
        REQUIRE(parserError.error == QJsonParseError::NoError);
    }

    SECTION( "load caving region") {

    }
}

