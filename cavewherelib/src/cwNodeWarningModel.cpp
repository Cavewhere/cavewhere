/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwNodeWarningModel.h"
#include "cwAttachedCenterlinesModel.h"
#include "cwAttachedFixModel.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwFixStationModel.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"

//Qt includes
#include <QHash>
#include <QSet>

//Std includes
#include <algorithm>
#include <array>

namespace {

//! Roll-up kinds that are not a warning type: a survey file that failed to read,
//! and any shown warning with no type of its own.
constexpr int kFileErrorKind = -1;
constexpr int kOtherWarningKind = -2;

//! The order the roll-up rows list in: a file that failed to read first, then
//! what keeps the survey from its place on the map, then the rest.
constexpr std::array kRollUpKindOrder = {
    kFileErrorKind,
    static_cast<int>(cwErrorTypeId::AttachedFileUnsolved),
    static_cast<int>(cwErrorTypeId::AttachedFileFixWithoutCS),
    static_cast<int>(cwErrorTypeId::AttachedFileUnfixed),
    static_cast<int>(cwErrorTypeId::AttachedFileDefaultDatum),
    static_cast<int>(cwErrorTypeId::AttachedFileReadWarnings),
    static_cast<int>(cwErrorTypeId::UnconnectedStations),
    static_cast<int>(cwErrorTypeId::FixStationOutlier),
    static_cast<int>(cwErrorTypeId::FixStationDomain),
    static_cast<int>(cwErrorTypeId::FixStationReference),
    kOtherWarningKind
};

//! A roll-up row's detail names this many nodes, then counts the rest.
constexpr int kMaxNamedNodes = 5;

bool isShownWarning(const cwError& error)
{
    return error.type() == cwError::Warning && !error.suppressed();
}

//! The warnings a node's page lists from its trips, which own them.
constexpr std::array kTripWarningKinds = {
    static_cast<int>(cwErrorTypeId::AttachedFileUnsolved),
    static_cast<int>(cwErrorTypeId::UnconnectedStations)
};

bool isTripWarning(const cwError& error)
{
    return isShownWarning(error)
           && std::find(kTripWarningKinds.cbegin(), kTripWarningKinds.cend(), error.errorTypeId())
                  != kTripWarningKinds.cend();
}

/**
 * The Fix Stations page's row of the first fix read in no coordinate system under
 * \a node, -1 when there is none. The page lists a node's fixStationTable()
 * rows, then its derived trips' attached fixes, then each child node's subtree
 * in order, so a row's place counts every row listed ahead of it; \a rowsAhead
 * carries that count down the walk.
 */
int firstBareFixPageRow(const cwSurveyNode* node, int& rowsAhead)
{
    const cwAttachedFixModel* attachedFixes = node->attachedFixes();
    const int bareRow = attachedFixes->firstFixWithoutReadInSystem();
    if (bareRow >= 0) {
        return rowsAhead
               + node->fixStationTable()->mapFromSource(attachedFixes->index(bareRow)).row();
    }
    rowsAhead += node->fixStationCount();
    for (const cwTrip* trip : node->trips()) {
        const cwAttachedFixModel* tripFixes = trip->attachedFixes();
        const int tripBareRow = tripFixes->firstFixWithoutReadInSystem();
        if (tripBareRow >= 0) {
            return rowsAhead + tripBareRow;
        }
        rowsAhead += tripFixes->count();
    }
    for (const cwSurveyNode* child : node->childNodes()) {
        const int row = firstBareFixPageRow(child, rowsAhead);
        if (row >= 0) {
            return row;
        }
    }
    return -1;
}

int rollUpKind(const cwError& error)
{
    const int typeId = error.errorTypeId();
    const bool isTyped = std::find(kRollUpKindOrder.cbegin(), kRollUpKindOrder.cend(), typeId)
                         != kRollUpKindOrder.cend();
    return isTyped ? typeId : kOtherWarningKind;
}

QString countedPhrase(int nodeCount, const QString& singular, const QString& plural)
{
    if (nodeCount == 1) {
        return QStringLiteral("1 node below %1").arg(singular);
    }
    return QStringLiteral("%1 nodes below %2").arg(QString::number(nodeCount), plural);
}

QString rollUpMessage(int kind, int nodeCount)
{
    switch (kind) {
    case kFileErrorKind:
        return countedPhrase(nodeCount,
                             QStringLiteral("has a survey file that failed to read."),
                             QStringLiteral("have survey files that failed to read."));
    case static_cast<int>(cwErrorTypeId::AttachedFileUnsolved):
        return countedPhrase(nodeCount,
                             QStringLiteral("has a trip whose survey file is not solved."),
                             QStringLiteral("have trips whose survey files are not solved."));
    case static_cast<int>(cwErrorTypeId::AttachedFileFixWithoutCS):
        return countedPhrase(nodeCount,
                             QStringLiteral("has fixes with no coordinate system."),
                             QStringLiteral("have fixes with no coordinate system."));
    case static_cast<int>(cwErrorTypeId::AttachedFileUnfixed):
        return countedPhrase(nodeCount,
                             QStringLiteral("has a survey file with no fixed station."),
                             QStringLiteral("have survey files with no fixed station."));
    case static_cast<int>(cwErrorTypeId::AttachedFileDefaultDatum):
        return countedPhrase(nodeCount,
                             QStringLiteral("reads a Compass file in the default datum."),
                             QStringLiteral("read Compass files in the default datum."));
    case static_cast<int>(cwErrorTypeId::AttachedFileReadWarnings):
        return countedPhrase(nodeCount,
                             QStringLiteral("has a survey file read with warnings."),
                             QStringLiteral("have survey files read with warnings."));
    case static_cast<int>(cwErrorTypeId::UnconnectedStations):
        return countedPhrase(nodeCount,
                             QStringLiteral("has stations that are not tied to the cave."),
                             QStringLiteral("have stations that are not tied to the cave."));
    case static_cast<int>(cwErrorTypeId::FixStationOutlier):
        return countedPhrase(nodeCount,
                             QStringLiteral("has a fix far from the survey."),
                             QStringLiteral("have fixes far from the survey."));
    case static_cast<int>(cwErrorTypeId::FixStationDomain):
        return countedPhrase(nodeCount,
                             QStringLiteral("has a fix outside its coordinate system's area."),
                             QStringLiteral("have fixes outside their coordinate system's area."));
    case static_cast<int>(cwErrorTypeId::FixStationReference):
        return countedPhrase(nodeCount,
                             QStringLiteral("has a fix on a station the survey lacks."),
                             QStringLiteral("have fixes on stations the survey lacks."));
    default:
        return countedPhrase(nodeCount,
                             QStringLiteral("has other warnings."),
                             QStringLiteral("have other warnings."));
    }
}

QString rollUpDetail(const QList<cwSurveyNode*>& nodes)
{
    QStringList names;
    const int namedCount = std::min(static_cast<int>(nodes.size()), kMaxNamedNodes);
    names.reserve(namedCount);
    for (int i = 0; i < namedCount; ++i) {
        names.append(nodes.at(i)->name());
    }
    const QString named = names.join(QStringLiteral(", "));
    const int unnamedCount = static_cast<int>(nodes.size()) - namedCount;
    if (unnamedCount > 0) {
        return QStringLiteral("%1, and %2 more").arg(named, QString::number(unnamedCount));
    }
    return named;
}

} // namespace

