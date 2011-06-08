//Our includes
#include "cwMouseEventTransition.h"

//Qt includes
#include <QStateMachine>
#include <QDebug>

cwMouseEventTransition::cwMouseEventTransition(QState *parent) :
    QMouseEventTransition(parent)
{
}

cwMouseEventTransition::cwMouseEventTransition(QObject * object,
                                QEvent::Type type,
                                Qt::MouseButton button,
                                QState * sourceState) :
    QMouseEventTransition(object, type, button, sourceState)
{

}

/**
  Called when the transition has been called
  */
 void cwMouseEventTransition::onTransition( QEvent * event ) {
     QMouseEventTransition::onTransition(event);
     QStateMachine::WrappedEvent* wrappedEvent = dynamic_cast<QStateMachine::WrappedEvent*>(event);
     if(wrappedEvent != NULL) {
         QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(wrappedEvent->event());
         if(mouseEvent != NULL) {
             emit onMouseEvent(mouseEvent);
         }
     }
 }
