#ifndef CWSTATIONREFERANCE_H
#define CWSTATIONREFERANCE_H

//Qt includes
#include <QObject>
#include <QSharedPointer>
#include <QVector3D>
//#include <QDebug>

//Our includes
class cwCave;
//class cwStation;
#include "cwStation.h"


/**
  \brief This class is a referance to a single station

  The same station may be used multiple times and this class make it
  so the data is shared and always upto date
  */
class cwStationReference
{

public:
    cwStationReference();
    cwStationReference(QString name);

    void setCave(cwCave* cave);
    cwCave* cave() const;

    QString name() const;
    double left() const;
    double right() const;
    double up() const;
    double down() const;
    QVector3D position() const;

    cwDistanceStates::State leftInputState() const;
    cwDistanceStates::State rightInputState() const;
    cwDistanceStates::State upInputState() const;
    cwDistanceStates::State downInputState() const;

    bool isValid() const { return SharedStation->isValid(); }

    void setName(QString Name);
    void setLeft(QString left);
    void setRight(QString right);
    void setUp(QString up);
    void setDown(QString down);
    void setPosition(QVector3D position);

    QSharedPointer<cwStation> station() const;

    bool operator ==(const cwStationReference& object) const;
    bool operator !=(const cwStationReference& object) const;
    bool operator <(const cwStationReference& object) const;

private:
    cwCave* Cave;
    QSharedPointer<cwStation> SharedStation;

private slots:
    void caveDestroyed();

};

inline uint qHash(const cwStationReference& station) {
    return qHash(station.station());
}

/**
  The cave that this station is located in
  */
inline cwCave* cwStationReference::cave() const {
    return Cave;
}

inline QString cwStationReference::name() const {
    return SharedStation->name();
}

inline double cwStationReference::left() const {
    return SharedStation->left();
}

inline double cwStationReference::right() const {
    return SharedStation->right();
}

inline double cwStationReference::up() const {
    return SharedStation->up();
}

inline double cwStationReference::down() const {
    return SharedStation->down();
}

inline QVector3D cwStationReference::position() const {
    return QVector3D();
//    return SharedStation->position();
}

inline cwDistanceStates::State cwStationReference::leftInputState() const
{
    return SharedStation->leftInputState();
}

inline cwDistanceStates::State cwStationReference::rightInputState() const
{
    return SharedStation->rightInputState();
}

inline cwDistanceStates::State cwStationReference::upInputState() const
{
    return SharedStation->upInputState();
}

inline cwDistanceStates::State cwStationReference::downInputState() const
{
    return SharedStation->downInputState();
}


inline QSharedPointer<cwStation> cwStationReference::station() const {
    return SharedStation;
}

inline bool cwStationReference::operator ==(const cwStationReference& object) const {
    return SharedStation == object.SharedStation;
}

inline bool cwStationReference::operator !=(const cwStationReference& object) const {
    return !operator ==(object);
}

inline bool cwStationReference::operator <(const cwStationReference& object) const {
    return SharedStation < object.SharedStation;
}

#endif // CWSTATIONREFERANCE_H
