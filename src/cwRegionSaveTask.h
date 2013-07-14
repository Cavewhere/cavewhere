/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWXMLPROJECTSAVETASK_H
#define CWXMLPROJECTSAVETASK_H

//Our includes
#include <cwRegionIOTask.h>
class cwCave;
class cwTrip;
class cwSurveyNoteModel;
class cwTripCalibration;
class cwSurveyChunk;
class cwTeam;
class cwNote;
class cwImage;
class cwScrap;
class cwImageResolution;
class cwNoteStation;
class cwNoteTranformation;
class cwTriangulatedData;
class cwLength;
class cwTeamMember;
class cwStation;
class cwShot;
class cwStationPositionLookup;

//Google protobuffer
#include "cavewhere.pb.h"
#include "qt.pb.h"

class cwRegionSaveTask : public cwRegionIOTask
{
    Q_OBJECT
public:
    explicit cwRegionSaveTask(QObject *parent = 0);

signals:

public slots:

protected:
    void runTask();

private:

    void saveToProtoBuffer();
    void saveCave(CavewhereProto::Cave* protoCave, cwCave* cave);
    void saveTrip(CavewhereProto::Trip* protoTrip, cwTrip* trip);
    void saveSurveyNoteModel(CavewhereProto::SurveyNoteModel* protoNoteModel,
                             cwSurveyNoteModel* noteModel);
    void saveTripCalibration(CavewhereProto::TripCalibration* protoTripCalibration,
                             cwTripCalibration* tripCalibration);
    void saveSurveyChunk(CavewhereProto::SurveyChunk* protoChunk,
                         cwSurveyChunk* chunk);
    void saveTeam(CavewhereProto::Team* protoTeam,
                  cwTeam* team);
    void saveNote(CavewhereProto::Note* protoNote,
                  cwNote* note);
    void saveImage(CavewhereProto::Image* protoImage,
                   const cwImage& image);
    void saveScrap(CavewhereProto::Scrap* protoScrap,
                   cwScrap* scrap);
    void saveImageResolution(CavewhereProto::ImageResolution* protoImageRes,
                             cwImageResolution* imageResolution);
    void saveNoteStation(CavewhereProto::NoteStation* protoNoteStation,
                         const cwNoteStation& noteStation);
    void saveNoteTranformation(CavewhereProto::NoteTranformation* protoNoteTransformation,
                               cwNoteTranformation* noteTransformation);
    void saveTriangulatedData(CavewhereProto::TriangulatedData* protoTriangulatedData,
                              const cwTriangulatedData& triangluatedData);
    void saveLength(CavewhereProto::Length* protoLength,
                    cwLength* length);
    void saveTeamMember(CavewhereProto::TeamMember* protoTeamMember,
                        const cwTeamMember& teamMember);
    void saveStation(CavewhereProto::Station* protoStation,
                     const cwStation& station);
    void saveShot(CavewhereProto::Shot* protoShot,
                  const cwShot& shot);
    void saveCavingRegion(CavewhereProto::CavingRegion& region);
    void saveStationLookup(CavewhereProto::StationPositionLookup* positionLookup,
                           const cwStationPositionLookup& stationLookup);

    //Utils
    void saveString(QtProto::QString* protoString, QString string);
    void saveDate(QtProto::QDate* protoDate, QDate date);
    void saveSize(QtProto::QSize* protoSize, QSize size);
    void saveSizeF(QtProto::QSizeF* protoSize, QSizeF size);
    void savePointF(QtProto::QPointF* protoPointF, QPointF point);
    void saveVector3D(QtProto::QVector3D* protoVector3D, QVector3D vector3D);
    void saveVector2D(QtProto::QVector2D* protoVector2D, QVector2D vector2D);
    void saveStringList(QtProto::QStringList* protoStringList, QStringList stringlist);

    //TODO: Remove old boost serialization
    void xmlSerialization();
    void writeXMLToDatabase(QString xml);
};

#endif // CWXMLPROJECTSAVETASK_H
