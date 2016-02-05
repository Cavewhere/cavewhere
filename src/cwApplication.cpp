/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwApplication.h"

//Qt includes
#include <QDebug>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QWheelEvent>

cwApplication::cwApplication(int & argc, char ** argv) :
    QApplication(argc, argv)
{
}

bool cwApplication::notify(QObject *receiver, QEvent *event)
{
//    QEvent* newEvent = cloneEvent(event);

//    if(newEvent != nullptr) {
//        qDebug() << "Receiver:" << receiver << event;
//    }

    return QApplication::notify(receiver, event);
}

/**
 * @brief cwEventRecorderModel::cloneEvent
 * @param event
 * @return
 */
QEvent* cwApplication::cloneEvent(QEvent *event) const
{
    if (dynamic_cast<QContextMenuEvent*>(event))
        return new QContextMenuEvent(*static_cast<QContextMenuEvent*>(event));
    else if (dynamic_cast<QKeyEvent*>(event))
        return new QKeyEvent(*static_cast<QKeyEvent*>(event));
    else if (dynamic_cast<QMouseEvent*>(event))
        return new QMouseEvent(*static_cast<QMouseEvent*>(event));
    else if (dynamic_cast<QTabletEvent*>(event))
        return new QTabletEvent(*static_cast<QTabletEvent*>(event));
    else if (dynamic_cast<QTouchEvent*>(event))
        return new QTouchEvent(*static_cast<QTouchEvent*>(event));
    else if (dynamic_cast<QWheelEvent*>(event))
        return new QWheelEvent(*static_cast<QWheelEvent*>(event));

    return nullptr;
}

//#ifdef QT_NO_DEBUG_STREAM
//QDebug operator<<(QDebug str, const QEvent *ev)
//{
//    static int eventEnumIndex = QEvent::staticMetaObject
//           .indexOfEnumerator("Type");
//     str << "QEvent";
//     if (ev) {
//        QString name = QEvent::staticMetaObject
//              .enumerator(eventEnumIndex).valueToKey(ev->type());
//        if (!name.isEmpty()) str << name; else str << ev->type();
//     } else {
//        str << (void*)ev;
//     }
//     return str.maybeSpace();
//}
//#endif //QT_NO_DEBUG_STREAM
