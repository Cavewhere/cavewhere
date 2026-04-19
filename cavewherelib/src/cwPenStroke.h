/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPENSTROKE_H
#define CWPENSTROKE_H

//Qt includes
#include <QColor>
#include <QMetaType>
#include <QUuid>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPenPoint.h"

class CAVEWHERE_LIB_EXPORT cwPenStroke {
    Q_GADGET

public:
    enum Kind {
        Wall = 0,
        Feature = 1
    };
    Q_ENUM(Kind)

    cwPenStroke() = default;

    Kind              kind  = Feature;
    double            width = 2.5;
    QColor            color;
    QVector<cwPenPoint> points;
    QUuid             id;
};

Q_DECLARE_METATYPE(cwPenStroke)

#endif // CWPENSTROKE_H
