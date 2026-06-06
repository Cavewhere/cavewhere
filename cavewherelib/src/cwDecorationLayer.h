/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDECORATIONLAYER_H
#define CWDECORATIONLAYER_H

//Qt includes
#include <QColor>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <QtCore/qnamespace.h>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPlacementRuleData.h"

// One drawn layer of a brush. A brush has no built-in centerline — a normal
// wall's visible line is just an OffsetCurve layer with offset 0.
// All dimensional fields are in paper millimetres.
struct CAVEWHERE_LIB_EXPORT cwDecorationLayer {
    enum Mode : int {
        RigidStamp = 0,   // glyph drawn straight at anchor + tangent rotation
        BendingStamp = 1, // glyph re-parameterised along centerline arclength
        PointStamp = 2,   // single glyph placement at an explicit anchor; no centerline
        OffsetCurve = 3,  // continuous polyline traced at perpendicular offset; no glyph
    };

    cwDecorationLayer() = default;

    QString glyphName;                       // used by all modes except OffsetCurve
    Mode    mode = RigidStamp;
    QVector<cwPlacementRuleData> rules;      // empty = use the mode's default stack

    double bufferMm = 1.0;                    // collision keep-clear; currently dormant
    int    collisionPriority = 0;             // MutateScene tiebreak
    double minPaperScale = 0.0;               // 0 = unbounded
    double maxPaperScale = 0.0;

    // OffsetCurve params (read only when mode == OffsetCurve).
    QColor           offsetCurveColorLight;
    QColor           offsetCurveColorDark;
    double           offsetCurveWidthMm  = 0.4;
    double           offsetCurveOffsetMm = 0.0;   // sign = side; 0 = on the centerline
    Qt::PenStyle     offsetCurveDash = Qt::SolidLine;
    Qt::PenCapStyle  offsetCurveCap  = Qt::RoundCap;
    Qt::PenJoinStyle offsetCurveJoin = Qt::RoundJoin;
    bool             offsetCurveAcceptsPressure = true;

    bool operator==(const cwDecorationLayer &o) const = default;
};

Q_DECLARE_METATYPE(cwDecorationLayer)

#endif // CWDECORATIONLAYER_H
