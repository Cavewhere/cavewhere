//Our includes
#include "cwWheelEventTransition.h"

//Qt includes
#include <QWheelEvent>
#include <QStateMachine>

cwWheelEventTransition::cwWheelEventTransition(QState *parent) :
    QEventTransition(parent)
{
}

cwWheelEventTransition::cwWheelEventTransition(QObject * object,
                                QEvent::Type type,
                                QState * sourceState) :
    QEventTransition(object, type, sourceState)
{

}

void cwWheelEventTransition::onTransition( QEvent * event ) {
    QEventTransition::onTransition(event);
    QStateMachine::WrappedEvent* wrappedEvent = dynamic_cast<QStateMachine::WrappedEvent*>(event);
    if(wrappedEvent != NULL) {
        QWheelEvent* wheelEvent = dynamic_cast<QWheelEvent*>(wrappedEvent->event());
        if(wheelEvent != NULL) {
            emit onWheelEvent(wheelEvent);
        }
    }
}
