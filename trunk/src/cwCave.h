#ifndef CWCAVE_H
#define CWCAVE_H

//Our include
class cwTrip;

//Qt includes
#include <QObject>
#include <QList>

class cwCave : public QObject
{
    Q_OBJECT

public:
    explicit cwCave(QObject* parent = NULL);
    cwCave(const cwCave& object);
    cwCave& operator=(const cwCave& object);

    QString name() const;
    void setName(QString name);

    int tripCount() const;
    cwTrip* trip(int index) const;
    void addTrip(cwTrip* trip);
    void insertTrip(int i, cwTrip* trip);
    void removeTrip(int i);

signals:
    void insertedTrips(int begin, int end);
    void removedTrips(int begin, int end);

    void nameChanged(QString name);

protected:
    QList<cwTrip*> Trips;
    QString Name;

private:
    cwCave& Copy(const cwCave& object);

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
inline cwTrip* cwCave::trip(int index) const {
    if(index < 0 || index >= Trips.size()) { return NULL; }
    return Trips[index];
}

#endif // CWCAVE_H
