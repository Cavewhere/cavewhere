/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLocalProjectionManager.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLocalProjection.h"
#include "cwMath.h"

//Std includes
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
    //! How near the origin a point has to sit to count as being on it. The frame
    //! travels as a PROJ string, so an origin written out and read back lands a
    //! fraction of a meter from where it started; a gap past this one is the
    //! project's data having moved rather than the round trip.
    constexpr double kFrameOriginToleranceMeters = 1.0;

    //! How far \a from and \a to sit apart horizontally, both being points in
    //! the frame. The frame is judged on horizontal position, so z takes no part.
    double horizontalDistance(const cwGeoPoint& from, const cwGeoPoint& to)
    {
        return std::hypot(to.x - from.x, to.y - from.y);
    }

    //! How far \a point sits from the frame's origin. The LDP fixes x_0 = y_0 = 0,
    //! so a point already in the frame is its own offset from the origin.
    double horizontalMagnitude(const cwGeoPoint& point)
    {
        return horizontalDistance(cwGeoPoint(0.0, 0.0, 0.0), point);
    }

    //! Whether \a fix can place anything. Only a Valid fix has components at
    //! all — every other state reads zeros, and a frame centered on a coordinate
    //! system's own origin is exactly the "cave in the Gulf of Guinea" failure
    //! the LDP exists to make impossible.
    bool usableFixStation(const cwFixStation& fix)
    {
        return fix.state() == cwFixStation::Valid
                && cwCoordinateTransform::isValidCS(fix.inputCS().trimmed());
    }
}

cwLocalProjectionManager::cwLocalProjectionManager(cwCavingRegion* region) :
    QObject(region),
    m_region(region)
{
    Q_ASSERT(m_region != nullptr);

    connect(m_region, &cwCavingRegion::caveCountChanged, this, [this] {
        syncCaveConnections();
        evaluate();
    });

    // Everything evaluate() reads off a layer arrives through the model, so
    // these three connections are the whole layer dependency. The row signals
    // rather than countChanged: a rescan that swaps one file for another leaves
    // the count alone, and clear() deletes its layers before it announces
    // anything.
    cwLazLayerModel* layers = m_region->lazLayers();
    connect(layers, &QAbstractItemModel::rowsInserted,
            this, &cwLocalProjectionManager::evaluate);
    connect(layers, &QAbstractItemModel::rowsRemoved,
            this, &cwLocalProjectionManager::evaluate);
    connect(layers, &QAbstractItemModel::modelReset,
            this, &cwLocalProjectionManager::evaluate);

    // The edges of "any header still being read", which are the two moments the
    // epoch turns on: the first probe of a batch opens it, and the last one to
    // land closes it and lets the anchor be chosen from the whole set. The
    // falling edge is also how a header's arrival is heard — the layer publishes
    // the header before it reports the probe finished.
    connect(layers, &cwLazLayerModel::headerProbeInFlightChanged,
            this, &cwLocalProjectionManager::evaluate);

    // The one input change with no probe behind it: setSourceCSOverride applies
    // its precedence in memory, so nothing above would fire for it. A rename is
    // the opposite case — it moves what the anchor is called without moving a
    // thing the state machine reads.
    connect(layers, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
        if (roles.isEmpty() || roles.contains(cwLazLayerModel::SourceCSRole)) {
            evaluate();
        }
        if (roles.isEmpty() || roles.contains(cwLazLayerModel::NameRole)) {
            updateAnchorDescription();
        }
    });

    syncCaveConnections();
    evaluate();
}

cwLocalProjectionManager::~cwLocalProjectionManager()
{
    cancelEpoch();
}

QFuture<QString> cwLocalProjectionManager::frameFuture()
{
    if (!epochOpen()) {
        return AsyncFuture::completed(m_region->geoReference()->localCoordinateSystem());
    }

    if (!m_epoch.has_value()) {
        m_epoch = AsyncFuture::deferred<QString>();
    }

    // Every caller gets its own view of the one epoch. AsyncFuture pushes a
    // cancel back up the chain it came from, so without the shield a single
    // layer giving up its wait would cancel the epoch itself — and every other
    // layer waiting on it would drop its points for a frame that then never
    // settles.
    return AsyncFuture::shield(m_epoch->future());
}

bool cwLocalProjectionManager::epochOpen() const
{
    return m_region->geoReference()->state() == cwGeoReference::Ungeoreferenced
            && m_region->lazLayers()->anyHeaderProbeInFlight();
}

