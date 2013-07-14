/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGRAPHICSSCENEMOUSETRANSITION_H
#define CWGRAPHICSSCENEMOUSETRANSITION_H

#include <QMouseEventTransition>

class cwGraphicsSceneMouseTransition : public QMouseEventTransition
{
    Q_OBJECT
public:
    explicit cwGraphicsSceneMouseTransition(QState * sourceState = 0);
    cwGraphicsSceneMouseTransition( QObject * object, QEvent::Type type, Qt::MouseButton button, QState * sourceState = 0 );

protected:
    virtual bool eventTest( QEvent * event );
    virtual void onTransition( QEvent * event );


signals:

public slots:

};

#endif // CWGRAPHICSSCENEMOUSETRANSITION_H
