/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWINTERACTIONMANAGER_H
#define CWINTERACTIONMANAGER_H

//Qt includes
#include <QObject>
#include <QDeclarativeListProperty>

//Our includes
class cwInteraction;


class cwInteractionManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwInteraction* defaultIteraction READ defaultIteraction WRITE setDefaultInteraction NOTIFY defaultIteractionChanged)
    Q_PROPERTY(QDeclarativeListProperty<cwInteraction> interactions READ interactions)

public:
    explicit cwInteractionManager(QObject *parent = 0);

    cwInteraction* defaultIteraction() const;
    void setDefaultInteraction(cwInteraction* defaultIteraction);

    QDeclarativeListProperty<cwInteraction> interactions() ;

signals:
    void defaultIteractionChanged();

public slots:

private:
    cwInteraction* DefaultInteraction; //!< This is the default interaction,
    QList<cwInteraction*> Interactions; //!< The list off all the interactions of this manager

    static void appendInteraction(QDeclarativeListProperty<cwInteraction>* property, cwInteraction *value);

};

/**
Gets default Iteraction
*/
inline cwInteraction* cwInteractionManager::defaultIteraction() const {
    return DefaultInteraction;
}

#endif // CWINTERACTIONMANAGER_H