cwNodeWarningModel::cwNodeWarningModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void cwNodeWarningModel::setNode(cwSurveyNode* node)
{
    if (m_node == node) {
        return;
    }
    disconnectAll(m_nodeConnections);
    m_node = node;
    connectNode();
    connectTrips();
    emit nodeChanged();
    rebuildWithRollUps();
}

void cwNodeWarningModel::setAttachedCenterlines(cwAttachedCenterlinesModel* attachedCenterlines)
{
    if (m_attachedCenterlines == attachedCenterlines) {
        return;
    }
    disconnectAll(m_attachedCenterlinesConnections);
    m_attachedCenterlines = attachedCenterlines;
    if (m_attachedCenterlines != nullptr) {
        const auto schedule = [this]() { scheduleRebuild(); };
        m_attachedCenterlinesConnections = {
            connect(m_attachedCenterlines, &cwAttachedCenterlinesModel::modelReset, this, schedule),
            connect(m_attachedCenterlines, &cwAttachedCenterlinesModel::rowsInserted, this, schedule),
            connect(m_attachedCenterlines, &cwAttachedCenterlinesModel::rowsRemoved, this, schedule),
            connect(m_attachedCenterlines, &cwAttachedCenterlinesModel::dataChanged, this, schedule),
            connect(m_attachedCenterlines, &QObject::destroyed, this, schedule)
        };
    }
    emit attachedCenterlinesChanged();
    rebuildWithRollUps();
}

int cwNodeWarningModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant cwNodeWarningModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const Entry& entry = m_entries.at(index.row());
    switch (role) {
    case MessageRole:
        return entry.message;
    case DetailRole:
        return entry.detail;
    case TargetRole:
        return static_cast<int>(entry.target);
    case TripRole:
        return QVariant::fromValue(entry.trip.data());
    case FixStationRowRole:
        return entry.fixStationRow;
    case NodeRole:
        return QVariant::fromValue(entry.node.data());
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwNodeWarningModel::roleNames() const
{
    return {
        { MessageRole, "message" },
        { DetailRole, "detail" },
        { TargetRole, "target" },
        { TripRole, "trip" },
        { FixStationRowRole, "fixStationRow" },
        { NodeRole, "node" }
    };
}

void cwNodeWarningModel::connectNode()
{
    if (m_node == nullptr) {
        return;
    }

    const auto reconnectTripsAndRebuild = [this]() {
        connectTrips();
        rebuild();
    };

    const cwErrorListModel* errors = m_node->errorModel()->errors();
    const cwFixStationModel* fixStations = m_node->fixStations();
    m_nodeConnections = {
        connect(errors, &cwErrorListModel::warningMessagesChanged, this, &cwNodeWarningModel::rebuild),
        connect(errors, &cwErrorListModel::dataChanged, this, &cwNodeWarningModel::rebuild),

        // The node's rows are its direct trips.
        connect(m_node, &cwSurveyNode::rowsInserted, this, reconnectTripsAndRebuild),
        connect(m_node, &cwSurveyNode::rowsRemoved, this, reconnectTripsAndRebuild),
        connect(m_node, &cwSurveyNode::modelReset, this, reconnectTripsAndRebuild),

        // A fix inserted or removed above the warning's fix moves its row.
        connect(fixStations, &cwFixStationModel::rowsInserted, this, &cwNodeWarningModel::rebuild),
        connect(fixStations, &cwFixStationModel::rowsRemoved, this, &cwNodeWarningModel::rebuild),
        connect(fixStations, &cwFixStationModel::rowsMoved, this, &cwNodeWarningModel::rebuild),
        connect(fixStations, &cwFixStationModel::modelReset, this, &cwNodeWarningModel::rebuild),
        // So does the attached fix the bare-fix warning points at.
        connect(m_node->attachedFixes(), &cwAttachedFixModel::modelReset,
                this, &cwNodeWarningModel::rebuild),
        connect(m_node->attachedFixes(), &cwAttachedFixModel::dataChanged,
                this, &cwNodeWarningModel::rebuild),

        // The error model counts the whole subtree's warnings, so this fires on
        // any warning below the node; the structure and names can move without one.
        connect(m_node->errorModel(), &cwErrorModel::warningCountChanged,
                this, &cwNodeWarningModel::scheduleRebuild),
        connect(m_node, &cwSurveyNode::subtreeChanged, this, &cwNodeWarningModel::scheduleRebuild),
        connect(m_node, &cwSurveyNode::scopeLabelsChanged, this, &cwNodeWarningModel::scheduleRebuild),

        // m_node already reads null here, so setNode(nullptr) would return early.
        connect(m_node, &QObject::destroyed, this, [this]() {
            disconnectAll(m_nodeConnections);
            disconnectAll(m_tripConnections);
            emit nodeChanged();
            rebuildWithRollUps();
        })
    };
}

void cwNodeWarningModel::connectTrips()
{
    disconnectAll(m_tripConnections);
    if (m_node == nullptr) {
        return;
    }

    for (const cwTrip* trip : m_node->trips()) {
        const cwErrorListModel* errors = trip->errorModel()->errors();
        m_tripConnections.append(connect(errors, &cwErrorListModel::warningMessagesChanged,
                                         this, &cwNodeWarningModel::rebuild));
        m_tripConnections.append(connect(errors, &cwErrorListModel::dataChanged,
                                         this, &cwNodeWarningModel::rebuild));
        // A derived trip's attached fixes sit on the page after the node's.
        m_tripConnections.append(connect(trip->attachedFixes(), &cwAttachedFixModel::modelReset,
                                         this, &cwNodeWarningModel::rebuild));
        m_tripConnections.append(connect(trip->attachedFixes(), &cwAttachedFixModel::dataChanged,
                                         this, &cwNodeWarningModel::rebuild));
    }
}

void cwNodeWarningModel::disconnectAll(QList<QMetaObject::Connection>& connections)
{
    for (const QMetaObject::Connection& connection : std::as_const(connections)) {
        disconnect(connection);
    }
    connections.clear();
}

void cwNodeWarningModel::scheduleRebuild()
{
    if (m_rebuildScheduled) {
        return;
    }
    m_rebuildScheduled = true;
    QMetaObject::invokeMethod(this, [this]() {
        if (m_rebuildScheduled) {
            rebuildWithRollUps();
        }
    }, Qt::QueuedConnection);
}

void cwNodeWarningModel::rebuildWithRollUps()
{
    m_rebuildScheduled = false;
    if (m_node != nullptr) {
        m_rollUpEntries = descendantEntries();
    } else {
        disconnectAll(m_descendantTripConnections);
        m_rollUpEntries.clear();
    }
    rebuild();
}

void cwNodeWarningModel::rebuild()
{
    QList<Entry> entries;
    if (m_node != nullptr) {
        const QList<cwError> nodeErrors = m_node->errorModel()->errors()->toList();
        for (const cwError& error : nodeErrors) {
            if (isShownWarning(error)) {
                entries.append(nodeEntry(error));
            }
        }

        for (cwTrip* trip : m_node->trips()) {
            const QList<cwError> tripErrors = trip->errorModel()->errors()->toList();
            for (const cwError& error : tripErrors) {
                if (isTripWarning(error)) {
                    entries.append(Entry{error.message(), error.detail(), Target::TripPage, trip});
                }
            }
        }

        entries.append(m_rollUpEntries);
    }

    if (entries == m_entries) {
        return;
    }

    const int oldCount = count();
    beginResetModel();
    m_entries = entries;
    endResetModel();
    if (count() != oldCount) {
        emit countChanged();
    }
}

cwNodeWarningModel::Entry cwNodeWarningModel::nodeEntry(const cwError& error) const
{
    Entry entry{error.message(), error.detail()};

    // The warning names the attached files' bare fixes in file order, which
    // lists them on the nodes whose files hold them, so the entry picks the
    // first one's row on this node's Fix Stations page.
    if (error.errorTypeId() == static_cast<int>(cwErrorTypeId::AttachedFileFixWithoutCS)) {
        entry.target = Target::FixStationRow;
        int rowsAhead = 0;
        entry.fixStationRow = firstBareFixPageRow(m_node, rowsAhead);
        return entry;
    }

    // The page itself: the user places the file by fixing any of its stations,
    // and the rows show the system a defaulted datum gave the file's fixes.
    if (error.errorTypeId() == static_cast<int>(cwErrorTypeId::AttachedFileUnfixed)
        || error.errorTypeId() == static_cast<int>(cwErrorTypeId::AttachedFileDefaultDatum)) {
        entry.target = Target::FixStationRow;
        return entry;
    }

    const QUuid targetId = error.targetId();
    if (targetId.isNull()) {
        return entry;
    }

    if (targetId == m_node->id()) {
        entry.target = Target::SourceLine;
        return entry;
    }

    const int fixStationRow = m_node->fixStations()->indexOf(targetId);
    if (fixStationRow >= 0) {
        entry.target = Target::FixStationRow;
        entry.fixStationRow = fixStationRow;
        return entry;
    }

    for (cwTrip* trip : m_node->trips()) {
        if (trip->id() == targetId) {
            entry.target = Target::TripPage;
            entry.trip = trip;
            return entry;
        }
    }
    return entry;
}

QList<cwNodeWarningModel::Entry> cwNodeWarningModel::descendantEntries()
{
    disconnectAll(m_descendantTripConnections);

    QHash<int, QList<cwSurveyNode*>> ownersByKind;
    const QList<cwSurveyNode*> descendants = m_node->allNodes();
    for (cwSurveyNode* descendant : descendants) {
        QSet<int> kinds;
        if (hasFileError(descendant)) {
            kinds.insert(kFileErrorKind);
        }

        const QList<cwError> errors = descendant->errorModel()->errors()->toList();
        for (const cwError& error : errors) {
            if (isShownWarning(error)) {
                kinds.insert(rollUpKind(error));
            }
        }

        for (const cwTrip* trip : descendant->trips()) {
            m_descendantTripConnections.append(
                connect(trip, &cwTrip::externalStationsErrorChanged,
                        this, &cwNodeWarningModel::scheduleRebuild));
            const QList<cwError> tripErrors = trip->errorModel()->errors()->toList();
            for (const cwError& error : tripErrors) {
                if (isTripWarning(error)) {
                    kinds.insert(error.errorTypeId());
                }
            }
        }

        for (int kind : std::as_const(kinds)) {
            ownersByKind[kind].append(descendant);
        }
    }

    QList<Entry> entries;
    for (int kind : kRollUpKindOrder) {
        const QList<cwSurveyNode*> owners = ownersByKind.value(kind);
        if (owners.isEmpty()) {
            continue;
        }
        Entry entry{rollUpMessage(kind, static_cast<int>(owners.size())),
                    rollUpDetail(owners),
                    Target::NodePage};
        entry.node = owners.first();
        entries.append(entry);
    }
    return entries;
}

bool cwNodeWarningModel::hasFileError(const cwSurveyNode* node) const
{
    const auto attachedError = [this](const QUuid& ownerId) {
        return m_attachedCenterlines != nullptr
               && !m_attachedCenterlines->errorFor(ownerId).isEmpty();
    };

    if (attachedError(node->id())) {
        return true;
    }
    for (const cwTrip* trip : node->trips()) {
        if (attachedError(trip->id()) || !trip->externalStationsError().isEmpty()) {
            return true;
        }
    }
    return false;
}
