#ifndef CWCAVE_H
#define CWCAVE_H

//Our include
class cwSurveyTrip;

//Qt includes
#include <QObject>
#include <QList>

class cwCave : public QObject
{
    Q_OBJECT

public:
    cwCave();

    QString name() const;
    void setName(QString name);

    int tripCount() const;
    cwSurveyTrip* trip(int index) const;
    void addTrip(cwSurveyTrip* trip);
    void insertTrip(int i, cwSurveyTrip* trip);
    void removeTrip(int i);

signals:
    void insertedTrips(int begin, int end);
    void removedTrips(int begin, int end);

    void nameChanged(QString name);

protected:
    QList<cwSurveyTrip*> Trips;
    QString Name;



};

/**
  \brief Get's the name of the cave
  */
inline QString cwCave::name() const {
    return Name;
}

/**
  \brief Get's the number of survey trips in the cave
  */
inline int cwCave::tripCount() const {
    return Trips.count();
}

/**
  \brief Get's the trip at an index

  If the index is out of bounds this return NULL
  */
inline cwSurveyTrip* cwCave::trip(int index) const {
    if(index < 0 || index >= Trips.size()) { return NULL; }
    return Trips[index];
}

#endif // CWCAVE_H
