/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwExclusivePointHandler.h"

//Qt includes
#include <QPointerEvent>
#include <QtQuick/private/qquickdeliveryagent_p_p.h>

cwExclusivePointHandler::cwExclusivePointHandler(QQuickItem *parent)
    : QQuickPointHandler(parent)
{
}

bool cwExclusivePointHandler::wantsEventPoint(const QPointerEvent *event,
                                              const QEventPoint &point)
{
    if (point.state() == QEventPoint::Pressed
        && QQuickSinglePointHandler::wantsEventPoint(event, point)) {
        return true;
    }
    return (point.state() != QEventPoint::Pressed
            && QQuickSinglePointHandler::point().id() == point.id());
}

void cwExclusivePointHandler::handleEventPoint(QPointerEvent *event, QEventPoint &point)
{
    switch (point.state()) {
    case QEventPoint::Pressed:
        if (QQuickDeliveryAgentPrivate::isTouchEvent(event)
            || (static_cast<const QSinglePointEvent *>(event)->buttons() & acceptedButtons()) != Qt::NoButton) {
            setExclusiveGrab(event, point);
            setActive(true);
        }
        break;
    case QEventPoint::Released:
        if (QQuickDeliveryAgentPrivate::isTouchEvent(event)
            || (static_cast<const QSinglePointEvent *>(event)->buttons() & acceptedButtons()) == Qt::NoButton) {
            setActive(false);
        }
        break;
    default:
        break;
    }
    // Lurking only; don't interfere with event propagation.
    point.setAccepted(false);
    emit translationChanged();
    QQuickSinglePointHandler::handleEventPoint(event, point);
}
