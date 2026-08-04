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
#include <QHash>
#include <QList>
#include <QSet>
#include <QUuid>
#include <QQmlEngine>

//Std includes

//Our includes
#include "cwGlobals.h"
#include "cwGeoPoint.h"

class cwCave;
class cwCavingRegion;
class cwErrorListModel;
enum class cwErrorTypeId : int;

/**
 * Finds fix stations whose coordinate is almost certainly a data-entry error
 * (wrong CS, wrong UTM zone, transposed digits) — the kind of mistake that
 * silently blows up the 3D render by inflating the scene bounds until the cave
 * is a sub-pixel dot.
 *
 * "Outlier" is defined relative to the whole survey, so detection is
 * region-scoped: it gathers every cave's fix stations, reprojects them into the
 * project's local projection, and flags any point that sits far from the cluster
 * of all the others. Owned by cwCavingRegion (region.fixStationValidator).
 */
class CAVEWHERE_LIB_EXPORT cwFixStationValidator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FixStationValidator)
    QML_UNCREATABLE("Owned by CavingRegion; access via region.fixStationValidator")

    //! Region-wide summary of the current outliers, for the render-view overlay:
    //! empty when nothing is flagged, otherwise a one-line message naming the
    //! first offending cave. outlierCount is the total across all caves, and
    //! firstOutlierCave is that named cave — a routing handle so the overlay can
    //! link the user to its fix stations (null when nothing is flagged).
    Q_PROPERTY(QString warningMessage READ warningMessage NOTIFY warningMessageChanged FINAL)
    Q_PROPERTY(int outlierCount READ outlierCount NOTIFY outlierCountChanged FINAL)
    Q_PROPERTY(cwCave* firstOutlierCave READ firstOutlierCave NOTIFY firstOutlierCaveChanged FINAL)

    //! The stable errorTypeIds this validator pushes onto a cave's errorModel —
    //! the fix-station warning kinds ({596,597,598}). A fix-station badge binds a
    //! cave's errorModel to just these so it stays distinct from the all-warnings
    //! page banner. Constant: the id set is fixed at compile time.
    Q_PROPERTY(QList<int> fixStationErrorTypeIds READ fixStationErrorTypeIds CONSTANT FINAL)

public:
    //! One reprojected fix station plus the provenance needed to attribute a
    //! warning back to the owning cave and row.
    struct FixCandidate {
        cwCave* cave = nullptr;
        QUuid fixId;
        cwGeoPoint global;  //!< reprojected into the project's local projection
        //! Whether the fix's raw coordinate is plausible for its own input CS
        //! (Part A). A domain-bad fix is a certain outlier with no cluster
        //! needed, and is excluded from the cluster math.
        bool domainValid = true;
    };

    //! Partition of the gathered candidates: the tight cluster (inliers), the
    //! stragglers far from it (cluster outliers), and the fixes whose coordinate
    //! is outside their own CS's valid domain (domain outliers — flagged on
    //! their own, independent of any cluster).
    struct Classification {
        QList<FixCandidate> inliers;
        QList<FixCandidate> outliers;
        QList<FixCandidate> domainOutliers;
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

    //! Gather every cave's fix stations, reproject into the project's frame, and
    //! classify. The region-bound wrapper over classifyCandidates().
    Classification currentClassification() const;

    QString warningMessage() const { return m_warningMessage; }
    int outlierCount() const { return m_outlierCount; }
    cwCave* firstOutlierCave() const { return m_firstOutlierCave; }

    static QList<int> fixStationErrorTypeIds();

signals:
    void warningMessageChanged();
    void outlierCountChanged();
    void firstOutlierCaveChanged();

private:
    QList<FixCandidate> gatherCandidates() const;

    //! Reclassify and push a Warning onto each offending cave's errorModel (and
    //! clear it from caves that no longer offend). Driven entirely by this
    //! object's own connections to the caves' fix stations and the region's CS,
    //! so a fix-coordinate edit re-attributes without an external trigger.
    void revalidate();

    void syncCaveConnections();

    //! Per-cave FixStationReference message: the fixes whose station name matches
    //! no station in that cave's survey network, joined into one warning. Caves
    //! with no broken reference are absent from the map (their warning clears).
    QHash<cwCave*, QString> referenceWarnings() const;

    //! Set (or, with an empty message, clear) the cave's Warning row for one of
    //! our stable errorTypeIds. Each id owns its own row, so the cluster-outlier
    //! and the domain warnings coexist and are suppressed independently.
    void setCaveWarning(cwCave* cave, cwErrorTypeId errorTypeId, const QString& message);

    //! Keep m_cavesWithWarning in step with the cave's error rows after a
    //! setCaveWarning: a cave stays tracked while it carries any of our
    //! warnings, and drops out once the last one clears.
    void updateWarningTracking(cwCave* cave, cwErrorListModel* errors);

    void setSummary(const QString& message, int count, cwCave* cave);

    cwCavingRegion* m_region = nullptr;

    QString m_warningMessage;
    int m_outlierCount = 0;

    //! The cave named in m_warningMessage. Raw pointer, kept consistent by
    //! revalidate() running synchronously whenever caves change — QML never
    //! observes it between a cave's removal and the refresh.
    cwCave* m_firstOutlierCave = nullptr;

    //! Caves whose fixStations we hold live connections to, so a cave leaving the
    //! region can be torn down (and its warning cleared) before it is destroyed.
    QSet<cwCave*> m_connectedCaves;

    //! Caves that currently carry our outlier Warning. The row itself is located
    //! in the cave's errorModel by its stable errorTypeId, so no cwError copy is
    //! mirrored here — a copy would stop matching the row once the user suppresses
    //! it (cwError equality includes the suppressed flag).
    QSet<cwCave*> m_cavesWithWarning;
};

#endif // CWFIXSTATIONVALIDATOR_H
