/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFINDFLOATINGSURVEYS_H
#define CWFINDFLOATINGSURVEYS_H

#include "CaveWhereLibExport.h"
#include "cwFindUnconnectedSurveyChunks.h"

#include <QList>
#include <QStringList>
#include <QUuid>

class cwScopeLabels;
class cwSurveyNetwork;
struct cwCaveData;
struct cwCavingRegionData;

/**
 * \brief The one "this survey is floating" answer, from either of the two
 *        places CaveWhere can learn it.
 *
 * A survey floats when nothing joins it to the rest of its cave, and that fact
 * arrives from two unrelated passes:
 *
 * - **Native chunks, before the solve.** cwFindUnconnectedSurveyChunks walks
 *   shared station names and finds chunks no name reaches. It runs first and
 *   stops the solve, so cavern never sees the cave at all.
 * - **External scopes, after the solve.** An attached .svx owns no
 *   cwSurveyChunk, so the pre-solve pass is blind to it, and only a completed
 *   run knows whether cavern placed its stations at all.
 *
 * Both fold into Result, so a caller can ask "is this trip floating?" without
 * knowing which pass answered — the two triggers, one signal.
 *
 * Pure compute over value snapshots; safe to call from any thread.
 */
class CAVEWHERE_LIB_EXPORT cwFindFloatingSurveys
{
public:
    cwFindFloatingSurveys() = delete;

    class Result {
    public:
        //! Which pass produced this record.
        enum class Trigger {
            UnconnectedChunks, //!< pre-solve, native chunks
            ExternalScope      //!< post-solve, an attached centerline's scope
        };

        QUuid caveId;

        //! The floating trip. Always set: a native chunk is owned by the trip
        //! holding it, and an external scope *is* a trip. The one float that
        //! would belong to a cave rather than a trip — a cave-level attachment,
        //! which unwraps straight into the cave scope — is out of reach here for
        //! the reason fromExternalScopes() gives, so nothing produces one.
        QUuid tripId;

        Trigger trigger = Trigger::ExternalScope;

        //! The floating survey's station names, sorted — all of them, since a
        //! survey floats as a unit and no station of it floats on its own.
        //! Cave-local and canonical (the same spelling cwStationPositionLookup
        //! and cwTrip::solvedStations use), so a caller can look each one up
        //! without re-deriving a scope.
        //!
        //! Empty means the survey produced no solved station at all: cavern
        //! dropped it as hanging, which is what an attached centerline with no
        //! fix and nothing tying it in does.
        QStringList stations;

        bool operator==(const Result& other) const = default;
    };

    //! One record per trip owning an unconnected chunk.
    static QList<Result> fromUnconnectedChunks(
        const cwCaveData& cave,
        const QList<cwFindUnconnectedSurveyChunks::Result>& unconnected);

    //! The post-solve answer: every externally-attached trip whose scope nothing
    //! joins to its cave.
    //!
    //! Connectivity is read from the ties the region *declares* — its cwEquates,
    //! chained, across both the cave-level and region-level homes — and not from
    //! the topology cavern solved. The exporter wraps each attachment in its own
    //! "*begin", so a bare name inside one can never reach the cave by accident;
    //! an equate is the one thing that can join the two, which makes the declared
    //! set the exact answer rather than an approximation of it. Reading the
    //! solved graph instead would be wrong in the commonest case there is: an
    //! attachment fixed at its own origin lands on top of a cave auto-fixed at
    //! the origin, and .3d legs name their endpoints only by coordinate, so the
    //! two surveys fuse into one component that looks perfectly connected.
    //!
    //! \a regionNetwork supplies what the declared ties cannot: which stations
    //! each scope actually solved, and so which attachments cavern dropped
    //! outright. An empty network means no run has happened yet, which is not
    //! the same as everything hanging — the result is empty.
    //!
    //! A *cave-level* attachment is out of reach here. It unwraps straight into
    //! the cave scope, so it shares a scope with the cave's native chunks and
    //! there is nothing scope-level left to compare. Telling those two apart
    //! needs station-name reasoning inside one scope, which is a different
    //! question from the one this answers.
    static QList<Result> fromExternalScopes(const cwCavingRegionData& region,
                                            const cwSurveyNetwork& regionNetwork,
                                            const cwScopeLabels& labels);
};

#endif // CWFINDFLOATINGSURVEYS_H
