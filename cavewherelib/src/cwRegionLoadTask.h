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
#include "cwReadingStates.h"

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
    class DistanceReading;
    class CompassReading;
    class ClinoReading;

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
    cwDistanceReading distanceReading(const CavewhereProto::DistanceReading& protoDistanceReading);
    cwCompassReading compassReading(const CavewhereProto::CompassReading& protoCompassReading);
    cwClinoReading clinoReading(const CavewhereProto::ClinoReading& protoClinoReading);
    int loadFileVersion(const CavewhereProto::CavingRegion& protoRegion);

    //Utils
    QString loadString(const std::string &protoString);
    QString loadLegacyString(const QtProto::QString& protoString);
    QDate loadDate(const QtProto::QDate& protoDate);
    QSize loadSize(const QtProto::QSize& protoSize);
    QSizeF loadSizeF(const QtProto::QSizeF &protoSize);
    QPointF loadPointF(const QtProto::QPointF& protoPointF);
    QVector3D loadVector3D(const QtProto::QVector3D& protoVector3D);
    QVector2D loadVector2D(const QtProto::QVector2D& protoVector2D);
    QStringList loadStringList(const QtProto::QStringList& protoStringList);

    template<auto HasLegacyFunc, auto LegacyGetFunc, auto GetFunc, typename Proto>
    QString loadString(Proto protoObj) {
        if ((protoObj.*HasLegacyFunc)()) {
            // legacy returns something UTF-8–compatible
            const std::string& string = (protoObj.*LegacyGetFunc)().stringdata();
            return QString::fromUtf8(string.c_str(), string.length());
        } else {
            // new field is std::string
            return loadString((protoObj.*GetFunc)());
        }
    }

#define LOAD_STRING(proto, Prop) \
    loadString< \
            &std::decay_t<decltype(proto)>::has_legacy_##Prop, \
            &std::decay_t<decltype(proto)>::legacy_##Prop, \
            &std::decay_t<decltype(proto)>::Prop \
        >(proto)

    template<typename T>
    QString loadName(T protoObj) {
        // LOAD_STRING(protoObj, name);
        return loadString<&T::has_legacy_name,
                          &T::legacy_name,
                          &T::name>
            (protoObj);
    }

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

    //Returns the distance based on functors
    template<typename hasReadingFnV6,
             typename readingFnV6,
             typename stateFnV5,
             typename valueFnV5>
    cwDistanceReading distance(
        hasReadingFnV6 hasReadingV6,
        readingFnV6 readingV6,
        stateFnV5 stateV5,
        valueFnV5 valueV5)
    {
        if (hasReadingV6()) {
            //Version 6
            return distanceReading(readingV6());
        } else {
            //Version 5, fallback
            auto s = static_cast<cwDistanceStates::State>(stateV5());
            switch (s) {
            case cwDistanceStates::Valid: {
                return cwDistanceReading(valueV5());
            }
            case cwDistanceStates::Empty: {
                return cwDistanceReading();
            }
            default: {
                Q_ASSERT(false);
                return cwDistanceReading();
            }
            }
        }
    };

    template<
        typename hasReadingFnV6,
        typename readingFnV6,
        typename stateFnV5,
        typename valueFnV5>
    cwClinoReading clino(
        hasReadingFnV6 hasReadingV6,
        readingFnV6 readingV6,
        stateFnV5 stateV5,
        valueFnV5 valueV5)
    {
         if (hasReadingV6()) {
            //Version 6
            return clinoReading(readingV6());
        } else {
            //Version 5, fallback, use old cwClinoStates
            auto s = static_cast<cwClinoStates::State>(stateV5());
            switch (s) {
            case cwClinoStates::Valid:
                return cwClinoReading(valueV5());
            case cwClinoStates::Empty:
                return cwClinoReading();
            case cwClinoStates::Up:
                return cwClinoReading(QStringLiteral("Up"));
            case cwClinoStates::Down:
                return cwClinoReading(QStringLiteral("Down"));
            default: {
                Q_ASSERT(false);
                return cwClinoReading();
            }
            }
        }
    };

    template<
        typename hasReadingFnV6,
        typename readingFnV6,
        typename stateFnV5,
        typename valueFnV5>
    cwCompassReading compass(
        hasReadingFnV6 hasReadingV6,
        readingFnV6 readingV6,
        stateFnV5 stateV5,
        valueFnV5 valueV5)
    {
        if (hasReadingV6()) {
            //Version 6
            return compassReading(readingV6());
        } else {
            //Version 5, fallback, use old cwClinoStates
            auto s = static_cast<cwCompassStates::State>(stateV5());
            switch (s) {
            case cwCompassStates::Valid:
                return cwCompassReading(valueV5());
            case cwCompassStates::Empty:
                return cwCompassReading();
            default: {
                Q_ASSERT(false);
                return cwCompassReading();
            }
            }
        }
    };



//    QString readXMLFromDatabase();
//    bool loadFromBoostSerialization();

    void insureVacuuming();

};

#endif // CWREGIONLOADTASK_H
