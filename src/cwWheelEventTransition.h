/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWWHEELEVENTTRANSITION_H
#define CWWHEELEVENTTRANSITION_H

//Qt includes
#include <QEventTransition>
class QWheelEvent;

class cwWheelEventTransition : public QEventTransition
{
    Q_OBJECT
public:
    explicit cwWheelEventTransition(QState *parent = 0);
    cwWheelEventTransition(QObject * object,
                           QEvent::Type type,
                           QState * sourceState = 0);

protected:
    virtual void onTransition( QEvent * event );

signals:
    void onWheelEvent( QWheelEvent* event);

public slots:

};

#endif // CWWHEELEVENTTRANSITION_H
