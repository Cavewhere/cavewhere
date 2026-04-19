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
#include <QQmlEngine>
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

// Re-export cwPenStroke as a QML namespace so its Kind enum is reachable as
// `PenStroke.Wall` / `PenStroke.Feature`. QML_FOREIGN_NAMESPACE avoids the
// "value type names should begin with a lowercase letter" warning emitted
// when a Q_GADGET is registered directly with QML_NAMED_ELEMENT.
struct cwPenStrokeForeign
{
    Q_GADGET
    QML_FOREIGN_NAMESPACE(cwPenStroke)
    QML_NAMED_ELEMENT(PenStroke)
};

#endif // CWPENSTROKE_H
