/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWXMLPROJECTSAVETASK_H
#define CWXMLPROJECTSAVETASK_H

//Our includes
#include "cwRegionIOTask.h"
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
class cwLead;
class cwSurveyNetwork;
class cwProjectedProfileScrapViewMatrix;
class cwDistanceReading;
class cwClinoReading;
class cwCompassReading;
#include "cwReading.h"

//Google protobuffer
namespace CavewhereProto {
    class CavingRegion;
    class Cave;
    class Trip;
    class SurveyNoteModel;
    class TripCalibration;
    class SurveyChunk;
    class Team;
    class Note;
    class Image;
    class Scrap;
    class ImageResolution;
    class NoteStation;
    class NoteTranformation;
    class TriangulatedData;
    class Length;
    class TeamMember;
    class Station;
    class Shot;
    class StationPositionLookup;
    class Lead;
    class SurveyNetwork;
    class ProjectedProfileScrapViewMatrix;
    class DistanceReading;
    class CompassReading;
    class ClinoReading;
    class StationShot;
};

namespace QtProto {
    class QString;
    class QDate;
    class QSizeF;
    class QSize;
    class QPointF;
    class QVector3D;
    class QVector2D;
    class QStringList;
};

class cwRegionSaveTask : public cwRegionIOTask
{
    Q_OBJECT
public:
    explicit cwRegionSaveTask(QObject *parent = 0);

    QByteArray serializedData(cwCavingRegion *region);
    QList<cwError> save(cwCavingRegion *region);


    static void saveCave(CavewhereProto::Cave* protoCave, cwCave* cave);
    static void saveTrip(CavewhereProto::Trip* protoTrip, cwTrip* trip);
    static void saveSurveyNoteModel(CavewhereProto::SurveyNoteModel* protoNoteModel,
                             cwSurveyNoteModel* noteModel);
    static void saveTripCalibration(CavewhereProto::TripCalibration* protoTripCalibration,
                             cwTripCalibration* tripCalibration);
    static void saveSurveyChunk(CavewhereProto::SurveyChunk* protoChunk,
                         cwSurveyChunk* chunk);
    static void saveTeam(CavewhereProto::Team* protoTeam,
                  cwTeam* team);
    static void saveNote(CavewhereProto::Note* protoNote,
                  cwNote* note);
    static void saveImage(CavewhereProto::Image* protoImage,
                   const cwImage& image);
    static void saveScrap(CavewhereProto::Scrap* protoScrap,
                   cwScrap* scrap);
    static void saveImageResolution(CavewhereProto::ImageResolution* protoImageRes,
                             cwImageResolution* imageResolution);
    static void saveNoteStation(CavewhereProto::NoteStation* protoNoteStation,
                         const cwNoteStation& noteStation);
    static void saveNoteTranformation(CavewhereProto::NoteTranformation* protoNoteTransformation,
                               cwNoteTranformation* noteTransformation);
    static void saveTriangulatedData(CavewhereProto::TriangulatedData* protoTriangulatedData,
                              const cwTriangulatedData& triangluatedData);
    static void saveLength(CavewhereProto::Length* protoLength,
                    cwLength* length);
    static void saveTeamMember(CavewhereProto::TeamMember* protoTeamMember,
                        const cwTeamMember& teamMember);
    static void saveStation(CavewhereProto::Station* protoStation,
                     const cwStation& station);
    static void saveShot(CavewhereProto::Shot* protoShot,
                  const cwShot& shot);
    static void saveStationShot(CavewhereProto::StationShot* protoStation,
                                const cwStation& station);
    static void saveStationShot(CavewhereProto::StationShot* protoShot,
                                const cwShot& shot);

    static void saveCavingRegion(CavewhereProto::CavingRegion& protoRegion, cwCavingRegion* region);
    static void saveStationLookup(CavewhereProto::StationPositionLookup* positionLookup,
                           const cwStationPositionLookup& stationLookup);
    static void saveLead(CavewhereProto::Lead* protoLead, const cwLead& lead);
    static void saveSurveyNetwork(CavewhereProto::SurveyNetwork* protoSurveyNetwork,
                           const cwSurveyNetwork& network);
    static void saveProjectedScrapViewMatrix(CavewhereProto::ProjectedProfileScrapViewMatrix* protoViewMatrix,
                                      cwProjectedProfileScrapViewMatrix* viewMatrix);
    // static void saveDistanceReading(CavewhereProto::DistanceReading* protoDistanceReading,
    //                          const cwDistanceReading& reading);
    // static void saveCompassReading(CavewhereProto::CompassReading* protoCompassReading,
    //                         const cwCompassReading& reading);
    // static void saveClinoReading(CavewhereProto::ClinoReading* protoClinoReading,
    //                       const cwClinoReading& reading);

    //Utils
    static void saveString(std::string *protoString, const QString &string);
    static void saveDate(QtProto::QDate* protoDate, QDate date);
    static void saveSize(QtProto::QSize* protoSize, QSize size);
    static void saveSizeF(QtProto::QSizeF* protoSize, QSizeF size);
    static void savePointF(QtProto::QPointF* protoPointF, QPointF point);
    static void saveVector3D(QtProto::QVector3D* protoVector3D, QVector3D vector3D);
    static void saveVector2D(QtProto::QVector2D* protoVector2D, QVector2D vector2D);
    static void saveStringList(QtProto::QStringList* protoStringList, QStringList stringlist);

    template<typename StringFunc, typename State>
    static void saveReading(StringFunc getProtoString, const cwReading& reading, State emptyState) {
        if(reading.state() != static_cast<int>(emptyState)) {
            saveString(getProtoString(), reading.value());
        }
    };


signals:

public slots:

protected:
    void runTask();

private:

    void saveToProtoBuffer(cwCavingRegion* region);

};

#endif // CWXMLPROJECTSAVETASK_H
