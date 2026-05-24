/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFINDUNCONNECTEDSURVEYCHUNKS_H
#define CWFINDUNCONNECTEDSURVEYCHUNKS_H

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"
#include "cwCaveData.h"
#include "cwError.h"

#include <QList>

/**
 * \brief Find survey chunks that aren't connected to the cave's main network.
 *
 * An unconnected survey chunk is a survey leg that is floating in the cave
 * and isn't reachable from the cave's first valid chunk via shared station
 * names (case-insensitive). Empty chunks are skipped.
 *
 * Pure compute — operates on the value-snapshot \a cave; safe to call from
 * any thread.
 */
class CAVEWHERE_LIB_EXPORT cwFindUnconnectedSurveyChunks
{
public:
    cwFindUnconnectedSurveyChunks() = delete;

    /**
     * \brief One unconnected-chunk record produced by find().
     *
     * Identifies a survey chunk (by trip index + chunk-within-trip index)
     * that was not reachable from the cave's first valid chunk via shared
     * station names.
     */
    class Result {
    public:
        Result() : TripIndex(-1), SurveyChunkIndex(-1) {}
        Result(int tripIndex, int surveyChunkIndex, cwError error)
            : TripIndex(tripIndex), SurveyChunkIndex(surveyChunkIndex), Error(error) {}

        int TripIndex;
        int SurveyChunkIndex;
        cwError Error;
    };

    static Monad::Result<QList<Result>> find(const cwCaveData& cave);
};

#endif // CWFINDUNCONNECTEDSURVEYCHUNKS_H
