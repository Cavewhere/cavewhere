#ifndef CWSTATIONREFERANCE_H
#define CWSTATIONREFERANCE_H

//Qt includes
#include <QObject>
#include <QSharedPointer>

//Our includes
class cwCave;
#include "cwStation.h"


/**
  \brief This class is a referance to a single station

  The same station may be used multiple times and this class make it
  so the data is shared and always upto date
  */
class cwStationReference : public QObject
{
    Q_OBJECT
public:
    cwStationReference(QObject* parent);

    void setCave(cwCave* cave);
    cwCave* cave() const;

    Q_INVOKABLE QString name() const;
    Q_INVOKABLE QString left() const;
    Q_INVOKABLE QString right() const;
    Q_INVOKABLE QString up() const;
    Q_INVOKABLE QString down() const;
    Q_INVOKABLE QVector3D position() const;

signals:
    void nameChanged();
    void leftChanged();
    void rightChanged();
    void upChanged();
    void downChanged();
    void positionChanged();
    void reset();

public slots:
    void setName(QString Name);
    void setLeft(QString left);
    void setRight(QString right);
    void setUp(QString up);
    void setDown(QString down);
    void setPosition(QVector3D position);

private:
    cwCave* Cave;
    QSharedPointer<cwStation> SharedStation;

    void DisconnectStation();
    void ConnectStation();

private slots:
    void caveDestroyed();

};

inline QString cwStationReference::name() const {
    return SharedStation->name();
}


inline QString cwStationReference::left() const {
    return SharedStation->left();
}

inline QString cwStationReference::right() const {
    return SharedStation->right();
}

inline QString cwStationReference::up() const {
    return SharedStation->up();
}

inline QString cwStationReference::down() const {
    return SharedStation->down();
}

inline QVector3D cwStationReference::position() const {
    return SharedStation->position();
}

inline void cwStationReference::setLeft(QString left) {
    Q_ASSERT(!SharedStation.isNull());
    SharedStation->setLeft(left);
}

inline void cwStationReference::setRight(QString right) {
    Q_ASSERT(!SharedStation.isNull());
    SharedStation->setRight(right);
}

inline void cwStationReference::setUp(QString up) {
    Q_ASSERT(!SharedStation.isNull());
    SharedStation->setUp(up);
}

inline void cwStationReference::setDown(QString down) {
    Q_ASSERT(!SharedStation.isNull());
    SharedStation->setDown(down);
}

inline void cwStationReference::setPosition(QVector3D position) {
    Q_ASSERT(!SharedStation.isNull());
    SharedStation->setPosition(position);
}

/**
  \brief Called when the cave has been destroyed
  */
inline void cwStationReference::caveDestroyed() {
    Cave = NULL;
}

/**
  \brief Disconnects all the signals from SharedStation
  */
inline void cwStationReference::DisconnectStation() {
    disconnect(SharedStation.data(), 0, this, 0);
}

#endif // CWSTATIONREFERANCE_H
