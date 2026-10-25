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
#include <QPointer>
#include <QSet>
#include <QStringList>
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
 * One unsuppressed fix-station warning and the node whose errorModel carries
 * it. The message is the node's own warning text, without the name prefix
 * cwFixStationValidator::fixStationWarningMessages() adds.
 */
class CAVEWHERE_LIB_EXPORT cwFixStationWarning
{
    Q_GADGET
    QML_VALUE_TYPE(cwFixStationWarning)

    Q_PROPERTY(cwSurveyNode* node READ node FINAL)
    Q_PROPERTY(QString message READ message FINAL)

public:
    cwFixStationWarning() = default;
    cwFixStationWarning(cwSurveyNode* node, const QString& message);

    cwSurveyNode* node() const;
    QString message() const { return m_message; }

    bool operator==(const cwFixStationWarning& other) const;

private:
    QPointer<cwSurveyNode> m_node;
    QString m_message;
};

//QML reads each entry's node to filter by subtree, so the list is registered as
//a QML sequence. See cwExternalSourceAttentionRowListRegistration.
class cwFixStationWarningListRegistration
{
    Q_GADGET
    QML_FOREIGN(QList<cwFixStationWarning>)
    QML_ANONYMOUS
    QML_SEQUENTIAL_CONTAINER(cwFixStationWarning)
};

Q_DECLARE_METATYPE(cwFixStationWarning)

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

    //! Every unsuppressed fix-station warning in the project, in tree pre-order,
    //! each tagged with its node. fixStationWarningMessages is the same list as
    //! "<node name>: <message>" lines, for the Data page's badge; a node page's
    //! badge shows the subset from fixStationWarningMessagesUnder().
    Q_PROPERTY(QList<cwFixStationWarning> fixStationWarnings READ fixStationWarnings NOTIFY fixStationWarningsChanged FINAL)
    Q_PROPERTY(QStringList fixStationWarningMessages READ fixStationWarningMessages NOTIFY fixStationWarningsChanged FINAL)

    //! The stable errorTypeIds this validator pushes onto a cave's errorModel —
    //! the fix-station warning kinds (FixStationOutlier,
    //! FixStationDomain, FixStationReference). A fix-station badge binds a
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
    cwCave* firstOutlierCave() const;

    //! The errorTypeIds this validator pushes onto a node's errorModel: the
    //! fix-station warning kinds.
    static QList<int> fixStationErrorTypeIds();

    QList<cwFixStationWarning> fixStationWarnings() const { return m_fixStationWarnings; }
    QStringList fixStationWarningMessages() const { return m_fixStationWarningMessages; }

    //! The fixStationWarningMessages lines whose node is \a node or one of its
    //! descendants. Changes only alongside fixStationWarningsChanged.
    Q_INVOKABLE QStringList fixStationWarningMessagesUnder(cwSurveyNode* node) const;

signals:
    void warningMessageChanged();
    void outlierCountChanged();
    void firstOutlierCaveChanged();
    void fixStationWarningsChanged();

private:
    QList<FixCandidate> gatherCandidates() const;

    //! Reclassify and push a Warning onto each offending cave's errorModel (and
    //! clear it from caves that no longer offend). Driven entirely by this
    //! object's own connections to the caves' fix stations and the region's CS,
    //! so a fix-coordinate edit re-attributes without an external trigger.
    void revalidate();
    //! revalidate()'s body: writes every node's warning rows.
    void revalidateNodes();

    //! Queue one revalidate() for this event-loop turn: a tree edit, an attach
    //! or a scan apply touches many nodes, and each pass walks the whole
    //! region. A synchronous revalidate() in between covers the queued one,
    //! which then does nothing.
    void scheduleRevalidate();
    void runPendingRevalidate();

    //! A node joined or left the tree: the next revalidate() rewires the
    //! per-node connections first.
    void markNodeConnectionsStale();
    void syncNodeConnections();
    //! Drops a destroyed node from the tracking sets by pointer value alone, so
    //! a node destroyed before the queued pass leaves no dangling pointer for
    //! syncNodeConnections() to tear down.
    void forgetNode(QObject* node);

    //! Collects fixStationWarnings() from the nodes' error lists. Also runs on a
    //! suppression, which changes a list without a revalidate().
    void rebuildFixStationWarnings();

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

    //! The cave named in m_warningMessage. Null once that cave is destroyed,
    //! until the queued pass names the next one.
    QPointer<cwCave> m_firstOutlierCave;

    //! Nodes whose fixStations we hold live connections to, so a node leaving the
    //! region can be torn down (and its warning cleared) before it is destroyed.
    QSet<cwSurveyNode*> m_connectedNodes;

    //! Nodes that currently carry our outlier Warning. The row itself is located
    //! in the node's errorModel by its stable errorTypeId, so no cwError copy is
    //! mirrored here — a copy would stop matching the row once the user suppresses
    //! it (cwError equality includes the suppressed flag).
    QSet<cwSurveyNode*> m_nodesWithWarning;

    QList<cwFixStationWarning> m_fixStationWarnings;
    QStringList m_fixStationWarningMessages;

    //! Set while revalidate() writes warnings, so the list is rebuilt once per
    //! pass rather than once per written row.
    bool m_isRevalidating = false;

    //! Whether a scheduleRevalidate() is still waiting for its turn.
    bool m_revalidatePending = false;
    bool m_nodeConnectionsStale = false;
    bool m_treeChangedSinceRebuild = false;
};

#endif // CWFIXSTATIONVALIDATOR_H
