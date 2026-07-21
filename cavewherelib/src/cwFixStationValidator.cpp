/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwFixStationValidator.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"

//Std includes
#include <algorithm>
#include <cmath>

namespace {

// A float's 23-bit mantissa gives a relative precision of 2^-23; the absolute
// error at coordinate magnitude d is d * this. Holding render jitter under a
// 1 cm budget caps the usable distance of any vertex from the world origin —
// beyond it, a coordinate is both a rendering problem and almost certainly a
// typo.
constexpr double kFloatPrecisionBudgetMeters = 0.01;
constexpr double kFloatMantissaRelPrecision = 1.0 / double(1 << 23);
constexpr double kOutlierFloorMeters = kFloatPrecisionBudgetMeters / kFloatMantissaRelPrecision;

// A fix must also be this many cluster-radii from the center to be flagged, so
// a legitimately spread-out survey (large radius) never trips on the floor.
constexpr double kMadMultiplier = 6.0;

// Below this many fixes there is no cluster to judge an outlier against.
constexpr int kMinFixesForDetection = 4;

double distance(const cwGeoPoint& a, const cwGeoPoint& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Median of a copy (leaves the caller's order intact). Averages the two middle
// values for an even count.
double median(QList<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const qsizetype n = values.size();
    if (n % 2 == 1) {
        return values.at(n / 2);
    }
    return 0.5 * (values.at(n / 2 - 1) + values.at(n / 2));
}

// Component-wise median center: robust to one bad axis, which is the common
// typo shape (a single corrupted coordinate). Cheaper than the geometric
// median and sufficient for separating a straggler from the cluster.
cwGeoPoint medianCenter(const QList<cwFixStationValidator::FixCandidate>& candidates)
{
    QList<double> xs;
    QList<double> ys;
    QList<double> zs;
    xs.reserve(candidates.size());
    ys.reserve(candidates.size());
    zs.reserve(candidates.size());
    for (const auto& c : candidates) {
        xs.append(c.global.x);
        ys.append(c.global.y);
        zs.append(c.global.z);
    }
    return cwGeoPoint{median(xs), median(ys), median(zs)};
}

} // namespace

cwFixStationValidator::cwFixStationValidator(cwCavingRegion* region) :
    QObject(region),
    m_region(region)
{
}

cwFixStationValidator::Classification
cwFixStationValidator::classifyCandidates(const QList<FixCandidate>& candidates)
{
    Classification result;

    if (candidates.size() < kMinFixesForDetection) {
        result.inliers = candidates;
        return result;
    }

    const cwGeoPoint center = medianCenter(candidates);

    QList<double> distances;
    distances.reserve(candidates.size());
    for (const auto& c : candidates) {
        distances.append(distance(c.global, center));
    }

    const double threshold = std::max(kOutlierFloorMeters, kMadMultiplier * median(distances));

    for (qsizetype i = 0; i < candidates.size(); ++i) {
        if (distances.at(i) > threshold) {
            result.outliers.append(candidates.at(i));
        } else {
            result.inliers.append(candidates.at(i));
        }
    }
    return result;
}

cwFixStationValidator::Classification
cwFixStationValidator::currentClassification() const
{
    return classifyCandidates(gatherCandidates());
}

QList<cwFixStationValidator::FixCandidate>
cwFixStationValidator::gatherCandidates() const
{
    QList<FixCandidate> candidates;
    if (m_region == nullptr) {
        return candidates;
    }

    const QString globalCSTrimmed = m_region->geoReference()->globalCoordinateSystem().trimmed();

    for (cwCave* cave : m_region->caves()) {
        if (cave == nullptr || cave->fixStations() == nullptr) {
            continue;
        }
        for (const cwFixStation& fix : cave->fixStations()->fixStations()) {
            QString inputCS = fix.inputCS().trimmed();
            if (inputCS.isEmpty()) {
                inputCS = globalCSTrimmed;
            }
            if (inputCS.isEmpty() || !cwCoordinateTransform::isValidCS(inputCS)) {
                continue;
            }

            const cwGeoPoint p(fix.easting(), fix.northing(), fix.elevation());

            cwGeoPoint global;
            if (globalCSTrimmed.isEmpty()
                || inputCS.compare(globalCSTrimmed, Qt::CaseInsensitive) == 0) {
                global = p;
            } else {
                cwCoordinateTransform t(inputCS, globalCSTrimmed);
                if (!t.isValid()) {
                    continue;
                }
                global = t.transform(p);
            }
            candidates.append(FixCandidate{cave, fix.id(), global});
        }
    }
    return candidates;
}
