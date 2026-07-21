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
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"

//Qt includes
#include <QStringList>

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

// A stable id for the fix-station outlier Warning, so a user's suppression of it
// survives the message text changing across versions (see cwError::errorTypeId).
// No other cwError sets an id today; this reserves one (issue #596).
constexpr int kOutlierErrorTypeId = 596;

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

// Component-wise mean of the candidates' reprojected points. Returns the origin
// for an empty list; callers that must distinguish "no candidates" guard first.
cwGeoPoint centroid(const QList<cwFixStationValidator::FixCandidate>& candidates)
{
    cwGeoPoint sum;
    if (candidates.isEmpty()) {
        return sum;
    }
    for (const auto& c : candidates) {
        sum.x += c.global.x;
        sum.y += c.global.y;
        sum.z += c.global.z;
    }
    const double n = double(candidates.size());
    return cwGeoPoint{sum.x / n, sum.y / n, sum.z / n};
}

} // namespace

cwFixStationValidator::cwFixStationValidator(cwCavingRegion* region) :
    QObject(region),
    m_region(region)
{
    if (m_region == nullptr) {
        return;
    }

    // A cave joining or leaving the region rewires the per-cave fix-station
    // connections and re-attributes. The CS changing reprojects every fix, so
    // an outlier under the old CS may cease to be one under the new.
    connect(m_region, &cwCavingRegion::caveCountChanged,
            this, [this] { syncCaveConnections(); revalidate(); });
    connect(m_region->geoReference(), &cwGeoReference::globalCoordinateSystemChanged,
            this, &cwFixStationValidator::revalidate);

    syncCaveConnections();
    revalidate();
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
    // Without a region global CS there is no common frame to reproject into, so
    // fixes entered in different input CSs would be compared as raw coordinates
    // and a legitimate station could be flagged. Skip classification entirely
    // until a global CS exists.
    if (m_region == nullptr
        || m_region->geoReference()->globalCoordinateSystem().trimmed().isEmpty()) {
        return {};
    }
    return classifyCandidates(gatherCandidates());
}

