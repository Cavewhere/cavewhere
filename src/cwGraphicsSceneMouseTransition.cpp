/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGraphicsSceneMouseTransition.h"
#include <QDebug>

cwGraphicsSceneMouseTransition::cwGraphicsSceneMouseTransition(QState * sourceState) :
    QMouseEventTransition(sourceState)
{

}

cwGraphicsSceneMouseTransition::cwGraphicsSceneMouseTransition( QObject * object, QEvent::Type type, Qt::MouseButton button, QState * sourceState) {

}


bool cwGraphicsSceneMouseTransition::eventTest( QEvent * event ) {
    qDebug() << "eventTest:" << event;
    return false;
}

void cwGraphicsSceneMouseTransition::onTransition( QEvent * event ) {
    qDebug() << "onTransition:" << event;
}
