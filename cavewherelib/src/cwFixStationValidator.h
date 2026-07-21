/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFIXSTATIONVALIDATOR_H
#define CWFIXSTATIONVALIDATOR_H

//Qt includes
#include <QObject>
#include <QList>
#include <QUuid>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
#include "cwGeoPoint.h"

class cwCave;
class cwCavingRegion;

/**
 * Finds fix stations whose coordinate is almost certainly a data-entry error
 * (wrong CS, wrong UTM zone, transposed digits) — the kind of mistake that
 * silently blows up the 3D render by dragging the world origin off the real
 * data and inflating the scene bounds until the cave is a sub-pixel dot.
 *
 * "Outlier" is defined relative to the whole survey, so detection is
 * region-scoped: it gathers every cave's fix stations, reprojects them into the
 * region's global CS, and flags any point that sits far from the cluster of all
 * the others. Owned by cwCavingRegion (region.fixStationValidator) — the region
 * keeps only the world-origin write and delegates the fix-station geometry here.
 */
class CAVEWHERE_LIB_EXPORT cwFixStationValidator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FixStationValidator)
    QML_UNCREATABLE("Owned by CavingRegion; access via region.fixStationValidator")

public:
    //! One reprojected fix station plus the provenance needed to attribute a
    //! warning back to the owning cave and row.
    struct FixCandidate {
        cwCave* cave = nullptr;
        QUuid fixId;
        cwGeoPoint global;  //!< reprojected into the region's global CS
    };

    //! Partition of the gathered candidates into the tight cluster (inliers)
    //! and the isolated stragglers (outliers).
    struct Classification {
        QList<FixCandidate> inliers;
        QList<FixCandidate> outliers;
    };

    explicit cwFixStationValidator(cwCavingRegion* region);

    //! Pure classification of already-gathered candidates: no region, no side
    //! effects, so the threshold math is unit-testable with hand-built input. A
    //! candidate is an outlier when its distance from the component-wise median
    //! center exceeds both the float-precision floor and a multiple of the
    //! cluster's median radius — so a legitimately spread-out survey (large
    //! radius) flags nothing, and a point too close to matter (below the floor)
    //! flags nothing.
    static Classification classifyCandidates(const QList<FixCandidate>& candidates);

    //! Gather every cave's fix stations, reproject into the region's global CS,
    //! and classify. The region-bound wrapper over classifyCandidates().
    Classification currentClassification() const;

private:
    QList<FixCandidate> gatherCandidates() const;

    cwCavingRegion* m_region = nullptr;
};

#endif // CWFIXSTATIONVALIDATOR_H
