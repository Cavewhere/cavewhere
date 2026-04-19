/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRIPLINEPLOTTASK_H
#define CWTRIPLINEPLOTTASK_H

//Qt includes
#include <QFuture>
#include <QHash>
#include <QList>
#include <QString>

//Our includes
#include "cwStationPositionLookup.h"
#include "cwSurveyChunkData.h"
#include "cwSurveyNetwork.h"
#include "cwTripCalibration.h"
#include "CaveWhereLibExport.h"

class cwTrip;

// Per-trip line-plot pipeline. Splits a trip's chunks into connected
// components, breaks intra-component loops by renaming duplicate station
// occurrences ("name#N"), strips declination, runs each connected chunk set
// through cwLinePlotTask, and returns the per-component geometry.
//
// Sketch-local by design: the output is stable against edits to *other*
// trips, loop closures elsewhere in the region, and declination changes on
// the trip being sketched. That stability is the reason for not using
// cwLinePlotManager's region-wide network.
namespace cwTripLinePlotTask {

struct TripComponent {
    // Edges and positions carry post-loop-break names — e.g. a duplicate
    // station "A3" appears as "A3#1" / "A3#2". The original names stay on
    // the first occurrence.
    cwSurveyNetwork         network;
    cwStationPositionLookup positions;

    // Synthetic (loop-break) name → original name. Empty for
    // loop-free components. Used by UI to render labels like "A3 (break)".
    QHash<QString, QString> renameRemap;

    // First station of the first chunk in this component (in original
    // un-renamed form). Used as the default anchor for single-component
    // trips and as the default selection in the multi-component picker.
    QString canonicalAnchor;

    // Stations visible in this component (post-loop-break names).
    // Convenience accessor for the picker dialog — network.stations() is
    // equivalent.
    QStringList stations() const { return network.stations(); }
};

struct Input {
    QString                        tripName;
    cwTripCalibrationData          calibration;
    QList<cwSurveyChunkData>       chunks;
};

// Extracts Input from a live cwTrip on the GUI thread. Safe to call with
// nullptr.
CAVEWHERE_LIB_EXPORT Input buildInput(const cwTrip *trip);

// Runs the pipeline asynchronously. The returned future resolves with a list
// of components ordered by station count descending — [0] is the largest.
// Empty input (no chunks) resolves to an empty list. The future never errors;
// individual components with cavern errors are simply omitted.
CAVEWHERE_LIB_EXPORT QFuture<QList<TripComponent>> run(Input input);

} // namespace cwTripLinePlotTask

#endif // CWTRIPLINEPLOTTASK_H
