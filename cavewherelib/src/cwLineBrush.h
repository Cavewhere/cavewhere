/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLINEBRUSH_H
#define CWLINEBRUSH_H

//Qt includes
#include <QMetaType>
#include <QString>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwDecorationLayer.h"

// A brush is data, not a class. Implicitly-shared value type: its QString
// fields and QVector are all copy-on-write, so copying it is a handful of
// atomic refcount bumps — which is what lets cwPaletteSnapshot::findBrush
// return one by value without a lifetime hazard.
struct CAVEWHERE_LIB_EXPORT cwLineBrush {
    cwLineBrush() = default;

    QString name;                            // machine id, kebab-case, immutable, unique within palette
    QString displayName;                     // human-readable label (stored verbatim, not translated)
    QString category;                        // display-only grouping in pickers
    int     zOrder = 0;                      // higher paints later/on top
    bool    scrapOutline = false;            // topology flag
    QVector<cwDecorationLayer> decorations;  // drawn in order; empty = no ink
    double  minPaperScale = 0.0;             // 0 = unbounded
    double  maxPaperScale = 0.0;

    bool operator==(const cwLineBrush &o) const = default;
};

Q_DECLARE_METATYPE(cwLineBrush)

#endif // CWLINEBRUSH_H