void cwLocalProjectionManager::settleEpoch()
{
    if (!m_epoch.has_value() || epochOpen()) {
        return;
    }

    // The frame as it now stands: derived from the batch that just closed,
    // taken from a fix station that arrived while the batch was still reading,
    // or empty because nothing in the project could place it. Empty is a real
    // answer — the clouds load untransformed, in their own units — and it is an
    // answer only a completed future can deliver, because the frame never
    // changed and so nothing signaled.
    auto epoch = *m_epoch;
    m_epoch.reset();
    epoch.complete(m_region->geoReference()->localCoordinateSystem());
}

void cwLocalProjectionManager::cancelEpoch()
{
    if (!m_epoch.has_value()) {
        return;
    }
    auto epoch = *m_epoch;
    m_epoch.reset();
    epoch.cancel();
}

void cwLocalProjectionManager::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    if (m_loading) {
        // The frame the load is about to restore is the one the project keeps,
        // so an epoch derived from what is here now has nothing left to settle
        // on. Cancelling rather than completing it means nothing chained on it
        // loads into a frame that is being replaced wholesale.
        cancelEpoch();
    } else {
        evaluate();
    }
}

QList<cwLocalProjectionManager::Input> cwLocalProjectionManager::gatherInputs() const
{
    QList<Input> inputs = gatherFixInputs();
    inputs.append(gatherLayerInputs());
    return inputs;
}

QList<cwLocalProjectionManager::Input> cwLocalProjectionManager::gatherFixInputs() const
{
    QList<Input> inputs;

    for (cwCave* cave : m_region->caves()) {
        if (cave == nullptr) {
            continue;
        }
        for (const cwFixStation& fix : cave->fixStations()->fixStations()) {
            if (!usableFixStation(fix)) {
                continue;
            }
            inputs.append(inputOf(fix));
        }
    }

    return inputs;
}

cwLocalProjectionManager::Input cwLocalProjectionManager::inputOf(const cwFixStation& fix)
{
    return Input{
        cwGeoReference::Anchor{cwGeoReference::Anchor::FixStation, fix.id()},
        fix.inputCS().trimmed(),
        cwGeoPoint(fix.easting(), fix.northing(), fix.elevation())
    };
}

QList<cwLocalProjectionManager::Input> cwLocalProjectionManager::gatherLayerInputs() const
{
    QList<Input> inputs;

    cwLazLayerModel* layers = m_region->lazLayers();
    for (int row = 0; row < layers->count(); ++row) {
        cwLazLayer* layer = layers->layerAt(row);
        if (layer == nullptr) {
            continue;
        }
        // The header says where the cloud sits in its own CRS, and it is read
        // as soon as the layer has a path — before the points decode, and even
        // for a layer that is disabled and will decode none. Every layer that
        // has one belongs here, because the questions asked of this list are
        // about the whole set: a layer missing from it reads as one that was
        // deleted, and the frame would move for a load that simply hasn't
        // landed yet.
        if (!layer->hasReadHeader()) {
            continue;
        }
        const QString layerCS = layer->sourceCS().trimmed();
        if (!cwCoordinateTransform::isValidCS(layerCS)) {
            continue;
        }
        inputs.append(Input{
            cwGeoReference::Anchor{cwGeoReference::Anchor::LazLayer, layer->id()},
            layerCS,
            layer->sourceBboxCenter()
        });
    }

    return inputs;
}

std::optional<cwGeoPoint> cwLocalProjectionManager::localPointOf(const Input& input) const
{
    const QString localCS = m_region->geoReference()->localCoordinateSystem();
    if (localCS.isEmpty()) {
        return std::nullopt;
    }

    return cwCoordinateTransform::transformPoint(
        input.coordinateSystem, localCS, input.point);
}

std::optional<double> cwLocalProjectionManager::distanceFromOrigin(const Input& input) const
{
    const auto local = localPointOf(input);
    if (!local.has_value()) {
        return std::nullopt;
    }

    return horizontalMagnitude(*local);
}

bool cwLocalProjectionManager::isWithinReach(const cwGeoPoint& center,
                                             const cwGeoPoint& point)
{
    return horizontalDistance(center, point) <= kAnchorThresholdMeters;
}

std::optional<cwGeoPoint> cwLocalProjectionManager::dataCenter() const
{
    return centerOf(gatherInputs());
}

