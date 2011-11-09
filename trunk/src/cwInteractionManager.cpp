//Our includes
#include "cwInteractionManager.h"

//Qt includes
#include <QDebug>

cwInteractionManager::cwInteractionManager(QObject *parent) :
    QObject(parent)
{
}

/**
Sets defaultIteraction
*/
void cwInteractionManager::setDefaultInteraction(cwInteraction* defaultIteraction) {
    if(DefaultInteraction != defaultIteraction) {
        DefaultInteraction = defaultIteraction;
        emit defaultIteractionChanged();
    }
}

/**
Gets interactions
*/
QDeclarativeListProperty<cwInteraction> cwInteractionManager::interactions() {
    return QDeclarativeListProperty<cwInteraction>(this, &Interactions, &cwInteractionManager::appendInteraction);
}

/**
  This appends new interaction to the interaction list
  */
void cwInteractionManager::appendInteraction(QDeclarativeListProperty<cwInteraction>* property, cwInteraction *value) {
    Q_UNUSED(property);
    qDebug() << "Append interaction: " << value;
}
