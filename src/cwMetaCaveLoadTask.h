/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWMETACAVELOADTASK_H
#define CWMETACAVELOADTASK_H

//Our includes
#include <cwRegionIOTask.h>
class cwCave;
class cwTrip;
class cwTeam;
class cwTeamMember;
class cwTripCalibration;
class cwSurveyChunk;
class cwStation;
class cwShot;

class cwMetaCaveLoadTask : public cwRegionIOTask
{
public:
    cwMetaCaveLoadTask();

protected:
    void runTask();

private:
    void loadCavingRegion(const QVariantMap& map);
    void loadCave(const QVariantMap& map, cwCave* cave);
    void loadTrip(const QVariantMap& map, cwTrip* trip);
//    void loadSurveyNoteModel(const CavewhereProto::SurveyNoteModel& protoNoteModel,
//                             cwSurveyNoteModel* noteModel);
    void loadTripCalibration(const QVariantMap& map, cwTripCalibration* tripCalibration);
//    void loadSurveyChunk(const CavewhereProto::SurveyChunk& protoChunk,
//                         cwSurveyChunk* chunk);
    void loadTeam(const QVariantMap& map, cwTeam* team);
//    void loadNote(const CavewhereProto::Note& protoNote,
//                  cwNote* note);
//    cwImage loadImage(const CavewhereProto::Image& protoImage);
//    void loadScrap(const CavewhereProto::Scrap& protoScrap,
//                   cwScrap* scrap);
//    void loadImageResolution(const CavewhereProto::ImageResolution& protoImageRes,
//                             cwImageResolution* imageResolution);
//    cwNoteStation loadNoteStation(const CavewhereProto::NoteStation& protoNoteStation);
//    void loadNoteTranformation(const CavewhereProto::NoteTranformation& protoNoteTransformation,
//                               cwNoteTranformation* noteTransformation);
//    cwTriangulatedData loadTriangulatedData(const CavewhereProto::TriangulatedData& protoTriangulatedData);
//    void loadLength(const CavewhereProto::Length& protoLength,
//                    cwLength* length);
//    cwTeamMember loadTeamMember(const CavewhereProto::TeamMember& protoTeamMember);
//    cwStation loadStation(const CavewhereProto::Station& protoStation);
//    cwShot loadShot(const CavewhereProto::Shot& protoShot);
//    cwStationPositionLookup loadStationPositionLookup(const CavewhereProto::StationPositionLookup& protoStationLookup);
//    cwLead loadLead(const CavewhereProto::Lead& protoLead);

//    //Utils
//    QString loadString(const QtProto::QString& protoString);
//    QDate loadDate(const QtProto::QDate& protoDate);
//    QSize loadSize(const QtProto::QSize& protoSize);
//    QSizeF loadSizeF(const QtProto::QSizeF &protoSize);
//    QPointF loadPointF(const QtProto::QPointF& protoPointF);
//    QVector3D loadVector3D(const QtProto::QVector3D& protoVector3D);
//    QVector2D loadVector2D(const QtProto::QVector2D& protoVector2D);
//    QStringList loadStringList(const QtProto::QStringList& protoStringList);

    void hasProperty(const QVariantMap& map, QString property, QString structureLocation = QString()) const;

};

#endif // CWMETACAVELOADTASK_H
