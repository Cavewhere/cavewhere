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
// wall's visible line is just a layer whose rule stack traces an offset polyline
// at offset 0. What a layer draws is decided entirely by its `rules` (the
// terminal rule produces either glyph stamps or a traced polyline); there is no
// mode. All dimensional fields are in paper millimetres.
struct CAVEWHERE_LIB_EXPORT cwDecorationLayer {
    cwDecorationLayer() = default;

    QString glyphName;                       // glyph ref for stamp layers; empty for line (offset-polyline) layers
    QVector<cwPlacementRuleData> rules;      // the placement-rule stack; empty = no ink

    double bufferMm = 1.0;                    // collision keep-clear; currently dormant
    int    collisionPriority = 0;             // MutateScene tiebreak
    double minPaperScale = 0.0;               // 0 = unbounded
    double maxPaperScale = 0.0;

    // Line-styling params, read by a Trace-offset-polyline terminal rule. (These
    // will migrate into that rule's params when param interpretation lands; see
    // the symbology plan's params/editor commit.)
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
