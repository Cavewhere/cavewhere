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
}

void cwGraphicsSceneMouseTransition::onTransition( QEvent * event ) {
    qDebug() << "onTransition:" << event;
}
