/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPENPOINT_H
#define CWPENPOINT_H

//Qt includes
#include <QMetaType>
#include <QPointF>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwPenPoint {
    Q_GADGET
    QML_VALUE_TYPE(penPoint)

    Q_PROPERTY(QPointF position MEMBER position)
    Q_PROPERTY(double pressure MEMBER pressure)
    Q_PROPERTY(qint64 timestampMs MEMBER timestampMs)

public:
    cwPenPoint() = default;
    cwPenPoint(QPointF pos, double pr, qint64 ts = 0)
        : position(pos), pressure(pr), timestampMs(ts) {}

    bool isNull() const { return pressure < 0.0; }

    QPointF position;
    double  pressure = -1.0;
    qint64  timestampMs = 0;
};

Q_DECLARE_METATYPE(cwPenPoint)

#endif // CWPENPOINT_H
