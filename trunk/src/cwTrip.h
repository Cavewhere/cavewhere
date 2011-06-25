#ifndef CWSURVERYCHUNKGROUP_H
#define CWSURVERYCHUNKGROUP_H

//Our includes
#include "cwUnits.h"
class cwSurveyChunk;
class cwCave;
class cwStationReference;
class cwTeam;
class cwTripCalibration;
class cwSurveyNoteModel;
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

public:
    explicit cwTrip(QObject *parent = 0);
    cwTrip(const cwTrip& object);
    cwTrip& operator=(const cwTrip& object);

    QString name() const;
    void setName(QString name);

    QDate date() const;
    void setDate(QDate date);

    void setTeam(cwTeam* team);
    cwTeam* team() const;

    void setCalibration(cwTripCalibration* calibrations);
    cwTripCalibration* calibrations() const;

    cwSurveyNoteModel* notes() const;

    cwUnits::LengthUnit distanceUnit() const;
    void setDistanceUnit(cwUnits::LengthUnit);

    void removeChunks(int begin, int end);
    void insertChunk(int row, cwSurveyChunk* chunk);
    void addChunk(cwSurveyChunk* chunk);

    int numberOfChunks() const;
    cwSurveyChunk* chunk(int i) const;
    QList<cwSurveyChunk*> chunks() const;

    void setParentCave(cwCave* parentCave);
    cwCave* parentCave();

    QList< cwStationReference > uniqueStations() const;

    //Stats
    int numberOfStations() const;

signals:
    void nameChanged(QString name);
    void dateChanged(QDate date);
    void distanceUnitChanged(cwUnits::LengthUnit);
    void chunksInserted(int begin, int end);
    void chunksRemoved(int begin, int end);
    void teamChanged();
    void calibrationChanged();
    void notesChanged();

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

    //Units
    cwUnits::LengthUnit DistanceUnit;

    virtual void setUndoStackForChildren();
private:
    void Copy(const cwTrip& object);

    class NameCommand : public QUndoCommand {
    public:
        NameCommand(cwTrip* trip, QString name);
        void redo();
        void undo();
    private:
        QWeakPointer<cwTrip> Trip;
        QString NewName;
        QString OldName;
    };

    class DateCommand : public QUndoCommand {
    public:
        DateCommand(cwTrip* trip, QDate date);
        void redo();
        void undo();
    private:
        QWeakPointer<cwTrip> Trip;
        QDate NewDate;
        QDate OldDate;
    };


};

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
  \brief Gets the distance unit for the trip
  */
inline cwUnits::LengthUnit cwTrip::distanceUnit() const {
    return DistanceUnit;
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

#endif // CWSURVERYCHUNKGROUP_H