std::optional<cwGeoPoint> cwFixStationValidator::robustWorldOrigin() const
{
    const QList<FixCandidate> inliers = currentClassification().inliers;
    if (inliers.isEmpty()) {
        return std::nullopt;
    }
    return centroid(inliers);
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

            // currentClassification() guarantees globalCSTrimmed is non-empty.
            cwGeoPoint global;
            if (inputCS.compare(globalCSTrimmed, Qt::CaseInsensitive) == 0) {
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

namespace {

QString stationNameFor(cwCave* cave, const QUuid& fixId)
{
    if (cave == nullptr || cave->fixStations() == nullptr) {
        return QString();
    }
    for (const cwFixStation& fix : cave->fixStations()->fixStations()) {
        if (fix.id() == fixId) {
            return fix.stationName();
        }
    }
    return QString();
}

} // namespace

void cwFixStationValidator::revalidate()
{
    const Classification classification = currentClassification();

    // The reference the outliers are "off" from: the inlier centroid — the same
    // robust origin the render centers on, so the reported distance is the gap
    // the user would see between the bad station and the rest of the survey.
    const cwGeoPoint center = centroid(classification.inliers);

    // Group each cave's outliers into one "«station» (~N km)" fragment list.
    QHash<cwCave*, QStringList> parts;
    for (const auto& c : classification.outliers) {
        if (c.cave == nullptr) {
            continue;
        }
        const QString name = stationNameFor(c.cave, c.fixId);
        const double km = distance(c.global, center) / 1000.0;
        parts[c.cave].append(QStringLiteral("\"%1\" (~%2 km)").arg(name).arg(km, 0, 'f', 0));
    }

    // Compose a message per offending cave, then reconcile against every cave
    // that has or had a warning so corrections clear the old one.
    QHash<cwCave*, QString> messages;
    for (auto it = parts.constBegin(); it != parts.constEnd(); ++it) {
        const QStringList& fragments = it.value();
        const QString stationWord = fragments.size() == 1
            ? QStringLiteral("station") : QStringLiteral("stations");
        const QString verb = fragments.size() == 1
            ? QStringLiteral("is") : QStringLiteral("are");
        messages.insert(it.key(),
            QStringLiteral("Fix %1 %2 %3 far from the rest of the survey — "
                           "check the coordinate system, UTM zone, and value.")
                .arg(stationWord, fragments.join(QStringLiteral(", ")), verb));
    }

    QSet<cwCave*> caves(m_connectedCaves);
    caves.unite(m_cavesWithWarning);
    for (cwCave* cave : caves) {
        setCaveWarning(cave, messages.value(cave));
    }

    // Region-wide summary for the render-view overlay. Name the first offending
    // cave in region order (classification.outliers follows caves() order), so
    // the message is deterministic when more than one cave has an outlier.
    cwCave* firstOffender = nullptr;
    for (const auto& c : classification.outliers) {
        if (c.cave != nullptr) {
            firstOffender = c.cave;
            break;
        }
    }
    QString summary;
    if (firstOffender != nullptr) {
        summary = QStringLiteral("Part of your survey is off-screen — a fix-station "
                                 "coordinate in \"%1\" looks wrong.")
                      .arg(firstOffender->name());
    }
    setSummary(summary, classification.outliers.size(), firstOffender);
}

void cwFixStationValidator::syncCaveConnections()
{
    const QList<cwCave*> current = m_region ? m_region->caves() : QList<cwCave*>();
    const QSet<cwCave*> currentSet(current.begin(), current.end());

    // Caves that left the region: still alive here (a removed cave outlives its
    // removal via the undo command), so tear down its connection and clear its
    // warning before it can be destroyed.
    for (auto it = m_connectedCaves.begin(); it != m_connectedCaves.end();) {
        cwCave* cave = *it;
        if (!currentSet.contains(cave)) {
            if (cave != nullptr) {
                disconnect(cave, nullptr, this, nullptr);
                if (cave->fixStations() != nullptr) {
                    disconnect(cave->fixStations(), nullptr, this, nullptr);
                }
            }
            setCaveWarning(cave, QString());
            it = m_connectedCaves.erase(it);
        } else {
            ++it;
        }
    }

    // New caves: an edit to any of their fix stations should re-attribute.
    for (cwCave* cave : current) {
        if (cave == nullptr || cave->fixStations() == nullptr
            || m_connectedCaves.contains(cave)) {
            continue;
        }
        cwFixStationModel* model = cave->fixStations();
        connect(model, &cwFixStationModel::countChanged,
                this, &cwFixStationValidator::revalidate, Qt::UniqueConnection);
        connect(model, &cwFixStationModel::dataChanged,
                this, &cwFixStationValidator::revalidate, Qt::UniqueConnection);
        connect(model, &cwFixStationModel::modelReset,
                this, &cwFixStationValidator::revalidate, Qt::UniqueConnection);

        // The render-view summary interpolates the offending cave's name, so a
        // rename must re-run to refresh the banner (the per-cave errorModel
        // message carries no name and is unaffected).
        connect(cave, &cwCave::nameChanged,
                this, &cwFixStationValidator::revalidate, Qt::UniqueConnection);
        m_connectedCaves.insert(cave);
    }
}

void cwFixStationValidator::setCaveWarning(cwCave* cave, const QString& message)
{
    if (cave == nullptr || cave->errorModel() == nullptr) {
        return;
    }
    cwErrorListModel* errors = cave->errorModel()->errors();

    // Find our warning by its stable errorTypeId, not by value equality: the user
    // can suppress it, and cwError::operator== includes the suppressed flag, so a
    // stored value copy would stop matching the row the moment it is suppressed.
    int row = -1;
    for (int i = 0; i < errors->size(); ++i) {
        if (errors->at(i).errorTypeId() == kOutlierErrorTypeId) {
            row = i;
            break;
        }
    }

    if (message.isEmpty()) {
        if (row >= 0) {
            errors->remove(row);
        }
        m_cavesWithWarning.remove(cave);
        return;
    }

    if (row >= 0) {
        // Text-only change: update the row in place so the cave-list badge, any
        // open delegate, the user's suppression, and warningCount all survive.
        if (errors->at(row).message() != message) {
            errors->setData(errors->index(row), message,
                            static_cast<int>(cwErrorListModel::ErrorRoles::MessageRole));
        }
        m_cavesWithWarning.insert(cave);
        return;
    }

    cwError error(message, cwError::Warning);
    error.setErrorTypeId(kOutlierErrorTypeId);
    errors->append(error);
    m_cavesWithWarning.insert(cave);
}

void cwFixStationValidator::setSummary(const QString& message, int count, cwCave* cave)
{
    if (m_warningMessage != message) {
        m_warningMessage = message;
        emit warningMessageChanged();
    }
    if (m_outlierCount != count) {
        m_outlierCount = count;
        emit outlierCountChanged();
    }
    if (m_firstOutlierCave != cave) {
        m_firstOutlierCave = cave;
        emit firstOutlierCaveChanged();
    }
}
