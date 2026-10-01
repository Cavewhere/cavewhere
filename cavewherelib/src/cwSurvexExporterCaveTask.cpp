/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurvexExporterUtils.h"
#include "cwTrip.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStringList>

//Std includes
#include <algorithm>

namespace {

// All driver-emitted *include paths are absolute with forward slashes
// (Survex accepts forward slashes on every platform) and double-quoted
// so sanitised cave/trip dir names with spaces survive. See
// plans/EXTERNAL_FILE_PHASE1.html §10.
QString driverIncludePathFor(const QString& attachmentDir, const QString& entryFile)
{
    const QString joined = QDir(attachmentDir).filePath(entryFile);
    return QFileInfo(joined).absoluteFilePath();
}

bool chunksHoldStations(const cwTripData& trip)
{
    return std::any_of(trip.chunks.cbegin(), trip.chunks.cend(),
                       [](const cwSurveyChunkData& chunk) { return !chunk.stations.isEmpty(); });
}

} // namespace

cwSurvexExporterCaveTask::DriverTree::DriverTree(const QList<cwCaveData>& nodes,
                                                 const cwScopeLabels& labels,
                                                 const QSet<QUuid>& excludedExternalOwners) :
    m_nodes(nodes),
    m_labels(labels),
    m_excludedExternalOwners(excludedExternalOwners)
{
    for (const cwCaveData& node : m_nodes) {
        index(node, false);
    }
}

// Returns whether \a node emits. A container under an excluded owner has no
// scope, and neither does one below a sourced root's own level: the root is
// one *include, and nothing beneath it is a block.
bool cwSurvexExporterCaveTask::DriverTree::index(const cwCaveData& node, bool scopeClosedAbove)
{
    const bool scopeClosed = scopeClosedAbove || m_excludedExternalOwners.contains(node.id);
    const bool sourced = !node.externalCenterline.isEmpty();

    m_nodeOwners.insert(node.id, {&node, nullptr, !scopeClosed});

    bool ownTripsHoldData = false;
    for (const cwTripData& trip : node.trips) {
        m_tripOwners.insert(trip.id, {&node, &trip, !scopeClosed});
        const bool tripHoldsData = trip.externalCenterline.isEmpty()
                                       ? chunksHoldStations(trip)
                                       : !m_excludedExternalOwners.contains(trip.id);
        ownTripsHoldData = ownTripsHoldData || tripHoldsData;
    }

    bool childEmits = false;
    for (const cwCaveData& child : node.nodes) {
        childEmits = index(child, scopeClosed || sourced) || childEmits;
    }

    // A sourced root is its *include alone (§7.1): its trips are windows on the
    // included file and its children are never blocks.
    const bool emits = sourced ? !m_excludedExternalOwners.contains(node.id)
                               : ownTripsHoldData || childEmits;
    if (emits) {
        m_emittingNodeIds.insert(node.id);
    }
    return emits;
}

QString cwSurvexExporterCaveTask::DriverTree::operand(const cwStationHandle& handle) const
{
    // A tie to an owner whose attachment was excluded names a scope the
    // driver never opened. Cavern does not reject an unknown operand — it
    // *creates* the station and a zero-length leg (netbits.c
    // process_equate) — so emitting the tie would fabricate a station under
    // the excluded owner's label, at the other operand's coordinate, and the
    // decode would route it back to the very survey that was left out.
    // containerId is the trip id for a Trip handle and the node id for a
    // NativeCave one, and the excluded set holds both kinds.
    if (m_excludedExternalOwners.contains(handle.containerId())) {
        return QString();
    }

    const QHash<QUuid, Owner>& owners =
        handle.scope() == cwStationHandle::Trip ? m_tripOwners : m_nodeOwners;
    const auto ownerIt = owners.constFind(handle.containerId());
    if (ownerIt == owners.constEnd() || !ownerIt->hasScope || !emits(ownerIt->node->id)) {
        return QString();
    }

    const Owner& owner = ownerIt.value();
    const QString nodePrefix = m_labels.prefix(owner.node->id);
    if (nodePrefix.isEmpty()) {
        return QString();
    }

    if (owner.trip == nullptr) {
        return nodePrefix + handle.tail();
    }
    return nodePrefix
           + cwTrip::scopePrefix(*owner.trip, m_labels.tripLabels(owner.node->id))
           + handle.tail();
}

cwSurvexExporterCaveTask::cwSurvexExporterCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
    TripExporter = new cwSurvexExporterTripTask(this);
    TripExporter->setParentSurvexExporter(this);
}

void cwSurvexExporterCaveTask::setExportOptions(const cwSurvexExporterRegion::Options& options)
{
    ExportOptions = options;
}

