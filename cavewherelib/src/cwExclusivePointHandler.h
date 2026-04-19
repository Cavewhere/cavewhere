/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXCLUSIVEPOINTHANDLER_H
#define CWEXCLUSIVEPOINTHANDLER_H

//Qt includes
#include <QQmlEngine>
#include <QtQuick/private/qquickpointhandler_p.h>

//Our includes
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwExclusivePointHandler : public QQuickPointHandler
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ExclusivePointHandler)

public:
    explicit cwExclusivePointHandler(QQuickItem *parent = nullptr);

protected:
    bool wantsEventPoint(const QPointerEvent *event, const QEventPoint &point) override;
    void handleEventPoint(QPointerEvent *event, QEventPoint &point) override;
};

#endif // CWEXCLUSIVEPOINTHANDLER_H
