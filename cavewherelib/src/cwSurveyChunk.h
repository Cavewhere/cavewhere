/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSurveyChunk_H
#define CWSurveyChunk_H

//Our includes
//#include "cwStationReference.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwError.h"
#include "cwGlobals.h"
#include "cwSurveyChunkData.h"
class cwErrorModel;
class cwTrip;
class cwCave;
class cwTripCalibration;

//Qt
#include <QObject>
#include <QList>
//#include <QDeclarativeListProperty>
#include <QVariant>
#include <QQmlEngine>

class CAVEWHERE_LIB_EXPORT cwSurveyChunk : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyChunk)

    Q_PROPERTY(cwTrip* parentTrip READ parentTrip WRITE setParentTrip NOTIFY parentTripChanged)
    Q_PROPERTY(cwErrorModel* errorModel READ errorModel CONSTANT)

    Q_PROPERTY(int stationCount READ stationCount NOTIFY stationCountChanged FINAL)
    Q_PROPERTY(int shotCount READ shotCount NOTIFY shotCountChanged FINAL)

public:
    enum Direction {
        Above,
        Below
    };

    enum DataRole {
        StationNameRole,
        StationLeftRole,
        StationRightRole,
        StationUpRole,
        StationDownRole,
        ShotDistanceRole,
        ShotDistanceIncludedRole,
        ShotCompassRole,
        ShotBackCompassRole,
        ShotClinoRole,
        ShotBackClinoRole
    };

    //If the chunk is connected to the rest of the cave, Connected
    enum ConnectedState {
        Connected, //Connected to the rest of the cave
        Disconnected, //Not connected, the survey is just floating
        Unknown //State isn't set
    };
    Q_ENUM(DataRole)
    Q_ENUM(ConnectedState)
    Q_ENUM(Direction)

    cwSurveyChunk(QObject *parent = 0);

    virtual ~cwSurveyChunk();

    QUuid id() const { return d.id; }
    void setId(const QUuid id) { d.id = id; }

    bool isValid() const;
    bool canAddShot(const cwStation& fromStation, const cwStation& toStation);

    void setParentTrip(cwTrip* trip);
    cwTrip* parentTrip() const;

    cwCave* parentCave() const;

    QList<cwStation> stations() const;
    QList<cwShot> shots() const;

    [[deprecated]]
    void addCalibration(int shotIndex, cwTripCalibration *calibration = nullptr);
    [[deprecated]]
    void removeCalibration(int shotIndex);
    [[deprecated]]
    QMap<int, cwTripCalibration *> calibrations() const;
    [[deprecated]]
    cwTripCalibration* lastCalibration() const;

    bool hasStation(QString stationName) const;
    QSet<cwStation> neighboringStations(QString stationName) const;

    Q_INVOKABLE bool isStationRole(cwSurveyChunk::DataRole role) const;
    Q_INVOKABLE bool isShotRole(cwSurveyChunk::DataRole role) const;

    Q_INVOKABLE QString guessLastStationName() const;
    QString guessNextStation(QString stationName) const;

    void setStation(cwStation station, int index);

    Q_INVOKABLE bool isStationAndShotsEmpty() const;

    cwErrorModel* errorModel() const;

    int stationCount() const;
    int shotCount() const;

    Q_INVOKABLE cwStation station(int index) const;
    Q_INVOKABLE QList<int> indicesOfStation(QString stationName) const;

    Q_INVOKABLE cwShot shot(int index) const;

    Q_INVOKABLE cwErrorModel* errorsAt(int index, DataRole role) const;
    Q_INVOKABLE QVariant data(DataRole role, int index) const;

    cwSurveyChunkData data() const;
    void setData(const cwSurveyChunkData& data);

signals:
    void parentTripChanged();

    void added(int stationBegin, int stationEnd,
               int shotBegin, int shotEnd);
    void aboutToRemove(int stationBegin, int stationEnd,
                       int shotBegin, int shotEnd);
    void removed(int stationBegin, int stationEnd,
                int shotBegin, int shotEnd);

    void stationsAdded(int beginIndex, int endIndex);
    void shotsAdded(int beginIndex, int endIndex);

    void stationsRemoved(int beginIndex, int endIndex);
    void shotsRemoved(int beginIndex, int endIndex);

    void calibrationsChanged();

    void dataChanged(cwSurveyChunk::DataRole mainRole, int index);

    void connectedChanged();

    void connectedStateChanged();

    void errorsChanged(cwSurveyChunk::DataRole mainRole, int index);

    void shotCountChanged();
    void stationCountChanged();