std::optional<cwGeoPoint> cwLocalProjectionManager::centerOf(const QList<Input>& inputs) const
{
    QList<double> xs;
    QList<double> ys;
    for (const Input& input : inputs) {
        const auto local = localPointOf(input);
        if (!local.has_value()) {
            // A system that can't be related to the frame can't vote on where
            // the middle of the project is.
            continue;
        }
        xs.append(local->x);
        ys.append(local->y);
    }

    if (xs.isEmpty()) {
        return std::nullopt;
    }

    // Component-wise median rather than mean, so a single tile 300 km out pulls
    // the center by meters instead of halfway to it. Horizontal only — the
    // derivation ignores z.
    return cwGeoPoint(cwMedian(std::move(xs)), cwMedian(std::move(ys)), 0.0);
}

std::optional<cwGeoPoint> cwLocalProjectionManager::localPointOfFix(const cwFixStation& fix) const
{
    if (!usableFixStation(fix)) {
        return std::nullopt;
    }
    return localPointOf(inputOf(fix));
}

cwRecenterCandidateModel* cwLocalProjectionManager::recenterCandidates()
{
    if (m_recenterCandidates == nullptr) {
        m_recenterCandidates = new cwRecenterCandidateModel(this, m_region);
    }
    return m_recenterCandidates;
}

std::optional<cwFixStation> cwLocalProjectionManager::fixStationWithId(const QUuid& stationId) const
{
    for (cwCave* cave : m_region->caves()) {
        if (cave == nullptr) {
            continue;
        }
        for (const cwFixStation& fix : cave->fixStations()->fixStations()) {
            if (fix.id() == stationId) {
                return fix;
            }
        }
    }
    return std::nullopt;
}

bool cwLocalProjectionManager::recenterOnStation(const QUuid& stationId)
{
    const auto fix = fixStationWithId(stationId);
    if (!fix.has_value()) {
        return false;
    }

    const auto local = localPointOfFix(*fix);
    const auto center = dataCenter();
    if (!local.has_value() || !center.has_value()
            || !isWithinReach(*center, *local)) {
        // The picker draws these as unusable or disabled, so reaching here means
        // the project changed between the list being drawn and the click. Refuse
        // rather than derive a frame the project's own data sits outside.
        return false;
    }

    return anchorTo(inputOf(*fix));
}

bool cwLocalProjectionManager::isCenteredOnDataCenter() const
{
    return isCenteredOnDataCenter(dataCenter());
}

bool cwLocalProjectionManager::isCenteredOnDataCenter(const std::optional<cwGeoPoint>& center) const
{
    // An anchored frame is never "already there", even with its anchor sitting
    // on the middle of the data: centering would still cut the frame loose from
    // the input it follows, which is a change worth offering.
    if (m_region->geoReference()->state() != cwGeoReference::Frozen) {
        return false;
    }

    return center.has_value()
            && horizontalMagnitude(*center) <= kFrameOriginToleranceMeters;
}

bool cwLocalProjectionManager::recenterOnDataCenter()
{
    const auto center = dataCenter();
    if (!center.has_value()) {
        return false;
    }

    // Deliberately without maybeRecenter()'s two guards. Those exist so the
    // automatic path only moves a frame that is provably wrong; here the user is
    // asking, and an origin already sitting on its data is precisely the case
    // this is for.
    return freezeAt(*center);
}

bool cwLocalProjectionManager::freezeAt(const cwGeoPoint& center)
{
    cwGeoReference* geoReference = m_region->geoReference();

    // The center is in the frame's coordinates and the frame carries the datum
    // pinned when the project was first placed, so deriving from it keeps that
    // datum: inputs on mixed datums vote on position, never on datum. StoredFrame
    // is what says so — without it a legacy frame on WGS84 would come back on the
    // plate-fixed datum, which is a migration, not a move.
    const QString candidate =
        cwLocalProjection::deriveFrom(geoReference->localCoordinateSystem(), center,
                                      cwLocalProjection::DatumSource::StoredFrame);
    if (candidate.isEmpty()) {
        // Nothing we could vouch for. Leave the frame alone rather than
        // half-move it, the way anchorTo() does.
        return false;
    }

    geoReference->recenter(candidate);
    recordFrameMove(false);
    return true;
}