bool cwSurvexExporterCaveTask::writeCave(QTextStream& stream, const cwCaveData& cave, const QString& globalCS)
{
    // A single-cave export has no region and no siblings, so the cave stands
    // alone under its own sanitized name.
    const QList<cwCaveData> nodes = {cave};
    const DriverTree tree(nodes, cwScopeLabels::forNode(cave), ExportOptions.excludedExternalOwners);

    if (!tree.emits(cave.id)) {
        // An excluded cave-level attachment is skipped, not failed: the
        // attachment is what is missing, and the reason is already on its row.
        const bool excludedAttachment = !cave.externalCenterline.isEmpty();
        if (excludedAttachment) {
            return true;
        }
        Errors.append(QStringLiteral("No survey data to export in %1").arg(cave.name));
        if (isRunning()) {
            stop();
        }
        return false;
    }

    if (!writeNode(stream, nodes.first(), tree, globalCS)) {
        return false;
    }

    // Ties emit after the cave's block closes, fully qualified, as the region
    // driver does. A tie reaching outside this cave renders no operand there,
    // so writeEquateLine drops it.
    writeEquates(stream, m_equates, tree);
    return true;
}

void cwSurvexExporterCaveTask::setEquates(const QList<cwEquate>& equates)
{
    m_equates = equates;
}

bool cwSurvexExporterCaveTask::writeNode(QTextStream& stream,
                                         const cwCaveData& node,
                                         const DriverTree& tree,
                                         const QString& globalCS)
{
    TotalProgress = 0;
    return writeNodeBlock(stream, node, tree, globalCS, Inherited());
}

bool cwSurvexExporterCaveTask::writeNodeBlock(QTextStream& stream,
                                              const cwCaveData& node,
                                              const DriverTree& tree,
                                              const QString& globalCS,
                                              const Inherited& inherited)
{
    // An empty block is worse than none: cavern fatals with "No survey data"
    // on a driver that declares only empty scopes, so a project whose one
    // cave is an excluded attachment would lose the whole solve rather than
    // the one survey.
    if (!tree.emits(node.id)) {
        return true;
    }

    const cwScopeLabels& labels = tree.labels();
    const QString label = labels.label(node.id);
    stream << "*begin " << label << " ;" << node.name << Qt::endl << Qt::endl;
    const auto writeEnd = [&stream, &label, &node]() {
        stream << "*end " << label << " ; End of " << node.name << Qt::endl << Qt::endl;
    };

    // Sourced root: skip fix stations, calibrations, trips and child nodes.
    // The included file carries its own *cs, *fix, and *begin/*end structure;
    // emitting our own would either silently shadow theirs (no-op) or fight
    // them (cavern error). Per master plan §6, native + external are not
    // mixed inside the same node body.
    if (!node.externalCenterline.isEmpty()) {
        if (!writeExternalInclude(stream, node.id,
                                  ExportOptions.caveAttachmentDirs,
                                  node.externalCenterline.entryFile(),
                                  node.name)) {
            return false;
        }
        writeEnd();
        return true;
    }

    Inherited here;
    here.anchored = writeFixStations(stream, node, globalCS, inherited.anchored)
                    || inherited.anchored;
    here.declination = cwSurvexExporterUtils::makeDeclinationContext(node.fixStations, globalCS);
    if (!here.declination.has_value()) {
        here.declination = inherited.declination;
    }

    const QHash<QUuid, QString>& tripLabels = labels.tripLabels(node.id);

    for (const cwTripData& tripData : node.trips) {
        // Trip-level external attachment: wrap the *include in
        // *begin <tripLabel> / *end <tripLabel> so the included file's
        // own *begin / *end stay isolated from this node's namespace
        // (master plan §6, "Native vs. external trip wrapping is
        // asymmetric — on purpose"). Stations from the included file
        // resolve to <nodePrefix><tripLabel>.<file-tail>.
        if (!tripData.externalCenterline.isEmpty()) {
            // Excluded attachments emit nothing at all. The trip
            // contributes no stations either way.
            if (tree.excludedExternalOwners().contains(tripData.id)) {
                continue;
            }

            const QString tripLabel = tripLabels.value(tripData.id);
            stream << "*begin " << tripLabel << " ; " << tripData.name << Qt::endl;
            // CaveWhere-resolved declination for files that carry none of
            // their own. Emitted before *include so it scopes over the
            // included legs; when the file owns declination the map has no
            // entry (see cwSurvexExporterRegion::Options).
            const auto declinationIt =
                ExportOptions.tripInjectedDeclinations.constFind(tripData.id);
            if (declinationIt != ExportOptions.tripInjectedDeclinations.constEnd()) {
                cwSurvexExporterUtils::writeCalibration(stream,
                                                        QStringLiteral("DECLINATION"),
                                                        declinationIt.value());
            }
            if (!writeExternalInclude(stream, tripData.id,
                                      ExportOptions.tripAttachmentDirs,
                                      tripData.externalCenterline.entryFile(),
                                      tripData.name)) {
                return false;
            }
            stream << "*end " << tripLabel << Qt::endl << Qt::endl;
            continue;
        }

        auto trip = std::make_unique<cwTrip>();
        trip->setData(tripData);
        TripExporter->writeTrip(stream, trip.get(), here.declination);
        TotalProgress += trip->numberOfStations();
        stream << Qt::endl;
    }

    for (const cwCaveData& child : node.nodes) {
        if (!writeNodeBlock(stream, child, tree, globalCS, here)) {
            return false;
        }
    }

    writeEnd();
    return true;
}

