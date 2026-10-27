/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSOURCESUBTREEFILL_H
#define CWSOURCESUBTREEFILL_H

//Our includes
#include "cwExternalCenterlineScanner.h"
#include "cwFutureManagerToken.h"
#include "cwGlobals.h"
#include "cwPhaseJob.h"

//Qt includes
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

//Std includes
#include <memory>

class cwSurveyNode;
class cwTrip;

/**
 * Brings the derived nodes and trips under a source root in line with the
 * scan's pre-order rows:
 *  - a node row matches the derived node with its sourcePath, or creates one;
 *  - a unit row matches the derived trip with
 *    cwExternalCenterlineAttach::unitSourcePath(row), or creates one, its date
 *    seeded from the row (a dateless row takes its nearest dated ancestor's).
 *    A node-and-unit row's trip sits under the row's node; a unit-only row's (a
 *    leaf Survex block, a Compass survey, a Walls .srv) under its parent's
 *    node, so a leaf Survex block is one trip;
 *  - a missing file's row is skipped, and every row below it;
 *  - a derived trip no row matches is removed when it holds no native content
 *    (chunks, notes, LiDAR scans, sketches); a derived node no row matches is
 *    removed when its subtree holds none, and otherwise stays and turns native;
 *  - native nodes and trips (no source path) are never touched.
 *
 * start() matches and plans in the calling turn. The new rows then install a
 * batch per event-loop turn: each batch is whole new subtrees, built detached,
 * that land under one existing node with one addNodes(), or new trips that land
 * with one addTrips(), so the tree fills in over several frames under a
 * task-list row whose progress counts the batches. The removals run in the
 * last turn, after every install, so a new name dedupes against a stale
 * sibling. A fill with nothing to install finishes inside start().
 *
 * The fill records no undo step: the root's undo stack is cleared for its
 * duration and handed back from the root's parent at the end.
 *
 * A root that leaves its region mid-fill ends the fill with finished(false).
 * Each turn builds only what it installs, so an ended fill leaves nothing
 * detached behind. The fill deletes itself after finished().
 */
class CAVEWHERE_LIB_EXPORT cwSourceSubtreeFill : public QObject
{
    Q_OBJECT

public:
    cwSourceSubtreeFill(cwSurveyNode* root, QObject* parent);
    ~cwSourceSubtreeFill() override;

    //! Plans \a rows against the root's subtree and starts the install. The
    //! task-list row is named \a jobName and registered through \a token.
    void start(const QList<cwScanRow>& rows,
               const QString& jobName,
               const cwFutureManagerToken& token);

signals:
    //! True once the last batch and the removals have landed; false when the
    //! root left its region first.
    void finished(bool installed);

private slots:
    void installNextBatch();

private:
    struct Plan;

    void plan(const QList<cwScanRow>& rows);
    bool rootInRegion() const;
    void removeStale();
    void finish(bool installed);

    QPointer<cwSurveyNode> m_root;
    std::unique_ptr<Plan> m_plan;
    int m_nextBatch = 0;
    cwPhaseJob m_job;
};

#endif // CWSOURCESUBTREEFILL_H