void cwLocalProjectionManager::maybeRecenter(const QList<Input>& inputs)
{
    for (const Input& input : inputs) {
        const auto local = localPointOf(input);
        if (!local.has_value()) {
            // A system that can't be related to the frame is neither near nor
            // far: it can't veto the move and it can't vote on where to go.
            continue;
        }

        if (horizontalMagnitude(*local) <= kAnchorThresholdMeters) {
            // Something is where the frame says the project is, so the frame is
            // still describing the project. This is the veto that keeps a tile
            // from the wrong county from dragging the origin to itself, and it
            // is the common case: a frozen project sitting on its own data pays
            // one transform for it and stops here.
            return;
        }
    }

    // The same inputs the veto just read: nothing can have changed under them,
    // so re-walking the region to gather them again would be answering a
    // question this function was already handed. centerOf() does reproject each
    // one a second time, which the memoized pipeline makes a per-point
    // multiply — and this branch only runs when the frame is about to move and
    // every cloud re-decodes anyway.
    const auto center = centerOf(inputs);
    if (!center.has_value()) {
        // Nothing that could be placed at all, so the frame isn't provably
        // wrong. Moving a stored frame on no evidence is the one thing storing
        // it was meant to prevent.
        return;
    }

    if (horizontalMagnitude(*center) <= kAnchorThresholdMeters) {
        // The origin is already in the middle of the data, however far the data
        // spreads either side of it: every input can be past the threshold while
        // the middle of them is a couple of kilometers away, which is not an
        // origin that is wrong.
        //
        // This is also what makes the rule terminate. Moving the frame onto the
        // median leaves the next median a few millimeters off the new origin,
        // and a proj string carries lat_0 to ten decimal places, so a check for
        // a frame that merely changed would keep finding one — re-decoding every
        // cloud in the project each time. The origin moves when it is
        // meaningfully wrong, on the same terms as everywhere else here.
        return;
    }

    freezeAt(*center);
}

bool cwLocalProjectionManager::anchorTo(const Input& input)
{
    const QString localCS = cwLocalProjection::deriveFrom(input.coordinateSystem, input.point);
    if (localCS.isEmpty()) {
        // Nothing to anchor to that we could vouch for. Leave the frame alone
        // rather than half-move it; the next edit tries again.
        return false;
    }
    m_region->geoReference()->anchorTo(input.anchor, localCS);
    recordFrameMove(true);
    return true;
}

void cwLocalProjectionManager::anchorToFirstUsable(const QList<Input>& inputs)
{
    for (const Input& input : inputs) {
        if (anchorTo(input)) {
            return;
        }
    }
}

// freezeFrame and clearFrame exist so that every move of the frame records what
// it did. evaluate() opens by reading an anchor it didn't record as a load, so a
// mutation that forgot to update m_lastAnchor would misread its own write as one.
void cwLocalProjectionManager::freezeFrame()
{
    m_region->geoReference()->freeze();
    recordFrameMove(false);
}

void cwLocalProjectionManager::clearFrame()
{
    m_region->geoReference()->clear();
    recordFrameMove(false);
    m_sawAnyInput = false;
}

void cwLocalProjectionManager::recordFrameMove(bool anchorSeen)
{
    m_lastAnchor = m_region->geoReference()->anchor();
    m_anchorSeen = anchorSeen;
    updateAnchorDescription();
}

void cwLocalProjectionManager::evaluate()
{
    if (m_loading) {
        return;
    }

    evaluateFrame();
    updateAnchorDescription();
    settleEpoch();
}

void cwLocalProjectionManager::updateAnchorDescription()
{
    m_region->geoReference()->setAnchorDescription(resolveAnchorDescription());
}

QString cwLocalProjectionManager::resolveAnchorDescription() const
{
    const cwGeoReference::Anchor anchor = m_region->geoReference()->anchor();

    switch (anchor.kind) {
    case cwGeoReference::Anchor::None:
        break;
    case cwGeoReference::Anchor::FixStation:
        for (cwCave* cave : m_region->caves()) {
            if (cave == nullptr) {
                continue;
            }
            const QList<cwFixStation>& fixes = cave->fixStations()->fixStations();
            for (const cwFixStation& fix : fixes) {
                if (fix.id() == anchor.id) {
                    return QStringLiteral("%1 — %2").arg(fix.stationName(), cave->name());
                }
            }
        }
        break;
    case cwGeoReference::Anchor::LazLayer:
        for (cwLazLayer* layer : m_region->lazLayers()->layers()) {
            if (layer != nullptr && layer->id() == anchor.id) {
                return layer->name();
            }
        }
        break;
    }

    return QString();
}

