/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSourceSubtreeFill.h"

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterlineAttach.h"
#include "cwNameUtils.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwUnits.h"

//Qt includes
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QMetaObject>
#include <QUuid>

//Std includes
#include <algorithm>
#include <vector>

namespace {

//! The most nodes and trips one event-loop turn installs. A subtree is never
//! split, so a single larger subtree installs alone in its turn.
constexpr int kInstallRowsPerTurn = 256;

struct TripRecord {
    QString name;
    QString sourcePath;
    QDate date;
};

//! A new derived node, planned but not yet built, with everything under it.
struct NodeRecord {
    QString name;
    cwSurveyNodeKind::Kind kind = cwSurveyNodeKind::Kind::Cave;
    QString sourcePath;
    std::vector<std::unique_ptr<NodeRecord>> children;
    QList<TripRecord> trips;

    int rowCount() const
    {
        int rows = 1 + static_cast<int>(trips.size());
        for (const auto& child : children) {
            rows += child->rowCount();
        }
        return rows;
    }
};

/**
 * True when \a trip carries survey data the user entered: chunks, or notes
 * of any kind (scanned pages and the scraps on them, LiDAR scans,
 * sketches). A window holding any of it outlives its block: the source
 * derives the window, and what the user put in it is theirs.
 */
bool holdsNativeContent(const cwTrip* trip)
{
    return trip->chunkCount() > 0
           || trip->notes()->hasNotes()
           || trip->notesLiDAR()->hasNotes()
           || trip->notesSketch()->hasNotes();
}

/**
 * True when \a node's subtree holds anything the source does not derive: a
 * native trip, a derived trip holding native content, or a node that came from
 * elsewhere (a native Folder, another source's root).
 */
bool subtreeHoldsNativeContent(const cwSurveyNode* node, const QUuid& sourceId)
{
    bool holds = false;
    node->walk([&holds, &sourceId](const cwSurveyNode* visited) {
        if (visited->sourceId() != sourceId) {
            holds = true;
        }
        const QList<cwTrip*> trips = visited->trips();
        for (const cwTrip* trip : trips) {
            if (trip->sourcePath().isEmpty() || holdsNativeContent(trip)) {
                holds = true;
            }
        }
    });
    return holds;
}

QList<cwTrip*> buildTrips(const QList<TripRecord>& records, cwUnits::LengthUnit distanceUnit)
{
    QList<cwTrip*> trips;
    trips.reserve(records.size());
    for (const TripRecord& record : records) {
        auto* trip = new cwTrip();
        // Seed the survey-entry unit the way cwCave does for a UI-created
        // trip, so chunks the user later adds read in the project's unit.
        trip->calibrations()->setDistanceUnit(distanceUnit);
        trip->setName(record.name);
        trip->setSourcePath(record.sourcePath);
        if (record.date.isValid()) {
            trip->setDate(QDateTime(record.date, QTime()));
        }
        trips.append(trip);
    }
    return trips;
}

//! Builds \a record's whole subtree detached: each child is filled in before
//! it is inserted, the way cwSurveyNode::setData does.
cwSurveyNode* buildNode(const NodeRecord& record,
                        const QUuid& sourceId,
                        cwUnits::LengthUnit distanceUnit)
{
    auto* node = new cwCave();
    node->setName(record.name);
    node->setKind(record.kind);
    node->setReadOnly(true);
    node->setSourceId(sourceId);
    node->setSourcePath(record.sourcePath);

    QList<cwSurveyNode*> children;
    children.reserve(static_cast<qsizetype>(record.children.size()));
    for (const auto& child : record.children) {
        children.append(buildNode(*child, sourceId, distanceUnit));
    }
    node->addNodes(children);
    node->addTrips(buildTrips(record.trips, distanceUnit));
    return node;
}

} // namespace

struct cwSourceSubtreeFill::Plan {
    //! A node that is already in the tree and what lands under it.
    struct Parent {
        QPointer<cwSurveyNode> node;
        std::vector<std::unique_ptr<NodeRecord>> nodes;
        QList<TripRecord> trips;
    };

    //! One turn's install: \a count of a parent's new nodes or new trips.
    struct Batch {
        int parent = 0;
        bool installsNodes = false;
        int first = 0;
        int count = 0;
    };

    QUuid sourceId;
    std::vector<Parent> parents;
    QList<Batch> batches;
    QList<QPointer<cwTrip>> staleTrips;
    //! Deepest first, so a node that turns native keeps its ancestors too.
    QList<QPointer<cwSurveyNode>> staleNodes;
};

cwSourceSubtreeFill::cwSourceSubtreeFill(cwSurveyNode* root, QObject* parent) :
    QObject(parent),
    m_root(root),
    m_plan(std::make_unique<Plan>())
{
}

cwSourceSubtreeFill::~cwSourceSubtreeFill() = default;

