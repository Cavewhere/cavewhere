/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLOCALPROJECTIONMANAGER_H
#define CWLOCALPROJECTIONMANAGER_H

//Qt includes
#include <QObject>
#include <QString>

//Std includes
#include <optional>

//Our includes
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwGlobals.h"

class cwCave;
class cwCavingRegion;
class cwLazLayer;

/**
 * Drives the project's local projection (LDP) through its lifecycle in
 * response to the region's georeferenced inputs — the fix stations that carry
 * a readable coordinate and the LAZ layers that declare a CRS.
 *
 * The states and their invariants live on cwGeoReference; the policy for
 * moving between them lives here, because deciding them needs the region's
 * caves and layers. See plans/LDP_AUTO_COORDINATE_SYSTEM_PLAN.html §4.
 *
 * The shape of the policy: the origin ends up near the data, moves only when
 * it was meaningfully wrong, and every move is caused by the user correcting
 * or deleting the input that put it there.
 *
 * Only witnessed disappearances count as deletions. A project's inputs
 * do not all arrive at once — LAZ layers are rescanned from disk after the
 * caves are loaded — so an anchor that is merely absent has not necessarily
 * been deleted, and treating it as deleted would move a frame that was stored
 * precisely so it would never have to be re-derived.
 */
class CAVEWHERE_LIB_EXPORT cwLocalProjectionManager : public QObject
{
    Q_OBJECT

public:
    explicit cwLocalProjectionManager(cwCavingRegion* region);

    //! Suspend evaluation while a project load replaces the region's data.
    //! Caves arriving mid-load would otherwise derive a frame that
    //! cwGeoReference::restore() overwrites moments later — a PROJ pipeline
    //! built on every open for nothing. Leaving the loading state runs one
    //! evaluate() against the restored frame.
    void setLoading(bool loading);

private:
    //! One candidate to anchor on: what would identify it, where it is, and the
    //! system that says so.
    struct Input {
        cwGeoReference::Anchor anchor;
        QString coordinateSystem;
        cwGeoPoint point;
    };

    cwCavingRegion* m_region = nullptr;

    //! Whether the current anchor has been seen among the inputs since it was
    //! set. Until it has, its absence means "not loaded yet" rather than
    //! "deleted", and the frame is left exactly as it was stored.
    bool m_anchorSeen = false;

    //! The anchor as of the last time this class looked. An anchor that changed
    //! without us doing it came from a load, and a loaded anchor has not been
    //! seen yet however long its predecessor had been.
    cwGeoReference::Anchor m_lastAnchor;

    //! Whether any georeferenced input has been seen at all. Same reasoning as
    //! m_anchorSeen, for the transition back to Ungeoreferenced.
    bool m_sawAnyInput = false;

    //! Whether a project load is replacing the region's data — see setLoading().
    bool m_loading = false;

    //! The georeferenced inputs in a fixed order — each cave's fix stations in
    //! region order, then the LAZ layers in model order. Nothing records which
    //! input truly came first, so "first" means first in this order: it only
    //! decides between inputs that appeared together, and it decides the same
    //! way every time, which is what a stored frame needs.
    //!
    //! Gathering is cheap enough to run on every keystroke: per input it is a
    //! hash lookup, because isValidCS memoizes the PROJ query per thread.
    QList<Input> gatherInputs() const;

    //! Run the state machine against the current inputs.
    void evaluate();

    //! Horizontal distance from the current LDP's origin to \a input, or an
    //! empty result when the two systems can't be related — an unanswerable
    //! question must not read as "close enough".
    std::optional<double> distanceFromOrigin(const Input& input) const;

    //! The three ways the frame moves. Each records what it did, so that
    //! evaluate()'s "an anchor I didn't write came from a load" test can't be
    //! fooled by this class's own writes.
    void anchorTo(const Input& input);
    void freezeFrame();
    void clearFrame();

    void syncCaveConnections();
    void syncLayerConnections();
};

#endif // CWLOCALPROJECTIONMANAGER_H