/**
 * Emit a bare `*equate` line per valid tie in `equates`, rendering each handle
 * fully qualified with DriverTree::operand(). No `*export`/`*infer` is needed —
 * a cross-scope `*equate` inside the enclosing block resolves both sibling
 * scopes on its own.
 * Structurally-invalid equates, and any whose handle the renderer cannot
 * resolve, are dropped silently (writeEquateLine); station *existence* is not
 * checked here (deferred to the equate UX, master plan §1.2), matching the
 * fix-station path which also trusts the snapshot.
 */
void cwSurvexExporterCaveTask::writeEquates(
    QTextStream& stream,
    const QList<cwEquate>& equates,
    const DriverTree& tree)
{
    for (const cwEquate& equate : equates) {
        if (!equate.isValid()) {
            continue;
        }

        const QList<cwStationHandle> handles = equate.stations();
        QStringList operands;
        operands.reserve(handles.size());
        for (const cwStationHandle& handle : handles) {
            operands.append(tree.operand(handle));
        }

        writeEquateLine(stream, operands);
    }
}

/**
 * Emit one `*equate` line from operands already rendered for the tie's scope.
 * An empty operand marks a handle that couldn't be resolved (out-of-cave, or
 * an unknown trip), so the whole tie is dropped rather than emit a name cavern
 * can't bind. Two distinct handles can still render to the same string (two
 * trips sharing a stationPrefix), so the line is de-duplicated and skipped
 * unless at least two distinct operands survive — cavern rejects `*equate X X`.
 */
void cwSurvexExporterCaveTask::writeEquateLine(QTextStream& stream, const QStringList& operands)
{
    QStringList distinct;
    distinct.reserve(operands.size());
    for (const QString& operand : operands) {
        if (operand.isEmpty()) {
            return;
        }
        if (!distinct.contains(operand)) {
            distinct.append(operand);
        }
    }

    if (distinct.size() < 2) {
        return;
    }

    stream << "*equate " << distinct.join(QLatin1Char(' ')) << Qt::endl;
}

/**
 * Emit the *cs / *fix block for the node. Validates the snapshot's
 * fixStations against the node's own station names; rejected fixes are
 * dropped silently here (the user-facing exporter — cwSurvexExporterRule —
 * runs the same validation at snapshot time and surfaces errors on the cave).
 * Falls back to `*fix <firstStation> 0 0 0` when no valid fix exists and no
 * enclosing block is anchored, so each un-fixed top-level survey still
 * resolves in cavern.
 */
bool cwSurvexExporterCaveTask::writeFixStations(QTextStream &stream, const cwCaveData &node,
                                                const QString& globalCS, bool anchoredAbove)
{
    QSet<QString> stationNamesLower;
    QString firstValidStation;
    for (const cwTripData& trip : node.trips) {
        for (const cwSurveyChunkData& chunk : trip.chunks) {
            for (const cwStation& station : chunk.stations) {
                if (station.isValid()) {
                    stationNamesLower.insert(cwStation::canonicalKey(station.name()));
                    if (firstValidStation.isEmpty()) {
                        firstValidStation = station.name();
                    }
                }
            }
        }
    }

    QStringList errors;
    const QList<cwFixStation> validFixes = cwSurvexExporterUtils::validateFixStations(
        node.fixStations, stationNamesLower, errors);
    for (const QString& message : std::as_const(errors)) {
        Errors.append(message);
    }

    const QString fallbackStation = anchoredAbove ? QString() : firstValidStation;
    cwSurvexExporterUtils::writeFixStations(stream, validFixes, fallbackStation, globalCS);
    return !validFixes.isEmpty() || !fallbackStation.isEmpty();
}

bool cwSurvexExporterCaveTask::writeExternalInclude(QTextStream& stream,
                                                    const QUuid& ownerId,
                                                    const QHash<QUuid, QString>& attachmentDirs,
                                                    const QString& entryFile,
                                                    const QString& ownerLabel)
{
    const auto it = attachmentDirs.constFind(ownerId);
    if (it == attachmentDirs.constEnd()) {
        Errors.append(QStringLiteral(
            "External centerline for '%1' has no resolved attachment directory "
            "(reconcile did not run before the driver export).").arg(ownerLabel));
        return false;
    }
    if (entryFile.isEmpty()) {
        Errors.append(QStringLiteral(
            "External centerline for '%1' has an empty entry file.").arg(ownerLabel));
        return false;
    }

    const QString absolutePath = driverIncludePathFor(it.value(), entryFile);

    // Refuse before we emit; the user surfaces this as an export error,
    // not a confusing cavern parse failure.
    if (!cwSurvexExporterUtils::isQuotableIncludePath(absolutePath)) {
        Errors.append(QStringLiteral(
            "External centerline path for '%1' contains characters that "
            "Survex *include cannot quote (\", newline, carriage return or Ctrl-Z): %2")
            .arg(ownerLabel, absolutePath));
        return false;
    }

    stream << "*include \"" << absolutePath << "\"" << Qt::endl;
    return true;
}
