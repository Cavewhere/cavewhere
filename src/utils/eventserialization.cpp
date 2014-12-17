/* Copyright 2011 Stanislaw Adaszewski. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY STANISLAW ADASZEWSKI ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANISLAW ADASZEWSKI OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Stanislaw Adaszewski. */

#include "eventserialization.h"

QDataStream& operator<<(QDataStream &ds, const QTouchEvent::TouchPoint &pt)
{
    ds << (qint32) pt.id() << (qint32) pt.state() <<
          pt.pos() << pt.startPos() << pt.lastPos() <<
          pt.scenePos() << pt.startScenePos() << pt.lastScenePos() <<
          pt.screenPos() << pt.startScreenPos() << pt.lastScreenPos() <<
          pt.normalizedPos() << pt.startNormalizedPos() << pt.lastNormalizedPos() <<
          pt.rect() << pt.sceneRect() << pt.screenRect() << (qreal) pt.pressure();

    return ds;
}

QDataStream& operator>>(QDataStream &ds, QTouchEvent::TouchPoint &pt)
{
    qint32 id, state;
    QPointF pos, startPos, lastPos;
    QPointF scenePos, startScenePos, lastScenePos;
    QPointF screenPos, startScreenPos, lastScreenPos;
    QPointF normalizedPos, startNormalizedPos, lastNormalizedPos;
    QRectF rect, sceneRect, screenRect;
    qreal pressure;

    ds >> id >> state >> pos >> startPos >> lastPos
            >> scenePos >> startScenePos >> lastScenePos
            >> screenPos >> startScreenPos >> lastScreenPos
            >> normalizedPos >> startNormalizedPos >> lastNormalizedPos
            >> rect >> sceneRect >> screenRect >> pressure;

    pt.setId(id);
    pt.setState((Qt::TouchPointStates) state);
    pt.setPos(pos);
    pt.setStartPos(startPos);
    pt.setLastPos(lastPos);
    pt.setScenePos(scenePos);
    pt.setStartScenePos(startScenePos);
    pt.setLastScenePos(lastScenePos);
    pt.setScreenPos(screenPos);
    pt.setStartScreenPos(startScreenPos);
    pt.setLastScreenPos(lastScreenPos);
    pt.setNormalizedPos(normalizedPos);
    pt.setStartNormalizedPos(startNormalizedPos);
    pt.setLastNormalizedPos(lastNormalizedPos);
    pt.setRect(rect);
    pt.setSceneRect(sceneRect);
    pt.setScreenRect(screenRect);
    pt.setPressure(pressure);

    return ds;
}

QDataStream& operator<<(QDataStream &ds, const QContextMenuEvent &ev)
{
    ds
            << (qint32) EventClass::ContextMenuEvent
            << (qint32) ev.type()
            << (qint32) ev.reason()
            << ev.pos()
            << ev.globalPos()
            << (qint32) ev.modifiers();
    return ds;
}

QDataStream& operator<<(QDataStream &ds, const QKeyEvent &ev)
{
    ds
            << (qint32) EventClass::KeyEvent
            << (qint32) ev.type()
            << (qint32) ev.key()
            << (qint32) ev.modifiers()
            << ev.text()
            << ev.isAutoRepeat()
            << (qint32) ev.count();
    return ds;
}

QDataStream& operator<<(QDataStream &ds, const QMouseEvent &ev)
{
    ds
            << (qint32) EventClass::MouseEvent
            << (qint32) ev.type()
            << ev.pos()
            << ev.globalPos()
            << (qint32) ev.button()
            << (qint32) ev.buttons()
            << (qint32) ev.modifiers();
    return ds;
}

QDataStream& operator<<(QDataStream &ds, const QTabletEvent &ev)
{
    ds
            << (qint32) EventClass::TabletEvent
            << (qint32) ev.type()
            << ev.pos()
            << ev.globalPos()
            << ev.hiResGlobalX()
            << ev.hiResGlobalY()
            << (qint32) ev.device()
            << (qint32) ev.pointerType()
            << (qreal) ev.pressure()
            << (qint32) ev.xTilt()
            << (qint32) ev.yTilt()
            << (qreal) ev.tangentialPressure()
            << (qreal) ev.rotation()
            << (qint32) ev.z()
            << (qint32) ev.modifiers()
            << (qint64) ev.uniqueId();

    return ds;
}

QDataStream& operator<<(QDataStream &ds, const QTouchEvent &ev)
{
    ds
            << (qint32) EventClass::TouchEvent
            << (qint32) ev.type()
            << (qint32) ev.device()->type()
            << (qint32) ev.modifiers()
            << (qint32) ev.touchPointStates()
            << ev.touchPoints();

    return ds;
}

QDataStream& operator<<(QDataStream &ds, const QWheelEvent &ev)
{
    ds
            << (qint32) EventClass::WheelEvent
            << (qint32) ev.type()
            << ev.pos()
            << ev.globalPos()
            << (qint32) ev.delta()
            << (qint32) ev.buttons()
            << (qint32) ev.modifiers()
            << (qint32) ev.orientation();
    return ds;
}