public slots:
    void appendNewShot();
    void appendShot(cwStation fromStation, cwStation toStation, cwShot shot);

    cwSurveyChunk* splitAtStation(int stationIndex);

    void insertStation(int stationIndex, Direction direction);
    void insertShot(int shotIndex, Direction direction);

    void removeStation(int stationIndex, Direction shot);
    bool canRemoveStation(int stationIndex, Direction shot);

    void removeShot(int shotIndex, Direction station);
    bool canRemoveShot(int shotIndex, Direction station);

    void setData(DataRole role, int index, QVariant data);


private:
    class CellIndex {

    public:
        CellIndex() : Index(-1), Role(-1) {}
        CellIndex(int index, int role) : Index(index), Role(role) {}

        bool operator<(const CellIndex& other) const {
            if(Index == other.Index) {
                return Role < other.Role;
            }
            return Index < other.Index;
        }

    private:
        int Index;
        int Role;
    };

    cwSurveyChunkData d;
    // QUuid m_id;
    // QList<cwStation> Stations;
    // QList<cwShot> Shots;

    // Deperate this?! currently not used?!
    QMap<int, cwTripCalibration*> Calibrations;

    cwErrorModel* ErrorModel;
    QMap<CellIndex, cwErrorModel*> CellErrorModels;

    cwTrip* ParentTrip;


    bool shotIndexCheck(int index) const { return index >= 0 && index < d.shots.count();  }
    bool stationIndexCheck(int index) const { return index >= 0 && index < d.stations.count(); }

    void remove(int stationIndex, int shotIndex);
    int index(int index, Direction direction);


    QVariant stationData(DataRole role, int index) const;
    QVariant shotData(DataRole role, int index) const;

    void setStationData(DataRole role, int index, const QVariant &data);
    void setShotData(DataRole role, int index, const QVariant &data);

    void checkForErrorOnDataChanged(DataRole role, int index);
    void checkForError(DataRole role, int index);
    void checkForStationError(int index);
    void checkForShotError(int index);
    QList<cwError> checkLRUDError(cwSurveyChunk::DataRole role, int index) const;
    QList<cwError> checkDataError(cwSurveyChunk::DataRole role, int index) const;
    QList<cwError> checkWithTolerance(cwSurveyChunk::DataRole frontSightRole, cwSurveyChunk::DataRole backSightRole, int index, double tolerance = 2.0, QString units = "°") const;
    QList<cwError> checkClinoMixingType(cwSurveyChunk::DataRole role, int index) const;
    bool isShotDataEmpty(int index) const;
    bool isStationDataEmpty(int index) const;
    void clearErrors();
    void updateErrors();
    bool isClinoDownOrUp(cwSurveyChunk::DataRole role, int index) const;
    bool isClinoDownOrUpHelper(cwSurveyChunk::DataRole role, int index) const;

private slots:
    void updateCompassErrors();
    void updateClinoErrors();
    void updateCompassClinoErrors();

    void updateCalibrationsNewShots(int beginIndex, int endIndex);
    void updateCalibrationsRemoveShots(int beginIndex, int endIndex);
};

Q_DECLARE_METATYPE(cwSurveyChunk*)

/**
  \brief Gets the parent trip for this chunk
  */
inline cwTrip* cwSurveyChunk::parentTrip() const {
    return ParentTrip;
}

/**
  \brief Gets all the stations

  You shouldn't modify the station data from this list
  */
inline QList<cwStation> cwSurveyChunk::stations() const {
    return d.stations;
}

/**
  \brief Gets all the shot date

  You shouldn't modify the shot data from this list
  */
inline QList<cwShot> cwSurveyChunk::shots() const {
    return d.shots;
}


/**
* @brief cwSurveyChunk::errorModel
* @return
*/
inline cwErrorModel* cwSurveyChunk::errorModel() const {
    return ErrorModel;
}

#endif // CWSurveyChunk_H
