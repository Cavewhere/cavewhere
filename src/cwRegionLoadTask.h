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

//Google protobuffer
#include "cavewhere.pb.h"
#include "qt.pb.h"

class cwRegionLoadTask : public cwRegionIOTask
{
    Q_OBJECT
public:
    explicit cwRegionLoadTask(QObject *parent = 0);

signals:
    void finishedLoading(cwCavingRegion* region);

public slots:

protected:
    void runTask();

private:
    bool loadFromProtoBuffer();
    QByteArray readProtoBufferFromDatabase();

    void loadCavingRegion(const CavewhereProto::CavingRegion& region);
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


    //Utils
    QString loadString(const QtProto::QString& protoString);
    QDate loadDate(const QtProto::QDate& protoDate);
    QSize loadSize(const QtProto::QSize& protoSize);
    QPointF loadPointF(const QtProto::QPointF& protoPointF);
    QVector3D loadVector3D(const QtProto::QVector3D& protoVector3D);
    QVector2D loadVector2D(const QtProto::QVector2D& protoVector2D);
    QStringList loadStringList(const QtProto::QStringList& protoStringList);


    QString readXMLFromDatabase();
    bool loadFromBoostSerialization();

    void insureVacuuming();


};

#endif // CWREGIONLOADTASK_H
