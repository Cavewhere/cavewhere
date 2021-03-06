/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWREGIONLOADTASK_H
#define CWREGIONLOADTASK_H

//Our includes
#include "cwRegionIOTask.h"
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwSurveyNoteModel;
class cwTripCalibration;
class cwSurveyChunk;
class cwTeam;
class cwNote;
class cwScrap;
class cwImageResolution;
class cwNoteStation;
class cwNoteTranformation;
class cwLength;
#include "cwTeamMember.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwTriangulatedData.h"
#include "cwImage.h"
#include "cwStationPositionLookup.h"
#include "cwLead.h"
#include "cwRegionLoadResult.h"

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
    class ScrapViewMatrix;
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

class cwRegionLoadTask : public cwRegionIOTask
{
    Q_OBJECT
public:   
    explicit cwRegionLoadTask(QObject *parent = 0);

    QByteArray readSeralizedData();

    void setDeleteOldImages(bool deleteImages);

    cwRegionLoadResult load();

signals:
    void finishedLoading();

public slots:

protected:
    void runTask();

private:
    class LoadData {
    public:
        LoadData() {}
        LoadData(cwCavingRegion* region, int fileVersion) :
            region(region), fileVersion(fileVersion)
        {}

        cwCavingRegionPtr region;
        int fileVersion = 0;
    };

    bool DeleteOldImages = true;

    LoadData loadFromProtoBuffer();
    QByteArray readProtoBufferFromDatabase(bool* okay);

    LoadData loadCavingRegion(const CavewhereProto::CavingRegion& protoRegion);
    void loadCave(const CavewhereProto::Cave& protoCave, cwCave* cave);
    void loadTrip(const CavewhereProto::Trip& protoTrip, cwTrip* trip);
    void loadSurveyNoteModel(const CavewhereProto::SurveyNoteModel& protoNoteModel,
                             cwSurveyNoteModel* noteModel);
    void loadTripCalibration(const CavewhereProto::TripCalibration& protoTripCalibration,
                             cwTripCalibration* tripCalibration);
    void loadSurveyChunk(const CavewhereProto::SurveyChunk& protoChunk,
                         cwSurveyChunk* chunk);
    void loadTeam(const CavewhereProto::Team& protoTeam,
                  cwTeam* team);
    void loadNote(const CavewhereProto::Note& protoNote,
                  cwNote* note);
    cwImage loadImage(const CavewhereProto::Image& protoImage);
    void loadScrap(const CavewhereProto::Scrap& protoScrap,
                   cwScrap* scrap);
    void loadImageResolution(const CavewhereProto::ImageResolution& protoImageRes,
                             cwImageResolution* imageResolution);
    cwNoteStation loadNoteStation(const CavewhereProto::NoteStation& protoNoteStation);
    void loadNoteTranformation(const CavewhereProto::NoteTranformation& protoNoteTransformation,
                               cwNoteTranformation* noteTransformation);
    cwTriangulatedData loadTriangulatedData(const CavewhereProto::TriangulatedData& protoTriangulatedData);
    void loadLength(const CavewhereProto::Length& protoLength,
                    cwLength* length);
    cwTeamMember loadTeamMember(const CavewhereProto::TeamMember& protoTeamMember);
    cwStation loadStation(const CavewhereProto::Station& protoStation);
    cwShot loadShot(const CavewhereProto::Shot& protoShot);
    cwStationPositionLookup loadStationPositionLookup(const CavewhereProto::StationPositionLookup& protoStationLookup);
    cwLead loadLead(const CavewhereProto::Lead& protoLead);
    int loadFileVersion(const CavewhereProto::CavingRegion& protoRegion);

    //Utils
    QString loadString(const QtProto::QString& protoString);
    QDate loadDate(const QtProto::QDate& protoDate);
    QSize loadSize(const QtProto::QSize& protoSize);
    QSizeF loadSizeF(const QtProto::QSizeF &protoSize);
    QPointF loadPointF(const QtProto::QPointF& protoPointF);
    QVector3D loadVector3D(const QtProto::QVector3D& protoVector3D);
    QVector2D loadVector2D(const QtProto::QVector2D& protoVector2D);
    QStringList loadStringList(const QtProto::QStringList& protoStringList);

    template<typename F>
    void runAfterConnected(F func) {
        bool connected = connectToDatabase("loadRegionTask");
        if(connected) {
            //This makes sure that sqlite is clean up after it self
            insureVacuuming();
            func();
        }
        disconnectToDatabase();
    }

//    QString readXMLFromDatabase();
//    bool loadFromBoostSerialization();

    void insureVacuuming();

};

#endif // CWREGIONLOADTASK_H
