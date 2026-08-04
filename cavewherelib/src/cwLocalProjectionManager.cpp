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

//Std includes
#include <algorithm>
#include <cmath>

namespace {
    //! How far an input may sit from the origin before the origin counts as
    //! meaningfully wrong. Scale error out here is ~30 ppm — still negligible —
    //! while the mistakes this is meant to catch (a wrong UTM zone, a hemisphere
    //! flip, a transposed digit) miss by far more.
    constexpr double kAnchorThresholdMeters = 50000.0;
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

    // The row signals rather than countChanged: a rescan that swaps one file for
    // another leaves the count alone, and clear() deletes its layers before it
    // announces anything, so the connections have to be reconciled off the model's
    // own structural signals or they outlive what they point at.
    cwLazLayerModel* layers = m_region->lazLayers();
    const auto layersChanged = [this] {
        syncLayerConnections();
        evaluate();
    };
    connect(layers, &QAbstractItemModel::rowsInserted, this, layersChanged);
    connect(layers, &QAbstractItemModel::rowsRemoved, this, layersChanged);
    connect(layers, &QAbstractItemModel::modelReset, this, layersChanged);

    syncCaveConnections();
    syncLayerConnections();
    evaluate();
}

void cwLocalProjectionManager::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    if (!m_loading) {
        evaluate();
    }
}

QList<cwLocalProjectionManager::Input> cwLocalProjectionManager::gatherInputs() const
{
    QList<Input> inputs;

    for (cwCave* cave : m_region->caves()) {
        if (cave == nullptr) {
            continue;
        }
        for (const cwFixStation& fix : cave->fixStations()->fixStations()) {
            // Only a Valid fix has components at all — every other state reads
            // zeros, and a frame centered on a coordinate system's own origin is
            // exactly the "cave in the Gulf of Guinea" failure the LDP exists to
            // make impossible.
            if (fix.state() != cwFixStation::Valid) {
                continue;
            }
            const QString inputCS = fix.inputCS().trimmed();
            if (!cwCoordinateTransform::isValidCS(inputCS)) {
                continue;
            }
            inputs.append(Input{
                cwGeoReference::Anchor{cwGeoReference::Anchor::FixStation, fix.id()},
                inputCS,
                cwGeoPoint(fix.easting(), fix.northing(), fix.elevation())
            });
        }
    }

    cwLazLayerModel* layers = m_region->lazLayers();
    for (int row = 0; row < layers->count(); ++row) {
        cwLazLayer* layer = layers->layerAt(row);
        if (layer == nullptr) {
            continue;
        }
        // Only a finished load has read the file's header, and only its
        // numbers say where the cloud sits in its own CRS. A layer that is
        // disabled, still loading, or failed has no position to offer — and
        // must not be treated as deleted either, which is what m_anchorSeen
        // is for.
        if (layer->loadStatus() != cwLazLayer::LoadStatus::Loaded) {
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

std::optional<double> cwLocalProjectionManager::distanceFromOrigin(const Input& input) const
{
    const QString localCS = m_region->geoReference()->localCoordinateSystem();
    if (localCS.isEmpty()) {
        return std::nullopt;
    }

    const auto local = cwCoordinateTransform::transformPoint(
        input.coordinateSystem, localCS, input.point);
    if (!local.has_value()) {
        return std::nullopt;
    }

    // x_0 = y_0 = 0, so a point's distance from the frame's origin is just its
    // magnitude once it is in the frame.
    return std::hypot(local->x, local->y);
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
    m_lastAnchor = m_region->geoReference()->anchor();
    m_anchorSeen = true;
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
    cwGeoReference* geoReference = m_region->geoReference();
    geoReference->freeze();
    m_lastAnchor = geoReference->anchor();
    m_anchorSeen = false;
}

void cwLocalProjectionManager::clearFrame()
{
    cwGeoReference* geoReference = m_region->geoReference();
    geoReference->clear();
    m_lastAnchor = geoReference->anchor();
    m_anchorSeen = false;
    m_sawAnyInput = false;
}

void cwLocalProjectionManager::evaluate()
{
    if (m_loading) {
        return;
    }

    cwGeoReference* geoReference = m_region->geoReference();

    if (geoReference->anchor() != m_lastAnchor) {
        // Something other than this class moved the anchor — a load restoring
        // what was stored. Whatever the previous anchor had proved about the
        // inputs proves nothing about this one.
        m_lastAnchor = geoReference->anchor();
        m_anchorSeen = false;
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
    }
    // Frozen: a frame with nothing left to follow. Only the user moves it.
}

void cwLocalProjectionManager::syncCaveConnections()
{
    // Connect-only, for the same reason as syncLayerConnections: a destroyed
    // cave drops its own connections, and UniqueConnection makes re-running
    // harmless. Remembering the caves instead would mean holding raw pointers to
    // caves the region has already handed to an undo command that deleteLater()s
    // them, and the row signals rather than countChanged so one setFixStations
    // doesn't evaluate twice.
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
    }
}

void cwLocalProjectionManager::syncLayerConnections()
{
    // Nothing is torn down here, and no layer is remembered between calls.
    // cwLazLayerModel::clear() deletes its layers outright before it announces
    // the reset, so a remembered list of them would already be dangling by the
    // time it could be walked. A destroyed layer drops its own connections, and
    // UniqueConnection makes re-running this harmless.
    cwLazLayerModel* layers = m_region->lazLayers();

    for (int row = 0; row < layers->count(); ++row) {
        cwLazLayer* layer = layers->layerAt(row);
        if (layer == nullptr) {
            continue;
        }
        connect(layer, &cwLazLayer::sourcePathChanged,
                this, &cwLocalProjectionManager::evaluate, Qt::UniqueConnection);
        connect(layer, &cwLazLayer::sourceCSChanged,
                this, &cwLocalProjectionManager::evaluate, Qt::UniqueConnection);
        // A layer becomes an input when its load lands, and stops being one
        // when it's disabled or reloaded. sourceCSChanged alone would miss a
        // reload that resolved to the same CS it had before.
        connect(layer, &cwLazLayer::loadStatusChanged,
                this, &cwLocalProjectionManager::evaluate, Qt::UniqueConnection);
    }
}