void cwSourceSubtreeFill::start(const QList<cwScanRow>& rows,
                                const QString& jobName,
                                const cwFutureManagerToken& token)
{
    if (m_root.isNull()) {
        finish(false);
        return;
    }

    m_root->setUndoStack(nullptr);
    plan(rows);

    if (m_plan->batches.isEmpty()) {
        removeStale();
        finish(true);
        return;
    }

    m_job.start(jobName, static_cast<int>(m_plan->batches.size()), token);
    QMetaObject::invokeMethod(this, &cwSourceSubtreeFill::installNextBatch, Qt::QueuedConnection);
}

void cwSourceSubtreeFill::plan(const QList<cwScanRow>& rows)
{
    cwSurveyNode* root = m_root.data();
    const QUuid sourceId = root->sourceId();
    m_plan->sourceId = sourceId;

    // Every derived record under the root, by the path that names it in the
    // scan. A match is taken out, so what is left at the end is what the scan
    // no longer lists.
    QHash<QString, cwSurveyNode*> nodesByPath;
    QHash<QString, cwTrip*> tripsByPath;
    const QList<cwSurveyNode*> descendants = root->allNodes();
    for (cwSurveyNode* node : descendants) {
        if (node->sourceId() == sourceId && !node->sourcePath().isEmpty()) {
            nodesByPath.insert(node->sourcePath(), node);
        }
    }
    // A nested source root's derived trips belong to that root's reconcile.
    for (cwTrip* trip : root->allTrips()) {
        if (!trip->sourcePath().isEmpty() && trip->parentNode()->sourceRoot() == root) {
            tripsByPath.insert(trip->sourcePath(), trip);
        }
    }

    std::vector<Plan::Parent>& parents = m_plan->parents;
    QHash<const cwSurveyNode*, int> parentIndex;
    const auto parentPlan = [&parents, &parentIndex](cwSurveyNode* node) -> Plan::Parent& {
        const auto found = parentIndex.constFind(node);
        if (found != parentIndex.constEnd()) {
            return parents.at(found.value());
        }
        parentIndex.insert(node, static_cast<int>(parents.size()));
        parents.push_back(Plan::Parent { node, {}, {} });
        return parents.back();
    };

    // Where a row's children land: a node already in the tree, or a planned one.
    struct Slot {
        cwSurveyNode* live = nullptr;
        NodeRecord* record = nullptr;
    };

    // The pre-order walk keeps one slot and one inherited date per depth; a
    // skipped node row skips every row below it.
    QList<Slot> slotAtDepth;
    QList<QDate> dateAtDepth;
    int skipBelowDepth = -1;

    for (const cwScanRow& row : rows) {
        if (skipBelowDepth >= 0 && row.depth > skipBelowDepth) {
            continue;
        }
        skipBelowDepth = -1;

        const bool isRootRow = row.depth == 0;
        if (!isRootRow && row.missing) {
            if (row.isNode) {
                skipBelowDepth = row.depth;
            }
            continue;
        }

        slotAtDepth.resize(row.depth + 1);
        dateAtDepth.resize(row.depth + 1);
        const Slot parent = isRootRow ? Slot() : slotAtDepth.at(row.depth - 1);
        const QDate inheritedDate = isRootRow ? QDate() : dateAtDepth.at(row.depth - 1);
        dateAtDepth[row.depth] = row.date.isValid() ? row.date : inheritedDate;

        Slot unitParent = parent;
        if (isRootRow) {
            unitParent = Slot { root, nullptr };
            slotAtDepth[0] = unitParent;
        } else if (row.isNode) {
            Slot slot;
            slot.live = nodesByPath.take(row.sourcePath);
            if (slot.live == nullptr) {
                // The name is only sanitized here: addNodes() dedupes it among
                // its siblings, in row order, when the batch lands.
                auto record = std::make_unique<NodeRecord>();
                record->name = cwNameUtils::sanitizeFileName(row.displayName);
                record->kind = row.kind;
                record->sourcePath = row.sourcePath;
                slot.record = record.get();
                if (parent.record != nullptr) {
                    parent.record->children.push_back(std::move(record));
                } else {
                    parentPlan(parent.live).nodes.push_back(std::move(record));
                }
            }
            slotAtDepth[row.depth] = slot;
            unitParent = slot;
        }

        if (!row.isUnit) {
            continue;
        }
        const QString tripPath = cwExternalCenterlineAttach::unitSourcePath(row);
        if (tripsByPath.take(tripPath) != nullptr) {
            continue;
        }
        // Seeding is create-only: a trip that already exists was matched
        // above, so a reload never writes over a date the user set.
        const TripRecord trip {
            cwNameUtils::sanitizeFileName(row.displayName), tripPath, dateAtDepth.at(row.depth)
        };
        if (unitParent.record != nullptr) {
            unitParent.record->trips.append(trip);
        } else {
            parentPlan(unitParent.live).trips.append(trip);
        }
    }

    for (cwTrip* trip : std::as_const(tripsByPath)) {
        m_plan->staleTrips.append(trip);
    }
    // allNodes() lists a parent before its children, so the reverse walk
    // visits the deepest first.
    for (auto it = descendants.crbegin(); it != descendants.crend(); ++it) {
        if (nodesByPath.value((*it)->sourcePath()) == *it) {
            m_plan->staleNodes.append(*it);
        }
    }

    for (int parentRow = 0; parentRow < static_cast<int>(parents.size()); ++parentRow) {
        const Plan::Parent& parent = parents.at(parentRow);

        const int nodeCount = static_cast<int>(parent.nodes.size());
        int first = 0;
        int rowsInBatch = 0;
        for (int i = 0; i < nodeCount; ++i) {
            const int subtreeRows = parent.nodes.at(i)->rowCount();
            if (i > first && rowsInBatch + subtreeRows > kInstallRowsPerTurn) {
                m_plan->batches.append(Plan::Batch { parentRow, true, first, i - first });
                first = i;
                rowsInBatch = 0;
            }
            rowsInBatch += subtreeRows;
        }
        if (first < nodeCount) {
            m_plan->batches.append(Plan::Batch { parentRow, true, first, nodeCount - first });
        }

        const int tripCount = static_cast<int>(parent.trips.size());
        for (int tripFirst = 0; tripFirst < tripCount; tripFirst += kInstallRowsPerTurn) {
            m_plan->batches.append(Plan::Batch {
                parentRow, false, tripFirst, std::min(kInstallRowsPerTurn, tripCount - tripFirst)
            });
        }
    }
}