QDataStream& operator<<(QDataStream &ds, const QInputEvent *ev)
{
    if (dynamic_cast<const QContextMenuEvent*>(ev))
        ds << *static_cast<const QContextMenuEvent*>(ev);
    else if (dynamic_cast<const QKeyEvent*>(ev))
        ds << *static_cast<const QKeyEvent*>(ev);
    else if (dynamic_cast<const QMouseEvent*>(ev))
        ds << *static_cast<const QMouseEvent*>(ev);
    else if (dynamic_cast<const QWheelEvent*>(ev))
        ds << *static_cast<const QWheelEvent*>(ev);

    return ds;
}

QDataStream& operator>>(QDataStream &ds, QContextMenuEvent* &ev)
{
    qint32 type;
    qint32 reason;
    QPoint pos, globalPos;
    qint32 modifiers;
    ds >> type >> reason >> pos >> globalPos >> modifiers;
    ev = new QContextMenuEvent((QContextMenuEvent::Reason) reason, pos, globalPos, (Qt::KeyboardModifiers) modifiers);
    return ds;
}

QDataStream& operator>>(QDataStream &ds, QKeyEvent* &ev)
{
    qint32 type, key, modifiers;
    QString text;
    bool autorep;
    qint32 count;
    ds >> type >> key >> modifiers >> text >> autorep >> count;
    ev = new QKeyEvent((QEvent::Type) type, key, (Qt::KeyboardModifiers) modifiers, text, autorep, count);
    return ds;
}

QDataStream& operator>>(QDataStream &ds, QMouseEvent* &ev)
{
    qint32 type;
    QPoint pos, globalPos;
    qint32 button, buttons, modifiers;
    ds >> type >> pos >> globalPos >> button >> buttons >> modifiers;
    ev = new QMouseEvent((QEvent::Type) type, pos, globalPos, (Qt::MouseButton) button, (Qt::MouseButtons) buttons, (Qt::KeyboardModifiers) modifiers);
    return ds;
}

//QDataStream& operator>>(QDataStream &ds, QTabletEvent* &ev)
//{
//	qint32 type;
//	QPoint pos, globalPos;
//	QPointF hiResGlobalPos;
//	qint32 device, pointerType;
//	qreal pressure;
//	qint32 xTilt, yTilt;
//	qreal tangentialPressure, rotation;
//	qint32 z, keyState;
//	qint64 uniqueID;

//	ds >> type >> pos >> globalPos >> hiResGlobalPos >> device >>
//		pointerType >> pressure >> xTilt >> yTilt >>
//		tangentialPressure >> rotation >> z >> keyState >> uniqueID;

//	ev = new QTabletEvent((QEvent::Type) type, pos, globalPos, hiResGlobalPos, device, pointerType, pressure, xTilt, yTilt, tangentialPressure, rotation, z, (Qt::KeyboardModifiers) keyState, uniqueID);

//	return ds;
//}

//QDataStream& operator>>(QDataStream &ds, QTouchEvent* &ev)
//{
//	qint32 type, deviceType, modifiers, touchPointStates;
//	QList<QTouchEvent::TouchPoint> touchPoints;

//	ds >> type >> deviceType >> modifiers >> touchPointStates >> touchPoints;
//	ev = new QTouchEvent((QEvent::Type) type, (QTouchEvent::DeviceType) deviceType, (Qt::KeyboardModifiers) modifiers, (Qt::TouchPointStates) touchPointStates, touchPoints);

//	return ds;
//}

QDataStream& operator>>(QDataStream &ds, QWheelEvent* &ev)
{
    qint32 type;
    QPoint pos, globalPos;
    qint32 delta, buttons, modifiers, orientation;
    ds >> type >> pos >> globalPos >> delta >> buttons >> modifiers >> orientation;
    ev = new QWheelEvent(pos, globalPos, delta, (Qt::MouseButtons) buttons, (Qt::KeyboardModifiers) modifiers, (Qt::Orientation) orientation);
    return ds;
}

QDataStream& operator>>(QDataStream &ds, QInputEvent*& ev)
{
    qint32 cls;
    ds >> cls;

    switch(cls)
    {
    case EventClass::ContextMenuEvent:
    {
        QContextMenuEvent *tmp;
        ds >> tmp;
        ev = tmp;
    }
        break;
    case EventClass::KeyEvent:
    {
        QKeyEvent *tmp;
        ds >> tmp;
        ev = tmp;
    }
        break;
    case EventClass::MouseEvent:
    {
        QMouseEvent *tmp;
        ds >> tmp;
        ev = tmp;
    }
        break;
        //	case EventClass::TabletEvent:
        //		{
        //			QTabletEvent *tmp;
        //			ds >> tmp;
        //			ev = tmp;
        //		}
        //		break;
        //	case EventClass::TouchEvent:
        //		{
        //			QTouchEvent *tmp;
        //			ds >> tmp;
        //			ev = tmp;
        //		}
    case EventClass::WheelEvent:
    {
        QWheelEvent *tmp;
        ds >> tmp;
        ev = tmp;
    }
        break;
    }


    return ds;
}


QDataStream &operator<<(QDataStream &ds, const cwMetaEvent &ev)
{
    Q_UNUSED(ev);
    ds
            << (qint32) EventClass::MetaCallEvent;
    return ds;
}
