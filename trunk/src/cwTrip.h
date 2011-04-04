#ifndef CWSURVERYCHUNKGROUP_H
#define CWSURVERYCHUNKGROUP_H

//Our includes
#include "cwUnits.h"
class cwSurveyChunk;
class cwCave;
class cwStationReference;


//Qt include
#include <QObject>
#include <QList>
#include <QAbstractListModel>
#include <QWeakPointer>
#include <QDate>

class cwTrip : public QObject
{
    Q_OBJECT
public:
    explicit cwTrip(QObject *parent = 0);
    cwTrip(const cwTrip& object);
    cwTrip& operator=(const cwTrip& object);

    QString name() const;
    void setName(QString name);

    QDate date() const;
    void setDate(QDate date);

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

    QList< cwStationReference* > uniqueStations() const;

    //Stats
    int numberOfStations() const;

signals:
    void nameChanged(QString name);
    void dateChanged(QDate date);
    void distanceUnitChanged(cwUnits::LengthUnit);
    void chunksInserted(int begin, int end);
    void chunksRemoved(int begin, int end);

public slots:
    void setChucks(QList<cwSurveyChunk*> chunks);


protected:
    QList<cwSurveyChunk*> Chunks;
    QString Name;
    QDate Date;
    cwCave* ParentCave;

    //Units
    cwUnits::LengthUnit DistanceUnit;


private:
    void Copy(const cwTrip& object);

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

#endif // CWSURVERYCHUNKGROUP_H
