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
#include <QUuid>

//Std includes
#include <optional>

//AsyncFuture
#include <asyncfuture.h>

//Our includes
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwGlobals.h"
#include "cwRecenterCandidateModel.h"

class cwCave;
class cwCavingRegion;
class cwFixStation;
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
    QML_NAMED_ELEMENT(LocalProjectionManager)
    QML_UNCREATABLE("Owned by CavingRegion; access via region.localProjection")

    Q_PROPERTY(cwRecenterCandidateModel* recenterCandidates READ recenterCandidates CONSTANT FINAL)

    //! The frame's reach, in meters, for the picker to print beside the rows it
    //! grays out. Exposed rather than restated in QML so the number the user
    //! reads is the number isWithinReach() applies.
    Q_PROPERTY(double anchorThresholdMeters READ anchorThresholdMeters CONSTANT FINAL)

public:
    //! How far an input may sit from the origin before the origin counts as
    //! meaningfully wrong. Scale error out here is ~30 ppm — still negligible —
    //! while the mistakes this is meant to catch (a wrong UTM zone, a hemisphere
    //! flip, a transposed digit) miss by far more.
    static constexpr double kAnchorThresholdMeters = 50000.0;

    explicit cwLocalProjectionManager(cwCavingRegion* region);
    ~cwLocalProjectionManager() override;

    double anchorThresholdMeters() const { return kAnchorThresholdMeters; }

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

    //! Whether \a point is close enough to \a center to be part of the same
    //! project, both being points in the frame. This is the frame's reach, so it
    //! is the same rule that decides a station is worth centering on and that an
    //! origin has ended up somewhere the project isn't.
    static bool isWithinReach(const cwGeoPoint& center, const cwGeoPoint& point);

    //! The stations the user may recenter on, in region order. Created empty on
    //! first access and filled by the picker's own refresh(), so a project that
    //! never opens the picker never pays for it.
    cwRecenterCandidateModel* recenterCandidates();

    //! The middle of the project — the component-wise median of every input the
    //! frame can place, in the frame's own coordinates — or an empty result when
    //! there is no frame or nothing it can place.
    std::optional<cwGeoPoint> dataCenter() const;

    //! Whether the frame is already frozen on the middle of the project, so
    //! centering there again would re-derive the frame it already has. Answered
    //! by position, unlike cwGeoReference::anchor(): freezing leaves no anchor
    //! behind for an identity check to compare against.
    bool isCenteredOnDataCenter() const;

    //! Where \a fix sits in the project's frame, or an empty result when the fix
    //! has no coordinate the frame can read. A station the frame can't place is
    //! not one the project can be centered on, so the two refusals are the same
    //! answer here.
    std::optional<cwGeoPoint> localPointOfFix(const cwFixStation& fix) const;

    //! Re-anchor the frame on the station \a stationId, making it the input the
    //! lifecycle follows from now on. Refuses an unknown id, an unusable
    //! station, and an ineligible one — the picker grays those out, and the
    //! project may have changed between the list being drawn and the click.
    Q_INVOKABLE bool recenterOnStation(const QUuid& stationId);

    //! Freeze the frame at the middle of every placeable input — the same point
    //! maybeRecenter() steers to, applied because the user asked rather than
    //! because the data drifted. Refuses a project with no frame, and one with
    //! nothing the frame can place.
    Q_INVOKABLE bool recenterOnDataCenter();

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

    //! The rows the picker shows, as of the last
    //! cwRecenterCandidateModel::refresh().
    cwRecenterCandidateModel* m_recenterCandidates = nullptr;

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

    //! \a fix as an anchor candidate. The one place a fix station becomes an
    //! Input, so the gather and the picker can only ever read it the same way.
    static Input inputOf(const cwFixStation& fix);

    //! dataCenter() over a list already gathered, so a caller holding the inputs
    //! doesn't transform every one of them a second time.
    std::optional<cwGeoPoint> centerOf(const QList<Input>& inputs) const;

    //! The region's fix station carrying \a stationId, or an empty result when
    //! the project no longer has it.
    std::optional<cwFixStation> fixStationWithId(const QUuid& stationId) const;

    //! The two halves of gatherInputs(), in the order it concatenates them.
    //! The fix half stands on its own while headers are still arriving: it is
    //! the part of the list that no disk is being waited on for.
    QList<Input> gatherFixInputs() const;
    QList<Input> gatherLayerInputs() const;

    //! Run the state machine against the current inputs, then settle the epoch
    //! if the last thing it was waiting on has landed.
    void evaluate();

    //! Re-resolve who the frame is centered on and publish it on the
    //! cwGeoReference. Separate from evaluate() because a rename changes the
    //! answer without changing a single thing the state machine reads — running
    //! the machine for one would be work at best and a re-derive at worst.
    void updateAnchorDescription();

    //! The anchor's id spelled back out as a name, by finding whatever currently
    //! carries it. Empty when nothing does, which covers both "no anchor" and
    //! "the input holding it has just gone" — the evaluate that settles the
    //! second case runs this again.
    QString resolveAnchorDescription() const;

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

    //! Where \a input sits in the current LDP, or an empty result when the two
    //! systems can't be related — an unanswerable question must not read as
    //! "close enough".
    std::optional<cwGeoPoint> localPointOf(const Input& input) const;

    //! Horizontal distance from the current LDP's origin to \a input, on the
    //! same terms as localPointOf().
    std::optional<double> distanceFromOrigin(const Input& input) const;

    //! Move a frozen frame to the middle of \a inputs when every one of them has
    //! ended up further than the threshold from the origin — the frame outlived
    //! whatever placed it and is now describing somewhere the project isn't. A
    //! single input within the threshold vetoes the move: the frame is where the
    //! project is, and the outliers are the ones that are wrong.
    void maybeRecenter(const QList<Input>& inputs);

    //! The ways the frame moves. Every one of them records what it did, so that
    //! evaluate()'s "an anchor I didn't write came from a load" test can't be
    //! fooled by this class's own writes — which is why re-centering goes
    //! through freezeAt() rather than writing cwGeoReference directly.
    bool anchorTo(const Input& input);
    void freezeFrame();
    void clearFrame();
    bool freezeAt(const cwGeoPoint& center);

    //! What a move of the frame has to record: the anchor this class just wrote,
    //! and who the frame is now centered on. \a anchorSeen is whether the move
    //! anchored on an input that was in front of us, which only anchorTo() has.
    //!
    //! The recentering the user asks for moves the frame without running the
    //! state machine, so a move that left the description to evaluate() would
    //! keep naming the anchor the project had before the click.
    void recordFrameMove(bool anchorSeen);

    //! Anchor on the first of \a inputs a frame can actually be derived from.
    //! A coordinate PROJ can't place — a UTM easting typed into a row that says
    //! lat/long, say — yields no projection, and stopping there would leave the
    //! whole project unplaced while a perfectly good fix sat behind it.
    void anchorToFirstUsable(const QList<Input>& inputs);

    void syncCaveConnections();
};

#endif // CWLOCALPROJECTIONMANAGER_H
