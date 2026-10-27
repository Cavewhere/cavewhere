/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFINDUNCONNECTEDSURVEYCHUNKS_H
#define CWFINDUNCONNECTEDSURVEYCHUNKS_H

#include "CaveWhereLibExport.h"
#include "cwCavingRegionData.h"
#include "cwError.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QUuid>

/**
 * \brief Find the survey chunks the region solve cannot place.
 *
 * A chunk is connected when it reaches an anchor: through a station name it
 * shares with another chunk in the same scope (case-insensitive), through a
 * region equate across scopes, or through a chunk that does. The anchors are
 * the ones the driver gives cavern: every fixed station, every station of an
 * attached file, and, for a node with no fix of its own, the first station of
 * its first valid chunk, where the driver puts its origin fix. Empty chunks
 * are skipped, and a sourced node's subtree holds no native chunk to check.
 *
 * Pure compute — operates on the value snapshot \a region; safe to call from
 * any thread.
 */
class CAVEWHERE_LIB_EXPORT cwFindUnconnectedSurveyChunks
{
public:
    cwFindUnconnectedSurveyChunks() = delete;

    /**
     * \brief One unconnected-chunk record produced by find().
     *
     * Identifies a survey chunk by trip index among its node's own trips and
     * chunk index within that trip.
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

    //! The unconnected chunks of \a region, keyed by the id of the node whose
    //! own trips hold them. A node with none has no entry. Equates and
    //! attachments of \a excludedExternalOwners are left out, as the driver
    //! leaves them out.
    static QHash<QUuid, QList<Result>> find(const cwCavingRegionData& region,
                                            const QSet<QUuid>& excludedExternalOwners = {});
};

#endif // CWFINDUNCONNECTEDSURVEYCHUNKS_H
