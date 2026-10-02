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
class cwSurveyNode;
class cwErrorListModel;
enum class cwErrorTypeId : int;

/**
 * Finds fix stations whose coordinate is almost certainly a data-entry error
 * (wrong CS, wrong UTM zone, transposed digits) — the kind of mistake that
 * silently blows up the 3D render by inflating the scene bounds until the cave
 * is a sub-pixel dot.
 *
 * "Outlier" is defined relative to the project's frame, so detection is
 * region-scoped: it gathers every cave's fix stations, reprojects them into the
 * project's local projection, and flags any point that sits implausibly far
 * from its origin. Because the LDP is centered on the project's anchor with
 * x_0 = y_0 = 0, that distance is a single hypot of coordinates already in
 * hand — so one fix can be judged as readily as fifty, which is what the
 * cross-cave clustering this replaced could never do.
 * Owned by cwCavingRegion (region.fixStationValidator).
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
    //! warning back to the owning node and row.
    struct FixCandidate {
        cwSurveyNode* node = nullptr;
        QUuid fixId;
        //! Reprojected into the project's local projection; the origin for a
        //! domain outlier, which is judged without the frame.
        cwGeoPoint global;
        //! Whether the fix's raw coordinate is plausible for its own input CS
        //! (Part A). A domain-bad fix is a certain outlier on its own, and Part
        //! B leaves it alone so one bad coordinate raises one warning.
        bool domainValid = true;
    };

    //! Partition of the gathered candidates: the fixes near the project's frame
    //! (inliers), those implausibly far from its origin (outliers), and those
    //! whose coordinate is outside their own CS's valid domain (domain
    //! outliers — flagged on their own, independent of the frame).
    //!
    //! Only the two outlier lists raise warnings. inliers completes the
    //! partition so a test can assert which fixes were cleared by name rather
    //! than by subtraction — say so before deleting it as unread.
    struct Classification {
        QList<FixCandidate> inliers;
        QList<FixCandidate> outliers;
        QList<FixCandidate> domainOutliers;
    };

    explicit cwFixStationValidator(cwCavingRegion* region);

    //! Pure classification of already-gathered candidates: no region, no side
    //! effects, so the threshold is unit-testable with hand-built input.
    //! Candidates are expected in the project's local projection, where a
    //! candidate is an outlier once its distance from the origin passes the
    //! threshold. Each fix is judged alone, so the count of candidates never
    //! changes any one verdict.
    static Classification classifyCandidates(const QList<FixCandidate>& candidates);

    //! Gather every node's fix stations, reproject into the project's frame, and
    //! classify. The region-bound wrapper over classifyCandidates(). Domain
    //! outliers are found with or without a frame; the distance check runs only
    //! once the project is georeferenced.
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

    void syncNodeConnections();

    //! One node's warning of one kind: its text, and the first fix it names,
    //! which is where a click on the warning takes the user.
    struct NodeWarning {
        QString message;
        QUuid fixId;
    };

    //! Per-node FixStationReference warning: the fixes whose station name
    //! matches no station in that node's survey network, has none, names a
    //! station an attached file fixes itself, or repeats an earlier row's
    //! station, joined into one warning. Nodes with no broken reference are
    //! absent from the map (their warning clears).
    QHash<cwSurveyNode*, NodeWarning> referenceWarnings() const;

    //! Set (or, with an empty message, clear) the node's Warning row for one of
    //! our stable errorTypeIds. Each id owns its own row, so the distance and
    //! the domain warnings coexist and are suppressed independently.
    void setNodeWarning(cwSurveyNode* node, cwErrorTypeId errorTypeId, const NodeWarning& warning);

    //! Keep m_nodesWithWarning in step with the node's error rows after a
    //! setNodeWarning: a node stays tracked while it carries any of our
    //! warnings, and drops out once the last one clears.
    void updateWarningTracking(cwSurveyNode* node, cwErrorListModel* errors);

    //! \a node is the offender m_warningMessage names; the QML-facing
    //! firstOutlierCave is that node as a cwCave.
    void setSummary(const QString& message, int count, cwSurveyNode* node);

    cwCavingRegion* m_region = nullptr;

    QString m_warningMessage;
    int m_outlierCount = 0;

    //! The cave named in m_warningMessage. Raw pointer, kept consistent by
    //! revalidate() running synchronously whenever caves change — QML never
    //! observes it between a cave's removal and the refresh.
    cwCave* m_firstOutlierCave = nullptr;

    //! Nodes whose fixStations we hold live connections to, so a node leaving the
    //! region can be torn down (and its warning cleared) before it is destroyed.
    QSet<cwSurveyNode*> m_connectedNodes;

    //! Nodes that currently carry our outlier Warning. The row itself is located
    //! in the node's errorModel by its stable errorTypeId, so no cwError copy is
    //! mirrored here — a copy would stop matching the row once the user suppresses
    //! it (cwError equality includes the suppressed flag).
    QSet<cwSurveyNode*> m_nodesWithWarning;
};

#endif // CWFIXSTATIONVALIDATOR_H
