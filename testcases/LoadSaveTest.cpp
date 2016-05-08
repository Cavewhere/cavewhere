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
#include "cwTripCalibration.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwUnitValue.h"
#include "cwImageResolution.h"
#include "cwTeam.h"
#include "cwTeamMember.h"
#include "cwLength.h"

//Qt includes
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QThreadPool>
#include <QMetaObject>
#include <QMetaProperty>
#include <QApplication>

void compareUnitValue(const cwUnitValue* load, const cwUnitValue* save) {
    REQUIRE(load->value() == save->value());
    REQUIRE(load->unit() == save->unit());
    REQUIRE(load->isUpdatingValue() == save->isUpdatingValue());
}

void compareNoteTransform(const cwNoteTranformation* load, const cwNoteTranformation* save) {
    REQUIRE(load->scale() == save->scale());
    REQUIRE(load->northUp() == save->northUp());
    compareUnitValue(load->scaleNumerator(), save->scaleNumerator());
    compareUnitValue(load->scaleDenominator(), save->scaleDenominator());
}

void compareScrap(const cwScrap* loadScrap, const cwScrap* saveScrap) {
    compareNoteTransform(loadScrap->noteTransformation(), saveScrap->noteTransformation());

    REQUIRE(loadScrap->points() == saveScrap->points());
    REQUIRE(loadScrap->stations() == saveScrap->stations());
    REQUIRE(loadScrap->leads() == saveScrap->leads());
}

void compareImage(const cwImage& loadImage, const cwImage& saveImage) {
    REQUIRE(loadImage.origianlSize() == saveImage.origianlSize());
    REQUIRE(loadImage.originalChecksum().toStdString() == saveImage.originalChecksum().toStdString());
    REQUIRE(loadImage.originalDotsPerMeter() == saveImage.originalDotsPerMeter());
    REQUIRE(loadImage.original() == saveImage.original());
    REQUIRE(loadImage.originalFilename() == saveImage.originalFilename());
    REQUIRE(loadImage.numberOfMipmapLevels() == saveImage.numberOfMipmapLevels());
    REQUIRE(loadImage.icon() == saveImage.icon());
    REQUIRE(loadImage.mipmaps() == saveImage.mipmaps());
    REQUIRE(loadImage.isValid() == saveImage.isValid());
}

void compareNote(const cwNote* loadNote, const cwNote* saveNote) {
    compareImage(loadNote->image(), saveNote->image());
    compareUnitValue(loadNote->imageResolution(), saveNote->imageResolution());

    REQUIRE(loadNote->rotate() == saveNote->rotate());
    REQUIRE(loadNote->scraps().size() == saveNote->scraps().size());

    for(int i = 0; i < loadNote->scraps().size(); i++) {
        cwScrap* loadScrap = loadNote->scraps().at(i);
        cwScrap* saveScrap = saveNote->scraps().at(i);

        compareScrap(loadScrap, saveScrap);
    }
}

void compareNotes(const cwSurveyNoteModel* loadNotes, const cwSurveyNoteModel* saveNotes) {
    REQUIRE(loadNotes->rowCount() == saveNotes->rowCount());

    for(int i = 0; i < loadNotes->rowCount(); i++) {
        cwNote* loadNote = loadNotes->notes().at(i);
        cwNote* saveNote = loadNotes->notes().at(i);

        compareNote(loadNote, saveNote);
    }
}

void compareCalibration(const cwTripCalibration* loadCalibration,
                        const cwTripCalibration* saveCalibration)
{
    Q_UNUSED(loadCalibration);
    Q_UNUSED(saveCalibration);

    for(int i = 0; i < loadCalibration->metaObject()->propertyCount(); i++) {
        QMetaProperty property = loadCalibration->metaObject()->property(i);

        QVariant loadVariant = loadCalibration->property(property.name());
        QVariant saveVariant = saveCalibration->property(property.name());

        INFO(property.name());
        REQUIRE(loadVariant.toString().toStdString() == saveVariant.toString().toStdString());
    }
}