void cwLocalProjectionManager::evaluateFrame()
{
    cwGeoReference* geoReference = m_region->geoReference();

    if (geoReference->anchor() != m_lastAnchor) {
        // Something other than this class moved the anchor — a load restoring
        // what was stored. Whatever the previous anchor had proved about the
        // inputs proves nothing about this one.
        m_lastAnchor = geoReference->anchor();
        m_anchorSeen = false;
    }

    if (epochOpen()) {
        // Headers are still arriving, so the layers that hold one right now are
        // whichever ones the disk got to first — a frame derived from them is a
        // frame that depends on disk timing. A fix station is the user's own
        // input and the model orders it, so it may anchor immediately: it leads
        // the list gatherInputs() builds, so anchoring on one now picks the same
        // input the closed batch would have.
        anchorToFirstUsable(gatherFixInputs());
        return;
    }

    const QList<Input> inputs = gatherInputs();

    if (geoReference->state() == cwGeoReference::Anchored) {
        // The overwhelmingly common case, and the one that runs on every edit:
        // the anchor is still there and the question is only whether it moved.
        const auto anchored =
            std::find_if(inputs.constBegin(), inputs.constEnd(),
                         [geoReference](const Input& input) {
                             return input.anchor == geoReference->anchor();
                         });
        if (anchored != inputs.constEnd()) {
            m_anchorSeen = true;
            m_sawAnyInput = true;
            const auto distance = distanceFromOrigin(*anchored);
            if (distance.has_value() && *distance > kAnchorThresholdMeters) {
                // The anchor was corrected by more than the frame can absorb —
                // the only automatic move of an existing origin. A smaller
                // correction is deliberately ignored: refining an entrance by a
                // few meters must not reproject the whole project.
                anchorTo(*anchored);
            }
            return;
        }

        if (!m_anchorSeen) {
            // The anchor has never been among the inputs, so this is a project
            // whose layers haven't been rescanned yet, not one whose anchor was
            // deleted.
            return;
        }

        if (inputs.isEmpty()) {
            // The anchor was the last georeferenced thing in the project.
            clearFrame();
            return;
        }
        m_sawAnyInput = true;

        // If anything georeferenced is still near the origin, the frame is
        // still a good frame and has only lost the input answerable for it —
        // handing the role to a neighbor would move the origin for no reason.
        const bool somethingNearby =
            std::any_of(inputs.constBegin(), inputs.constEnd(),
                        [this](const Input& input) {
                            const auto distance = distanceFromOrigin(input);
                            return distance.has_value()
                                   && *distance <= kAnchorThresholdMeters;
                        });
        if (somethingNearby) {
            freezeFrame();
        } else {
            anchorToFirstUsable(inputs);
        }
        return;
    }

    if (inputs.isEmpty()) {
        if (m_sawAnyInput) {
            clearFrame();
        }
        return;
    }
    m_sawAnyInput = true;

    if (geoReference->state() == cwGeoReference::Ungeoreferenced) {
        anchorToFirstUsable(inputs);
        return;
    }

    // Frozen: a frame with nothing left to follow, so no single input can be
    // answerable for it being wrong. It takes the whole project having left the
    // frame behind to move it.
    maybeRecenter(inputs);
}

void cwLocalProjectionManager::syncCaveConnections()
{
    // Connect-only: a destroyed cave drops its own connections, and
    // UniqueConnection makes re-running harmless. Remembering the caves instead
    // would mean holding raw pointers to caves the region has already handed to
    // an undo command that deleteLater()s them, and the row signals rather than
    // countChanged so one setFixStations doesn't evaluate twice.
    for (cwCave* cave : m_region->caves()) {
        if (cave == nullptr) {
            continue;
        }
        cwFixStationModel* model = cave->fixStations();
        connect(model, &QAbstractItemModel::rowsInserted,
                this, &cwLocalProjectionManager::evaluate, Qt::UniqueConnection);
        connect(model, &QAbstractItemModel::rowsRemoved,
                this, &cwLocalProjectionManager::evaluate, Qt::UniqueConnection);
        connect(model, &QAbstractItemModel::dataChanged,
                this, &cwLocalProjectionManager::evaluate, Qt::UniqueConnection);
        connect(model, &QAbstractItemModel::modelReset,
                this, &cwLocalProjectionManager::evaluate, Qt::UniqueConnection);

        // A cave's name is half of what an anchored fix station is called, and
        // it is the one part of that no input signal reports.
        connect(cave, &cwCave::nameChanged, this,
                &cwLocalProjectionManager::updateAnchorDescription,
                Qt::UniqueConnection);
    }
}