void cwSourceSubtreeFill::installNextBatch()
{
    if (!rootInRegion()) {
        finish(false);
        return;
    }

    const Plan::Batch& batch = m_plan->batches.at(m_nextBatch);
    const Plan::Parent& parent = m_plan->parents.at(batch.parent);
    cwSurveyNode* parentNode = parent.node.data();
    if (parentNode == nullptr || !parentNode->isSelfOrDescendantOf(m_root.data())) {
        finish(false);
        return;
    }

    const cwUnits::LengthUnit distanceUnit = cwUnits::surveyUnit(m_root->unitSystem());
    if (batch.installsNodes) {
        QList<cwSurveyNode*> nodes;
        nodes.reserve(batch.count);
        for (int i = batch.first; i < batch.first + batch.count; ++i) {
            nodes.append(buildNode(*parent.nodes.at(i), m_plan->sourceId, distanceUnit));
        }
        parentNode->addNodes(nodes);
    } else {
        parentNode->addTrips(buildTrips(parent.trips.mid(batch.first, batch.count), distanceUnit));
    }

    ++m_nextBatch;
    m_job.setProgressValue(m_nextBatch);

    if (m_nextBatch < m_plan->batches.size()) {
        QMetaObject::invokeMethod(this, &cwSourceSubtreeFill::installNextBatch,
                                  Qt::QueuedConnection);
        return;
    }

    removeStale();
    finish(true);
}

bool cwSourceSubtreeFill::rootInRegion() const
{
    // A removed root keeps its parent pointer for undo, so membership, not
    // liveness, says whether it still hangs in its region.
    const cwCavingRegion* region = m_root.isNull() ? nullptr : m_root->parentRegion();
    return region != nullptr && m_root->isSelfOrDescendantOf(region->rootNode());
}

void cwSourceSubtreeFill::removeStale()
{
    // removeTrip() and removeNode() are the G1 boundary: they emit
    // tripsDeleted()/nodesDeleted(), and with the undo stack cleared they
    // destroy the object, so the pointers are guarded.
    for (const QPointer<cwTrip>& trip : std::as_const(m_plan->staleTrips)) {
        if (trip.isNull() || holdsNativeContent(trip)) {
            continue;
        }
        cwSurveyNode* parent = trip->parentNode();
        const int row = parent != nullptr ? parent->indexOf(trip) : -1;
        if (row >= 0) {
            parent->removeTrip(row);
        }
    }

    for (const QPointer<cwSurveyNode>& node : std::as_const(m_plan->staleNodes)) {
        if (node.isNull()) {
            continue;
        }
        if (subtreeHoldsNativeContent(node, m_plan->sourceId)) {
            node->setSourceId(QUuid());
            node->setSourcePath(QString());
            node->setReadOnly(false);
            continue;
        }
        cwSurveyNode* parent = node->parentNode();
        const int row = parent != nullptr ? parent->indexOfNode(node) : -1;
        if (row >= 0) {
            parent->removeNode(row);
        }
    }
}

void cwSourceSubtreeFill::finish(bool installed)
{
    if (!m_root.isNull()) {
        if (cwSurveyNode* parent = m_root->parentNode()) {
            m_root->setUndoStack(parent->undoStack());
        }
    }
    m_job.finish();
    emit finished(installed);
    deleteLater();
}
