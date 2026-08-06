/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLOCALPROJECTIONMANAGER_H
#define CWLOCALPROJECTIONMANAGER_H

//Qt includes
#include <QFuture>
#include <QObject>
#include <QString>

//Std includes
#include <optional>

//AsyncFuture
#include <asyncfuture.h>

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
 *
 * The anchor may depend on which inputs a project has and on their order in
 * the model, never on the order in which their I/O completes. A frame derived
 * from whichever header a disk happened to return first would differ between
 * two people importing the same directory, and it is written into the project
 * file. That is what the epoch — frameFuture() — is for.
 */
class CAVEWHERE_LIB_EXPORT cwLocalProjectionManager : public QObject
{
    Q_OBJECT

public:
    explicit cwLocalProjectionManager(cwCavingRegion* region);
    ~cwLocalProjectionManager() override;

    //! The project's frame, as a future that finishes once the frame has
    //! stopped moving — settled is exactly "this future is finished", and its
    //! value is the frame it settled on, which may be the empty string.
    //!
    //! A GIS layer chains its decode on this, because points are only in the
    //! right place if the frame they were transformed into is the one the
    //! project keeps. On a project that is already anchored or frozen the
    //! future is already finished and the decode starts with no wait at all;
    //! it is pending only while the frame is still being derived from headers
    //! that are still arriving.
    //!
    //! Each call hands back an independent view of the same epoch, so a caller
    //! that cancels its own wait stops only itself.
    QFuture<QString> frameFuture();

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

    //! The epoch currently being settled, if the frame is still being derived.
    //! Held only while frameFuture() has been asked for during an open epoch,
    //! so a project that never has to derive anything never mints one.
    std::optional<AsyncFuture::Deferred<QString>> m_epoch;

    //! The georeferenced inputs in a fixed order — each cave's fix stations in
    //! region order, then the LAZ layers in model order. Nothing records which
    //! input truly came first, so "first" means first in this order: it only
    //! decides between inputs that appeared together, and it decides the same
    //! way every time, which is what a stored frame needs.
    //!
    //! Gathering is cheap enough to run on every keystroke: per input it is a
    //! hash lookup, because isValidCS memoizes the PROJ query per thread.
    QList<Input> gatherInputs() const;

    //! The two halves of gatherInputs(), in the order it concatenates them.
    //! The fix half stands on its own while headers are still arriving: it is
    //! the part of the list that no disk is being waited on for.
    QList<Input> gatherFixInputs() const;
    QList<Input> gatherLayerInputs() const;

    //! Run the state machine against the current inputs, then settle the epoch
    //! if the last thing it was waiting on has landed.
    void evaluate();

    //! The state machine itself. Split from evaluate() so that every path out
    //! of it — including the early returns — goes through the settle.
    void evaluateFrame();

    //! Whether the frame is still being derived: it is Ungeoreferenced and at
    //! least one layer is still reading its header. Which layers hold a header
    //! at any instant during that is decided by disk timing, and the frame is
    //! stored, so the anchor waits for the whole set rather than racing it.
    bool epochOpen() const;

    //! Finish the epoch on the frame as it now stands, exactly once. Anything
    //! chained on frameFuture() runs from here.
    void settleEpoch();

    //! End the epoch without settling it, so that nothing waiting on it loads
    //! into a frame that is being replaced wholesale.
    void cancelEpoch();

    //! Horizontal distance from the current LDP's origin to \a input, or an
    //! empty result when the two systems can't be related — an unanswerable
    //! question must not read as "close enough".
    std::optional<double> distanceFromOrigin(const Input& input) const;

    //! The three ways the frame moves. Each records what it did, so that
    //! evaluate()'s "an anchor I didn't write came from a load" test can't be
    //! fooled by this class's own writes.
    bool anchorTo(const Input& input);
    void freezeFrame();
    void clearFrame();

    //! Anchor on the first of \a inputs a frame can actually be derived from.
    //! A coordinate PROJ can't place — a UTM easting typed into a row that says
    //! lat/long, say — yields no projection, and stopping there would leave the
    //! whole project unplaced while a perfectly good fix sat behind it.
    void anchorToFirstUsable(const QList<Input>& inputs);

    void syncCaveConnections();
};

#endif // CWLOCALPROJECTIONMANAGER_H
