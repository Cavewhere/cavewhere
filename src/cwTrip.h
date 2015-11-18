/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVERYCHUNKGROUP_H
#define CWSURVERYCHUNKGROUP_H

//Our includes
#include "cwUnits.h"
#include "cwStation.h"
#include "cwError.h"
class cwSurveyChunk;
class cwCave;
class cwTeam;
class cwTripCalibration;
class cwSurveyNoteModel;
class cwShot;
class cwErrorModel;
#include "cwUndoer.h"

//Qt include
#include <QObject>
#include <QList>
#include <QAbstractListModel>
#include <QWeakPointer>
#include <QDate>
#include <QUndoCommand>


class cwTrip : public QObject, public cwUndoer
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QDate date READ date WRITE setDate NOTIFY dateChanged)
    Q_PROPERTY(cwSurveyNoteModel* notes READ notes NOTIFY notesChanged)
    Q_PROPERTY(cwTeam* team READ team WRITE setTeam NOTIFY teamChanged)
    Q_PROPERTY(int numberOfChunks READ numberOfChunks NOTIFY numberOfChunksChanged)
    Q_PROPERTY(cwTripCalibration* calibration READ calibrations WRITE setCalibration NOTIFY calibrationChanged)  
    Q_PROPERTY(cwCave* parentCave READ parentCave WRITE setParentCave NOTIFY parentCaveChanged)
    Q_PROPERTY(cwErrorModel* errorModel READ errorModel CONSTANT)

public:
    explicit cwTrip(QObject *parent = 0);
    cwTrip(const cwTrip& object);
    cwTrip& operator=(const cwTrip& object);
    ~cwTrip();

    QString name() const;
    void setName(QString name);

    QDate date() const;
    void setDate(QDate date);

    void setTeam(cwTeam* team);
    cwTeam* team() const;

    void setCalibration(cwTripCalibration* calibrations);
    cwTripCalibration* calibrations() const;

    cwSurveyNoteModel* notes() const;

    void addShotToLastChunk(const cwStation& fromStation, const cwStation& toStation, const cwShot& shot);
    void removeChunks(int begin, int end);
    void insertChunk(int row, cwSurveyChunk* chunk);
    void addChunk(cwSurveyChunk* chunk);
    Q_INVOKABLE void addNewChunk();
    Q_INVOKABLE void removeChunk(cwSurveyChunk* chunk);

    int numberOfChunks() const;
    Q_INVOKABLE cwSurveyChunk* chunk(int i) const;
    QList<cwSurveyChunk*> chunks() const;

    void setParentCave(cwCave* parentCave);
    cwCave* parentCave();

    QList< cwStation > stations() const;
    QList< cwStation > uniqueStations() const;

    //Network operations
    int numberOfStations() const;
    bool hasStation(QString stationName) const;
    QSet<cwStation> neighboringStations(QString stationName) const;

    void stationPositionModelUpdated();

    cwErrorModel* errorModel() const;
signals:
    void nameChanged();
    void dateChanged(QDate date);
    void chunksInserted(int begin, int end);
    void chunksAboutToBeRemoved(int begin, int end);
    void chunksRemoved(int begin, int end);
    void teamChanged();
    void calibrationChanged();
    void notesChanged();
    void numberOfChunksChanged();
    void parentCaveChanged();

public slots:
    void setChucks(QList<cwSurveyChunk*> chunks);


protected:
    QList<cwSurveyChunk*> Chunks;
    QString Name;
    QDate Date;
    cwTeam* Team;
    cwTripCalibration* Calibration;
    cwCave* ParentCave;
    cwSurveyNoteModel* Notes;
    cwErrorModel* ErrorModel; //!<

    //Units


    virtual void setUndoStackForChildren();
private:
    void Copy(const cwTrip& object);

    class NameCommand : public QUndoCommand {
    public:
        NameCommand(cwTrip* trip, QString name);
        void redo();
        void undo();
    private:
        cwTrip* Trip;
        QString NewName;
        QString OldName;
    };

    class DateCommand : public QUndoCommand {
    public:
        DateCommand(cwTrip* trip, QDate date);
        void redo();
        void undo();
    private:
        cwTrip* Trip;
        QDate NewDate;
        QDate OldDate;
    };


};

Q_DECLARE_METATYPE(cwTrip*)

/**
Gets numberOfChunks
*/
inline int cwTrip::numberOfChunks() const {
    return chunks().size();
}

/**
  \brief Get's the name of the survey trip
  */
inline QString cwTrip::name() const {
    return Name;
}

/**
  Gets the date of the survey trip
  */
inline QDate cwTrip::date() const {
    return Date;
}

/**
  \brief Parent's cave
  */
inline cwCave* cwTrip::parentCave() {
    return ParentCave;
}


/**
  \brief Gets the survey team for the trip
  */
inline cwTeam* cwTrip::team() const {
    return Team;
}

/**
  \brief Gets the trip's calibration
  */
inline cwTripCalibration* cwTrip::calibrations() const {
    return Calibration;
}

/**
  \brief This gets the notes for a trip
  */
inline cwSurveyNoteModel* cwTrip::notes() const {
    return Notes;
}

/**
* @brief class::errorModel
* @return
*/
inline cwErrorModel* cwTrip::errorModel() const {
    return ErrorModel;
}

#endif // CWSURVERYCHUNKGROUP_H
