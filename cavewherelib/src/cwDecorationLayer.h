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

    // Line-styling params, read by a Trace terminal rule. (These will migrate
    // into that rule's params when param interpretation lands; see the symbology
    // plan's params/editor commit. Lateral offset is NOT here — it is the Offset
    // stroke rule's param.)
    QColor           lineColorLight;
    QColor           lineColorDark;
    double           lineWidthMm  = 0.4;
    Qt::PenStyle     lineDash = Qt::SolidLine;
    Qt::PenCapStyle  lineCap  = Qt::RoundCap;
    Qt::PenJoinStyle lineJoin = Qt::RoundJoin;
    bool             lineAcceptsPressure = true;

    // Fill colour, read by a Trace terminal rule alongside the line* pen. Invalid
    // (default QColor) = no fill, so the trace draws outline-only; a valid colour
    // makes it a filled region. Both variants are stored because cave maps invert
    // in dark mode. Migrates into the Trace rule's params with the line* migration
    // (commit 9).
    QColor fillColorLight;
    QColor fillColorDark;

    bool operator==(const cwDecorationLayer &o) const = default;
};

Q_DECLARE_METATYPE(cwDecorationLayer)

#endif // CWDECORATIONLAYER_H
