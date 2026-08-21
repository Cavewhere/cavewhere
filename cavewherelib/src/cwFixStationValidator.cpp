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
#include "cwFixStationDiagnostics.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"

//Qt includes
#include <QStringList>

//Std includes
#include <cmath>
#include <optional>

namespace {

// How far a fix may sit from the project's frame before it reads as a typo
// rather than a survey.
//
// The LDP is a transverse Mercator centered on the project's anchor with
// x_0 = y_0 = 0, so a fix's coordinates in that frame already are its offset
// from the origin: the test is one hypot per fix, with no cluster to establish
// and no minimum number of fixes to establish it from. That is what lets a
// two-fix project — the most common shape there is — be judged at all.
//
// The distance itself is a judgment call, bounded from both sides. Below it: a
// region-wide project — a whole karst area, caves a few hundred km apart — is
// ordinary and must stay quiet. Above it: a coordinate typed under the
// neighboring UTM zone lands one zone width away, and a zone is 6° wide — ~470
// km at 45°N, narrowing toward the poles — so reaching much higher would stop
// catching the wrong-zone typo at mid latitudes. Above ~55° a zone is narrower
// than this threshold and the domain check is what catches those. Transposed
// digits and swapped lat/lon land far beyond either bound.
//
// Two independent limits also degrade around here, which is why there is no
// reason to reach higher: float32's 23-bit mantissa puts render jitter at ~5 cm
// at this range, and the frame's own transverse Mercator scale error reaches
// ~0.2% (2 m per km), so a survey this far out is being distorted by the
// projection carrying it. That second figure is this distance read off the same
// curve cwLocalProjection.h quotes near the origin, so it holds only for the
// LDP recipe described there — change the recipe and this needs rereading.
constexpr double kMaxDistanceFromFrameMeters = 400000.0;

// A cave's warning nests two levels of joining, and they are not interchangeable:
// station names are listed inside one sentence, while a cave with more than one
// kind of problem gets one sentence per kind.
constexpr QLatin1String kFragmentSeparator(", ");
constexpr QLatin1String kSentenceSeparator(" ");

// Our two Warning kinds are identified by stable ids from the cwErrorTypeId
// registry, so a user's suppression survives the message text changing across
// versions (see cwError::errorTypeId) and each kind is suppressed independently.
// FixStationOutlier is the fix too far from the project's frame; FixStationDomain
// is a coordinate outside its own CS's valid range (a transposed digit or wrong
// zone), which needs no frame to prove.

//! How far \a p sits from the project's frame origin. The frame has no z origin,
//! so z is the elevation itself — which is what makes a transposed elevation
//! visible here at all, and costs nothing, since no real elevation comes near
//! the threshold.
double distanceFromFrameOrigin(const cwGeoPoint& p)
{
    return std::hypot(p.x, p.y, p.z);
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
    // connections and re-attributes. The frame moving reprojects every fix, so
    // an outlier under the old frame may cease to be one under the new.
    connect(m_region, &cwCavingRegion::caveCountChanged,
            this, [this] { syncCaveConnections(); revalidate(); });
    connect(m_region->geoReference(), &cwGeoReference::localProjectionChanged,
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

    for (const auto& c : candidates) {
        if (!c.domainValid) {
            // Part A: the coordinate is implausible for the CS the fix itself
            // declares, which is certain on its own and says more than a
            // distance does. One bad coordinate earns one warning, so a fix
            // Part A has already claimed is never measured again by Part B.
            result.domainOutliers.append(c);
        } else if (distanceFromFrameOrigin(c.global) > kMaxDistanceFromFrameMeters) {
            // Part B: in-domain for its own CS, but nowhere near the project.
            // The wrong-zone and wrong-hemisphere typos live here — both give a
            // perfectly legal coordinate for somewhere the project is not.
            result.outliers.append(c);
        } else {
            result.inliers.append(c);
        }
    }
    return result;
}

cwFixStationValidator::Classification
cwFixStationValidator::currentClassification() const
{
    // Without a project frame there is no origin to measure a distance from,
    // and fixes entered in different input CSs would be compared as raw
    // coordinates — degrees against meters. Skip classification entirely until
    // the project is georeferenced.
    if (m_region == nullptr || !m_region->geoReference()->hasCoordinateSystem()) {
        return {};
    }
    return classifyCandidates(gatherCandidates());
}

QList<cwFixStationValidator::FixCandidate>
cwFixStationValidator::gatherCandidates() const
{
    QList<FixCandidate> candidates;
    if (m_region == nullptr) {
        return candidates;
    }

    const QString frameCS = m_region->geoReference()->localCoordinateSystem();

    for (cwCave* cave : m_region->caves()) {
        if (cave == nullptr || cave->fixStations() == nullptr) {
            continue;
        }
        for (const cwFixStation& fix : cave->fixStations()->fixStations()) {
            // Only a Valid fix has components — every other state reads 0,
            // because cwFixStation::refresh() zeroes them before it branches.
            // A row that carries a coordinate system but no coordinate yet is
            // what "Mark Station as Fixed" makes, so without this the ordinary
            // half-finished row would be judged at its CS's origin — off the
            // African coast for a geographic CS — and flagged the moment the
            // user created it. Valid also implies a non-empty inputCS.
            if (fix.state() != cwFixStation::Valid) {
                continue;
            }
            const QString inputCS = fix.inputCS().trimmed();
            if (!cwCoordinateTransform::isValidCS(inputCS)) {
                continue;
            }

            const cwGeoPoint p(fix.easting(), fix.northing(), fix.elevation());

            // Part A: does the raw coordinate even belong to its own CS? This is
            // independent of the project's frame, so it still judges the fix the
            // frame was derived from — the one Part B always measures at zero.
            const bool domainValid = cwFixStationDiagnostics::isDomainValid(fix);

            // currentClassification() guarantees frameCS is non-empty. The
            // memoizing form matters here: frameCS is the derived local
            // projection, which no fix's inputCS ever equals, so every fix in
            // every cave needs a real transform — and revalidate() runs on each
            // fix-station edit and each solve.
            const std::optional<cwGeoPoint> global =
                cwCoordinateTransform::transformPoint(inputCS, frameCS, p);
            if (!global.has_value()) {
                continue;
            }
            candidates.append(FixCandidate{cave, fix.id(), *global, domainValid});
        }
    }
    return candidates;
}

namespace {

//! Two whole sentences rather than slotting "station"/"stations" and "is"/"are"
//! into one template — number agreement is not always confined to those words.
//! `parts` must be non-empty.
QString namedCaveWarning(const QStringList& parts,
                         const QString& singular,
                         const QString& plural)
{
    return parts.size() == 1
        ? singular.arg(parts.first())
        : plural.arg(parts.join(kFragmentSeparator));
}

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

    // Distance messages (Part B): one "«station» (~N km)" fragment list per
    // offending cave. The distance is measured from the frame origin, which the
    // rest of the survey sits on — so it is also the gap the user would see
    // between the bad station and everything else.
    QHash<cwCave*, QStringList> distantParts;
    for (const auto& c : classification.outliers) {
        if (c.cave == nullptr) {
            continue;
        }
        const QString name = stationNameFor(c.cave, c.fixId);
        const double km = distanceFromFrameOrigin(c.global) / 1000.0;
        distantParts[c.cave].append(QStringLiteral("\"%1\" (~%2 km)").arg(name).arg(km, 0, 'f', 0));
    }

    QHash<cwCave*, QString> distantMessages;
    for (auto it = distantParts.constBegin(); it != distantParts.constEnd(); ++it) {
        distantMessages.insert(it.key(),
            namedCaveWarning(it.value(),
                QStringLiteral("Fix station %1 is far from the rest of the survey — "
                               "check the coordinate system, UTM zone, and value."),
                QStringLiteral("Fix stations %1 are far from the rest of the survey — "
                               "check the coordinate system, UTM zone, and value.")));
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
        domainMessages.insert(it.key(),
            namedCaveWarning(it.value(),
                QStringLiteral("Fix station %1 has a coordinate outside the valid range for its "
                               "coordinate system — check for a transposed digit or the wrong CS/zone."),
                QStringLiteral("Fix stations %1 have coordinates outside the valid range for their "
                               "coordinate system — check for a transposed digit or the wrong CS/zone.")));
    }

    // Reference messages: fixes whose station name matches no survey station in
    // the owning cave, plus fixes with no name at all (both are anchors survex
    // silently drops). Independent of the project frame and the distance/domain
    // math, so it runs even before the project is georeferenced. A cave whose
    // network hasn't been computed yet is skipped for its *named* fixes.
    const QHash<cwCave*, QString> referenceMessages = referenceWarnings();

    // Reconcile every warning kind against every cave that has or had one, so a
    // correction clears the old warning.
    QSet<cwCave*> caves(m_connectedCaves);
    caves.unite(m_cavesWithWarning);
    for (cwCave* cave : caves) {
        setCaveWarning(cave, cwErrorTypeId::FixStationOutlier, distantMessages.value(cave));
        setCaveWarning(cave, cwErrorTypeId::FixStationDomain, domainMessages.value(cave));
        setCaveWarning(cave, cwErrorTypeId::FixStationReference, referenceMessages.value(cave));
    }

    // Region-wide summary for the render-view overlay. A domain-bad fix is the
    // most certain error, so it names the culprit first; otherwise the first
    // distance outlier in region order (both lists follow caves() order). The
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
            switch (cwFixStationDiagnostics::classifyStationReference(fix.stationName(), network)) {
            case cwFixStationDiagnostics::StationReference::Unknown:
                unknownNames.append(QStringLiteral("\"%1\"").arg(fix.stationName().trimmed()));
                break;
            case cwFixStationDiagnostics::StationReference::Empty:
                ++emptyCount;
                break;
            case cwFixStationDiagnostics::StationReference::Ok:
                break;
            }
        }

        // Each broken category gets its own sentence; survex silently drops
        // both, so the phrasing tells the user the fix is being ignored.
        QStringList parts;
        if (!unknownNames.isEmpty()) {
            parts.append(namedCaveWarning(unknownNames,
                QStringLiteral("Fix station %1 names a survey station that doesn't exist in "
                               "this cave — the fix is ignored until the name matches a station."),
                QStringLiteral("Fix stations %1 name survey stations that don't exist in this "
                               "cave — the fixes are ignored until the names match stations.")));
        }
        // Counted rather than named — an unnamed fix has nothing to list it by —
        // so this stays hand-rolled: its singular form takes no argument at all,
        // and namedCaveWarning() interpolates unconditionally.
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
        messages.insert(cave, parts.join(kSentenceSeparator));
    }
    return messages;
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
