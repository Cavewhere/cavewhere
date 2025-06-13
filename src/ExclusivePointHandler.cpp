#include "ExclusivePointHandler.h"

//Qt includes
#include <QPointerEvent>
#include <QtQuick/private/qquickdeliveryagent_p_p.h>

ExclusivePointHandler::ExclusivePointHandler(QQuickItem *parent) :
    QQuickPointHandler(parent)
{

}

bool ExclusivePointHandler::wantsEventPoint(const QPointerEvent *event, const QEventPoint &point)
{
    // On press, we want it unless a sibling of the same type also does.
    if (point.state() == QEventPoint::Pressed && QQuickSinglePointHandler::wantsEventPoint(event, point)) {
        // for (const QObject *grabber : event->passiveGrabbers(point)) {
        //     if (grabber && grabber->parent() == parent() &&
        //         grabber->metaObject()->className() == metaObject()->className())
        //         return false;
        // }
        return true;
    }
    // If we've already been interested in a point, stay interested, even if it has strayed outside bounds.
    return (point.state() != QEventPoint::Pressed &&
            QQuickSinglePointHandler::point().id() == point.id());
}

void ExclusivePointHandler::handleEventPoint(QPointerEvent *event, QEventPoint &point)
{
    switch (point.state()) {
    case QEventPoint::Pressed:
        if (QQuickDeliveryAgentPrivate::isTouchEvent(event) ||
            (static_cast<const QSinglePointEvent *>(event)->buttons() & acceptedButtons()) != Qt::NoButton) {
            qDebug() << "Point:" << point;
            setExclusiveGrab(event, point);
            setActive(true);
        }
        break;
    case QEventPoint::Released:
        if (QQuickDeliveryAgentPrivate::isTouchEvent(event) ||
            (static_cast<const QSinglePointEvent *>(event)->buttons() & acceptedButtons()) == Qt::NoButton)
            setActive(false);
        break;
    default:
        break;
    }
    point.setAccepted(false); // Just lurking... don't interfere with propagation
    emit translationChanged();
    QQuickSinglePointHandler::handleEventPoint(event, point);
}
