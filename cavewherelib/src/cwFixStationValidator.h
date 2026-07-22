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
#include <QSet>
#include <QUuid>
#include <QQmlEngine>

//Std includes
#include <optional>

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

    //! Region-wide summary of the current outliers, for the render-view overlay:
    //! empty when nothing is flagged, otherwise a one-line message naming the
    //! first offending cave. outlierCount is the total across all caves, and
    //! firstOutlierCave is that named cave — a routing handle so the overlay can
    //! link the user to its fix stations (null when nothing is flagged).
    Q_PROPERTY(QString warningMessage READ warningMessage NOTIFY warningMessageChanged FINAL)
    Q_PROPERTY(int outlierCount READ outlierCount NOTIFY outlierCountChanged FINAL)
    Q_PROPERTY(cwCave* firstOutlierCave READ firstOutlierCave NOTIFY firstOutlierCaveChanged FINAL)

    //! First-fix output-CS prompt: a project with fix stations but no global
    //! (output) CS can't place its caves — the line plot renders around an
    //! untouched (0,0,0) origin, so the cave lands far away. needsOutputCS is
    //! true in exactly that state. suggestedOutputCS is a projected CS derived
    //! from the first fix that carries a real, in-domain coordinate — empty when
    //! no coordinate has been entered yet, or when it is invalid (see below). The
    //! prompt pre-fills its coordinate-system picker with it.
    //! outputCSCoordinateInvalid is true when that first real coordinate falls
    //! outside its own input CS's valid domain (almost certainly a data-entry
    //! error): no suggestion can be trusted, so the prompt grays out its picker
    //! and points the user at the coordinate instead.
    Q_PROPERTY(bool needsOutputCS READ needsOutputCS NOTIFY needsOutputCSChanged FINAL)
    Q_PROPERTY(QString suggestedOutputCS READ suggestedOutputCS NOTIFY suggestedOutputCSChanged FINAL)
    Q_PROPERTY(bool outputCSCoordinateInvalid READ outputCSCoordinateInvalid NOTIFY outputCSCoordinateInvalidChanged FINAL)

public:
    //! One reprojected fix station plus the provenance needed to attribute a
    //! warning back to the owning cave and row.
    struct FixCandidate {
        cwCave* cave = nullptr;
        QUuid fixId;
        cwGeoPoint global;  //!< reprojected into the region's global CS
        //! Whether the fix's raw coordinate is plausible for its own input CS
        //! (Part A). A domain-bad fix is a certain outlier with no cluster
        //! needed, and is excluded from the cluster math and the world origin.
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

    //! Gather every cave's fix stations, reproject into the region's global CS,
    //! and classify. The region-bound wrapper over classifyCandidates().
    Classification currentClassification() const;

    //! The centroid of the inlier fix stations, in the region's global CS — the
    //! world origin to render around. Excluding outliers keeps a single typo'd
    //! fix from dragging the origin off the real data. Empty when there are no
    //! usable candidates, so the caller can leave the origin untouched.
    std::optional<cwGeoPoint> robustWorldOrigin() const;

    QString warningMessage() const { return m_warningMessage; }
    int outlierCount() const { return m_outlierCount; }
    cwCave* firstOutlierCave() const { return m_firstOutlierCave; }

    bool needsOutputCS() const { return m_needsOutputCS; }
    QString suggestedOutputCS() const { return m_suggestedOutputCS; }
    bool outputCSCoordinateInvalid() const { return m_outputCSCoordinateInvalid; }

signals:
    void warningMessageChanged();
    void outlierCountChanged();
    void firstOutlierCaveChanged();
    void needsOutputCSChanged();
    void suggestedOutputCSChanged();
    void outputCSCoordinateInvalidChanged();

private:
    QList<FixCandidate> gatherCandidates() const;

    //! Reclassify and push a Warning onto each offending cave's errorModel (and
    //! clear it from caves that no longer offend). Driven entirely by this
    //! object's own connections to the caves' fix stations and the region's CS,
    //! so a fix-coordinate edit re-attributes without an external trigger.
    void revalidate();

    void syncCaveConnections();

    //! Set (or, with an empty message, clear) the cave's Warning row for one of
    //! our stable errorTypeIds. Each id owns its own row, so the cluster-outlier
    //! and the domain warnings coexist and are suppressed independently.
    void setCaveWarning(cwCave* cave, cwErrorTypeId errorTypeId, const QString& message);

    //! Keep m_cavesWithWarning in step with the cave's error rows after a
    //! setCaveWarning: a cave stays tracked while it carries any of our
    //! warnings, and drops out once the last one clears.
    void updateWarningTracking(cwCave* cave, cwErrorListModel* errors);

    void setSummary(const QString& message, int count, cwCave* cave);

    //! Recompute needsOutputCS/suggestedOutputCS from the current fixes and
    //! global CS. Runs on the same triggers as revalidate() (a fix edit, a cave
    //! joining/leaving, the global CS changing), so the prompt tracks live.
    void updateOutputCSPrompt();

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

    bool m_needsOutputCS = false;
    QString m_suggestedOutputCS;
    bool m_outputCSCoordinateInvalid = false;
};

#endif // CWFIXSTATIONVALIDATOR_H
