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
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwFixStation.h"
#include "cwFixStationDiagnosticsModel.h"
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

// Below this many domain-valid fixes there is no cluster to judge an outlier
// against. Kept small (3, not 4) so the common "one cluster + one straggler
// cave" shape is caught: with an odd count the component-wise median lands on
// the majority. The pathological even balanced split (2 vs 2, no majority) is
// covered by the per-fix domain check instead. The cluster rule is best-effort
// for small N; the domain check (Part A) is the reliable path.
constexpr int kMinFixesForDetection = 3;

// Our two Warning kinds are identified by stable ids from the cwErrorTypeId
// registry, so a user's suppression survives the message text changing across
// versions (see cwError::errorTypeId) and each kind is suppressed independently.
// FixStationOutlier is the cross-cave cluster straggler; FixStationDomain is a
// coordinate outside its own CS's valid range (a transposed digit or wrong
// zone), which needs no cluster to prove.

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

QList<int> cwFixStationValidator::fixStationErrorTypeIds()
{
    return {
        static_cast<int>(cwErrorTypeId::FixStationOutlier),
        static_cast<int>(cwErrorTypeId::FixStationDomain),
        static_cast<int>(cwErrorTypeId::FixStationReference),
    };
}

cwFixStationValidator::Classification
cwFixStationValidator::classifyCandidates(const QList<FixCandidate>& candidates)
{
    Classification result;

    // Part A (primary): a fix whose coordinate is implausible for its own CS is
    // a certain outlier on its own — no cluster needed. Pull these out first so
    // a wild typo can't drag the median center off the real data and mask a
    // second, subtler straggler.
    QList<FixCandidate> clusterCandidates;
    clusterCandidates.reserve(candidates.size());
    for (const auto& c : candidates) {
        if (c.domainValid) {
            clusterCandidates.append(c);
        } else {
            result.domainOutliers.append(c);
        }
    }

    // Part B (secondary): the cross-cave cluster rule. Below the minimum there
    // is no majority to place the median center against, so nothing is judged.
    if (clusterCandidates.size() < kMinFixesForDetection) {
        result.inliers = clusterCandidates;
        return result;
    }

    const cwGeoPoint center = medianCenter(clusterCandidates);

    QList<double> distances;
    distances.reserve(clusterCandidates.size());
    for (const auto& c : clusterCandidates) {
        distances.append(distance(c.global, center));
    }

    const double threshold = (std::max)(kOutlierFloorMeters, kMadMultiplier * median(distances));

    for (qsizetype i = 0; i < clusterCandidates.size(); ++i) {
        if (distances.at(i) > threshold) {
            result.outliers.append(clusterCandidates.at(i));
        } else {
            result.inliers.append(clusterCandidates.at(i));
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

            // Part A: does the raw coordinate even belong to its own CS? This is
            // independent of the global CS and of any cluster, so it catches the
            // single-cave, single-fix typo the cluster rule structurally can't.
            const bool domainValid = cwCoordinateTransform::isWithinDomain(inputCS, p);

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
            candidates.append(FixCandidate{cave, fix.id(), global, domainValid});
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

    // The reference the cluster outliers are "off" from: the inlier centroid —
    // the same robust origin the render centers on, so the reported distance is
    // the gap the user would see between the bad station and the rest.
    const cwGeoPoint center = centroid(classification.inliers);

    // Cluster-outlier messages (Part B): one "«station» (~N km)" fragment list
    // per offending cave.
    QHash<cwCave*, QStringList> clusterParts;
    for (const auto& c : classification.outliers) {
        if (c.cave == nullptr) {
            continue;
        }
        const QString name = stationNameFor(c.cave, c.fixId);
        const double km = distance(c.global, center) / 1000.0;
        clusterParts[c.cave].append(QStringLiteral("\"%1\" (~%2 km)").arg(name).arg(km, 0, 'f', 0));
    }

    QHash<cwCave*, QString> clusterMessages;
    for (auto it = clusterParts.constBegin(); it != clusterParts.constEnd(); ++it) {
        const QStringList& fragments = it.value();
        const QString stationWord = fragments.size() == 1
            ? QStringLiteral("station") : QStringLiteral("stations");
        const QString verb = fragments.size() == 1
            ? QStringLiteral("is") : QStringLiteral("are");
        clusterMessages.insert(it.key(),
            QStringLiteral("Fix %1 %2 %3 far from the rest of the survey — "
                           "check the coordinate system, UTM zone, and value.")
                .arg(stationWord, fragments.join(QStringLiteral(", ")), verb));
    }

    // Domain-outlier messages (Part A): fixes outside their own CS's valid range.
    QHash<cwCave*, QStringList> domainParts;
    for (const auto& c : classification.domainOutliers) {
        if (c.cave == nullptr) {
            continue;
        }
        domainParts[c.cave].append(QStringLiteral("\"%1\"").arg(stationNameFor(c.cave, c.fixId)));
    }

    QHash<cwCave*, QString> domainMessages;
    for (auto it = domainParts.constBegin(); it != domainParts.constEnd(); ++it) {
        const QStringList& fragments = it.value();
        const QString message = fragments.size() == 1
            ? QStringLiteral("Fix station %1 has a coordinate outside the valid range for its "
                             "coordinate system — check for a transposed digit or the wrong CS/zone.")
                  .arg(fragments.first())
            : QStringLiteral("Fix stations %1 have coordinates outside the valid range for their "
                             "coordinate system — check for a transposed digit or the wrong CS/zone.")
                  .arg(fragments.join(QStringLiteral(", ")));
        domainMessages.insert(it.key(), message);
    }

    // Reference messages: fixes whose station name matches no survey station in
    // the owning cave, plus fixes with no name at all (both are anchors survex
    // silently drops). Independent of the global CS and the cluster/domain math,
    // so it runs even before an output CS is set. A cave whose network hasn't
    // been computed yet is skipped by the classifier for its *named* fixes.
    const QHash<cwCave*, QString> referenceMessages = referenceWarnings();

    // Reconcile every warning kind against every cave that has or had one, so a
    // correction clears the old warning.
    QSet<cwCave*> caves(m_connectedCaves);
    caves.unite(m_cavesWithWarning);
    for (cwCave* cave : caves) {
        setCaveWarning(cave, cwErrorTypeId::FixStationOutlier, clusterMessages.value(cave));
        setCaveWarning(cave, cwErrorTypeId::FixStationDomain, domainMessages.value(cave));
        setCaveWarning(cave, cwErrorTypeId::FixStationReference, referenceMessages.value(cave));
    }

    // Region-wide summary for the render-view overlay. A domain-bad fix is the
    // most certain error, so it names the culprit first; otherwise the first
    // cluster outlier in region order (both lists follow caves() order). The
    // message stays generic so it covers either kind.
    cwCave* firstOffender = nullptr;
    for (const auto& c : classification.domainOutliers) {
        if (c.cave != nullptr) {
            firstOffender = c.cave;
            break;
        }
    }
    if (firstOffender == nullptr) {
        for (const auto& c : classification.outliers) {
            if (c.cave != nullptr) {
                firstOffender = c.cave;
                break;
            }
        }
    }
    const int total = int(classification.domainOutliers.size() + classification.outliers.size());
    QString summary;
    if (firstOffender != nullptr) {
        summary = QStringLiteral("Part of your survey is off-screen — a fix-station "
                                 "coordinate in \"%1\" looks wrong.")
                      .arg(firstOffender->name());
    }
    setSummary(summary, total, firstOffender);

    updateOutputCSPrompt();
}

QHash<cwCave*, QString> cwFixStationValidator::referenceWarnings() const
{
    QHash<cwCave*, QString> messages;
    if (m_region == nullptr) {
        return messages;
    }

    for (cwCave* cave : m_region->caves()) {
        if (cave == nullptr || cave->fixStations() == nullptr) {
            continue;
        }
        const cwSurveyNetwork network = cave->network();
        QStringList unknownNames;
        int emptyCount = 0;
        for (const cwFixStation& fix : cave->fixStations()->fixStations()) {
            switch (cwFixStationDiagnosticsModel::classifyStationReference(fix.stationName(), network)) {
            case cwFixStationDiagnosticsModel::StationReference::Unknown:
                unknownNames.append(QStringLiteral("\"%1\"").arg(fix.stationName().trimmed()));
                break;
            case cwFixStationDiagnosticsModel::StationReference::Empty:
                ++emptyCount;
                break;
            case cwFixStationDiagnosticsModel::StationReference::Ok:
                break;
            }
        }

        // Each broken category gets its own sentence; survex silently drops
        // both, so the phrasing tells the user the fix is being ignored.
        QStringList parts;
        if (!unknownNames.isEmpty()) {
            parts.append(unknownNames.size() == 1
                ? QStringLiteral("Fix station %1 names a survey station that doesn't exist in "
                                 "this cave — the fix is ignored until the name matches a station.")
                      .arg(unknownNames.first())
                : QStringLiteral("Fix stations %1 name survey stations that don't exist in this "
                                 "cave — the fixes are ignored until the names match stations.")
                      .arg(unknownNames.join(QStringLiteral(", "))));
        }
        if (emptyCount > 0) {
            parts.append(emptyCount == 1
                ? QStringLiteral("A fix station has no station name — it is ignored until you "
                                 "enter the survey station it fixes.")
                : QStringLiteral("%1 fix stations have no station name — they are ignored until "
                                 "you enter the survey stations they fix.")
                      .arg(emptyCount));
        }
        if (parts.isEmpty()) {
            continue;
        }
        messages.insert(cave, parts.join(QStringLiteral(" ")));
    }
    return messages;
}

void cwFixStationValidator::updateOutputCSPrompt()
{
    bool needs = false;
    bool coordinateInvalid = false;
    QString suggested;

    // Only prompt while there is no output CS to place the caves; a set global
    // CS clears the state (globalCoordinateSystemChanged re-runs revalidate).
    if (m_region != nullptr
        && m_region->geoReference()->globalCoordinateSystem().trimmed().isEmpty()) {
        // A fix that carries an input CS is the signal that real fix data is
        // being entered (a blank scaffold row has none). The suggestion comes
        // from the first such fix that also has a coordinate: a fix still at the
        // origin counts as "not entered yet", so a freshly picked CS doesn't
        // momentarily read as an invalid coordinate.
        const auto scan = [&]() {
            for (cwCave* cave : m_region->caves()) {
                if (cave == nullptr || cave->fixStations() == nullptr) {
                    continue;
                }
                for (const cwFixStation& fix : cave->fixStations()->fixStations()) {
                    const QString inputCS = fix.inputCS().trimmed();
                    if (inputCS.isEmpty()) {
                        continue;
                    }
                    needs = true;
                    const cwGeoPoint p(fix.easting(), fix.northing(), fix.elevation());
                    if (p.x == 0.0 && p.y == 0.0) {
                        continue;  // coordinate not entered yet
                    }
                    // The first real coordinate decides the suggestion. A
                    // coordinate outside its own CS's valid domain is almost
                    // certainly a data-entry error, so offer no suggestion and
                    // flag it — the prompt grays out and points at the coordinate.
                    if (cwCoordinateTransform::isWithinDomain(inputCS, p)) {
                        suggested = cwCoordinateTransform::deriveProjectedOutputCS(inputCS, p);
                    } else {
                        coordinateInvalid = true;
                    }
                    return;
                }
            }
        };
        scan();
    }

    if (m_needsOutputCS != needs) {
        m_needsOutputCS = needs;
        emit needsOutputCSChanged();
    }
    if (m_suggestedOutputCS != suggested) {
        m_suggestedOutputCS = suggested;
        emit suggestedOutputCSChanged();
    }
    if (m_outputCSCoordinateInvalid != coordinateInvalid) {
        m_outputCSCoordinateInvalid = coordinateInvalid;
        emit outputCSCoordinateInvalidChanged();
    }
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
            setCaveWarning(cave, cwErrorTypeId::FixStationOutlier, QString());
            setCaveWarning(cave, cwErrorTypeId::FixStationDomain, QString());
            setCaveWarning(cave, cwErrorTypeId::FixStationReference, QString());
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
        // The survey network decides which fix references are broken, so a
        // recompute (a station appearing/disappearing) must re-attribute.
        connect(cave, &cwCave::surveyNetworkChanged,
                this, &cwFixStationValidator::revalidate, Qt::UniqueConnection);
        m_connectedCaves.insert(cave);
    }
}

void cwFixStationValidator::setCaveWarning(cwCave* cave, cwErrorTypeId errorTypeId, const QString& message)
{
    if (cave == nullptr || cave->errorModel() == nullptr) {
        return;
    }
    cwErrorListModel* errors = cave->errorModel()->errors();
    const int errorTypeIdValue = static_cast<int>(errorTypeId);

    // Find our warning by its stable errorTypeId, not by value equality: the user
    // can suppress it, and cwError::operator== includes the suppressed flag, so a
    // stored value copy would stop matching the row the moment it is suppressed.
    int row = -1;
    for (int i = 0; i < errors->size(); ++i) {
        if (errors->at(i).errorTypeId() == errorTypeIdValue) {
            row = i;
            break;
        }
    }

    if (message.isEmpty()) {
        if (row >= 0) {
            errors->remove(row);
        }
    } else if (row >= 0) {
        // Text-only change: update the row in place so the cave-list badge, any
        // open delegate, the user's suppression, and warningCount all survive.
        if (errors->at(row).message() != message) {
            errors->setData(errors->index(row), message,
                            static_cast<int>(cwErrorListModel::ErrorRoles::MessageRole));
        }
    } else {
        cwError error(message, cwError::Warning);
        error.setErrorTypeId(errorTypeIdValue);
        errors->append(error);
    }

    updateWarningTracking(cave, errors);
}

void cwFixStationValidator::updateWarningTracking(cwCave* cave, cwErrorListModel* errors)
{
    // A cave stays tracked while it carries either warning kind, so reconcile
    // still visits it to clear the last one; it drops out once both are gone.
    for (int i = 0; i < errors->size(); ++i) {
        const int id = errors->at(i).errorTypeId();
        if (id == static_cast<int>(cwErrorTypeId::FixStationOutlier)
            || id == static_cast<int>(cwErrorTypeId::FixStationDomain)
            || id == static_cast<int>(cwErrorTypeId::FixStationReference)) {
            m_cavesWithWarning.insert(cave);
            return;
        }
    }
    m_cavesWithWarning.remove(cave);
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