void compareStation(const cwStation& load, const cwStation& save) {
    REQUIRE(load.name() == save.name());
    REQUIRE(load.left() == save.left());
    REQUIRE(load.right() == save.right());
    REQUIRE(load.up() == save.up());
    REQUIRE(load.down() == save.down());
}

void compareShot(const cwShot& load, const cwShot& save) {
    REQUIRE(load.distance() == save.distance());
    REQUIRE(load.distanceState() == save.distanceState());
    REQUIRE(load.compass() == save.compass());
    REQUIRE(load.compassState() == save.compassState());
    REQUIRE(load.backCompass() == save.backCompass());
    REQUIRE(load.backCompassState() == save.backCompassState());
    REQUIRE(load.clino() == save.clino());
    REQUIRE(load.clinoState() == save.clinoState());
    REQUIRE(load.backClino() == save.backClino());
    REQUIRE(load.backClinoState() == save.backClinoState());
    REQUIRE(load.isDistanceIncluded() == save.isDistanceIncluded());
    REQUIRE(load.isValid() == save.isValid());
}

void compareChunk(const cwSurveyChunk* loadChunk,
                  const cwSurveyChunk* saveChunk)
{
    REQUIRE(loadChunk->shots().size() == saveChunk->shots().size());
    REQUIRE(loadChunk->stations().size() == saveChunk->shots().size());

    for(int i = 0; i < loadChunk->shots().size(); i++) {
        cwShot load = loadChunk->shots().at(i);
        cwShot save = saveChunk->shots().at(i);
        compareShot(load, save);
    }

    for(int i = 0; i < loadChunk->stations().size(); i++) {
        cwStation load = loadChunk->stations().at(i);
        cwStation save = loadChunk->stations().at(i);
        compareStation(load, save);
    }
}

void compareTeam(const cwTeam* loadTeam, const cwTeam* saveTeam) {
    REQUIRE(loadTeam->teamMembers() == saveTeam->teamMembers());
}

void compareTrips(const cwTrip* loadTrip, const cwTrip* saveTrip) {
    REQUIRE(loadTrip->name() == saveTrip->name());
    REQUIRE(loadTrip->date() == saveTrip->date());

    compareNotes(loadTrip->notes(), saveTrip->notes());
    compareCalibration(loadTrip->calibrations(), saveTrip->calibrations());

    REQUIRE(loadTrip->numberOfChunks() == saveTrip->numberOfChunks());

    for(int i = 0; i < loadTrip->numberOfChunks(); i++) {
        cwSurveyChunk* loadChunk = loadTrip->chunks().at(i);
        cwSurveyChunk* saveChunk = saveTrip->chunks().at(i);

        compareChunk(loadChunk, saveChunk);
    }
}

void compareCaves(const cwCave* loadCave, const cwCave* saveCave) {
    REQUIRE(loadCave->name() == saveCave->name());
    REQUIRE(loadCave->tripCount() == saveCave->tripCount());

    for(int i = 0; i < loadCave->tripCount(); i++) {
        cwTrip* loadTrip = loadCave->trip(i);
        cwTrip* saveTrip = saveCave->trip(i);

        compareTrips(loadTrip, saveTrip);
    }

    REQUIRE(loadCave->stationPositionLookup().positions() == saveCave->stationPositionLookup().positions());
}

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

    QString filename("LoadSaveTest.cw");

    QDir dir;
    qDebug() << "FilePath:" << dir.absoluteFilePath(filename);


    SECTION( "save caving region" ) {

        QFile::remove(filename);

        project->saveAs(filename);

        //Saving is async, wait until the thread are finished
        project->saveWaitToFinish();

        QFileInfo fileInfo(filename);
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

        cwProject* loadProject = new cwProject();
        loadProject->loadFile(filename);

        //Loaded is async, wait until the thread finishes
        loadProject->loadWaitToFinish();

        cwCavingRegion* loadRegion = loadProject->cavingRegion();

        REQUIRE(loadRegion->caveCount() == project->cavingRegion()->caveCount());

        for(int i = 0; i < loadRegion->caveCount(); i++) {
            cwCave* loadCave = loadRegion->cave(i);
            cwCave* saveCave = project->cavingRegion()->cave(i);

            compareCaves(loadCave, saveCave);
        }
    }
}

