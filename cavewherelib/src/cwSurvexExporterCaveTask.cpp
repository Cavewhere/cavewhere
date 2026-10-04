/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterCaveTask.h"
#include "cwExternalCenterlineScanner.h"
#include "cwFixStationDiagnostics.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurvexExporter.h"
#include "cwSurvexExporterUtils.h"
#include "cwSurvexCS.h"
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

// True when a native trip written inside \a node's block uses auto
// declination. A sourced node's trips are windows on its *include, never
// written, so the walk stops there.
bool subtreeUsesAutoDeclination(const cwCaveData& node)
{
    if (!node.externalCenterline.isEmpty()) {
        return false;
    }
    const bool ownTripUsesAuto = std::any_of(node.trips.cbegin(), node.trips.cend(),
                                             [](const cwTripData& trip) {
                                                 return trip.externalCenterline.isEmpty()
                                                        && trip.calibrations.autoDeclination();
                                             });
    return ownTripUsesAuto
           || std::any_of(node.nodes.cbegin(), node.nodes.cend(), subtreeUsesAutoDeclination);
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
    // Each top-level block starts with no input *cs in scope: the region
    // block above names only *cs out.
    const cwSurvexExporterUtils::CsScope csScope(sidecars());
    return writeNodeBlock(stream, node, tree, globalCS, Inherited(), csScope);
}

bool cwSurvexExporterCaveTask::writeNodeBlock(QTextStream& stream,
                                              const cwCaveData& node,
                                              const DriverTree& tree,
                                              const QString& globalCS,
                                              const Inherited& inherited,
                                              const cwSurvexExporterUtils::CsScope& enclosingScope)
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

    // Sourced root: the node's own fixes, then the *include; calibrations,
    // trips and child nodes are skipped. The included file carries its own
    // *cs, *fix, and *begin/*end structure; emitting our own would either
    // silently shadow theirs (no-op) or fight them (cavern error). Per master
    // plan §6, native + external are not mixed inside the same node body.
    if (!node.externalCenterline.isEmpty()) {
        if (inherited.autoDeclinationInScope) {
            cwSurvexExporterUtils::writeDeclinationReset(stream);
        }
        // The node's own fixes name the file's stations in this block's scope,
        // except a station the file fixes itself, whose fix writeFixStations
        // drops. A file that fixes nothing sits at the origin, like a native
        // node with no fix.
        cwSurvexExporterUtils::CsScope includeScope(enclosingScope);
        writeFixStations(stream, node, tree, globalCS, inherited.anchored, includeScope);
        // A georeferenced run names *cs out, after which cavern refuses any
        // *fix with no input system. The file's own *cs still wins inside
        // its blocks; this only catches the bare ones.
        includeScope.ensureAnySystem(stream, globalCS);
        if (!writeExternalInclude(stream, node.id,
                                  ExportOptions.caveAttachmentDirs,
                                  node.externalCenterline.entryFile(),
                                  node.name)) {
            return false;
        }
        writeEnd();
        return true;
    }

    // Cavern scopes *cs to its *begin block and copies the enclosing one in,
    // so this block starts with whatever system its parent left in force.
    cwSurvexExporterUtils::CsScope csScope(enclosingScope);

    Inherited here = inherited;
    const WrittenFixes written =
        writeFixStations(stream, node, tree, globalCS, inherited.anchored, csScope);
    here.anchored = written.anchored || inherited.anchored;

    // A node with a location of its own declares `*declination auto` for its
    // whole subtree and sets the grid convergence below it. A node without one
    // inherits both from the nearest located ancestor. The location comes from
    // the fixes just written: a dropped fix's *cs would land after the fallback
    // *fix, which cavern rejects ("fixed before CS command first used").
    const auto ownLocation = cwSurvexExporterUtils::makeDeclinationContext(written.kept);
    if (ownLocation.has_value()) {
        if (cwSurvexExporterUtils::writeBlockDeclinationAuto(stream, written.kept,
                                                             subtreeUsesAutoDeclination(node),
                                                             csScope)) {
            here.autoDeclinationInScope = true;
        }
        // One convergence for the whole block: it is a property of the grid at
        // the node's location, and every trip inside is solved on the same grid.
        here.gridConvergence = cwSurvexExporterUtils::gridConvergenceForBlock(ownLocation, globalCS);
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
                cwSurvexExporterUtils::writeDeclinationCalibration(stream,
                                                                   /*autoDeclination*/ false,
                                                                   declinationIt.value(),
                                                                   here.autoDeclinationInScope,
                                                                   here.gridConvergence);
            } else if (here.autoDeclinationInScope) {
                cwSurvexExporterUtils::writeDeclinationReset(stream);
            }
            cwSurvexExporterUtils::CsScope includeScope(csScope);
            const QString tripOriginStation =
                here.anchored ? QString()
                              : originStation(tripData.externalCenterline, tripData.externalStations,
                                              ExportOptions.externalFixedStations.value(tripData.id));
            if (!tripOriginStation.isEmpty()) {
                cwSurvexExporterUtils::writeFixStations(stream, {}, tripOriginStation, globalCS,
                                                        includeScope);
            }
            includeScope.ensureAnySystem(stream, globalCS);
            if (!writeExternalInclude(stream, tripData.id,
                                      ExportOptions.tripAttachmentDirs,
                                      tripData.externalCenterline.entryFile(),
                                      tripData.name)) {
                return false;
            }
            stream << "*end " << tripLabel << Qt::endl << Qt::endl;
            continue;
        }

        TripExporter->writeTrip(stream, tripData, here.autoDeclinationInScope, here.gridConvergence);
        TotalProgress += cwSurvexExporter::stationCount(tripData);
        stream << Qt::endl;
    }

    for (const cwCaveData& child : node.nodes) {
        if (!writeNodeBlock(stream, child, tree, globalCS, here, csScope)) {
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
 * fixStations against the station names the node's block knows: its native
 * chunk stations, each attached trip's harvested stations under the trip's
 * scope ("<tripLabel>.doghill.d1"), and its own attached file's harvested
 * stations ("doghill.d1"). Those are the names the node's solved network
 * carries, so the Fix Stations page and the driver agree on one spelling, and
 * the *fix written from this block resolves to the file's station inside its
 * *include. Rejected fixes, and fixes on a station an attached file fixes
 * itself, are dropped from the output and their reasons appended to Errors.
 * Falls back to `*fix <firstStation> 0 0 0` when no valid fix exists and no
 * enclosing block is anchored, so each un-fixed top-level survey still
 * resolves in cavern.
 */
cwSurvexExporterCaveTask::WrittenFixes
cwSurvexExporterCaveTask::writeFixStations(QTextStream &stream, const cwCaveData &node,
                                           const DriverTree& tree,
                                           const QString& globalCS, bool anchoredAbove,
                                           cwSurvexExporterUtils::CsScope& scope)
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

    // The stations the attached files fix themselves, under the same keys,
    // each with the name of the file that fixes it.
    QHash<QString, QString> fileFixedStations;
    const auto addFileStations = [&](const QUuid& ownerId, const QString& scope,
                                     const cwExternalCenterline& centerline,
                                     const QStringList& stations) {
        stationNamesLower.unite(cwSurvexExporterUtils::scopedStationKeys(scope, stations));
        const QString fileName = QFileInfo(centerline.entryFile()).fileName();
        const QSet<QString> fixedKeys = cwSurvexExporterUtils::scopedStationKeys(
            scope, ExportOptions.externalFixedStations.value(ownerId));
        for (const QString& key : fixedKeys) {
            fileFixedStations.insert(key, fileName);
        }
    };

    const QHash<QUuid, QString>& tripLabels = tree.labels().tripLabels(node.id);
    for (const cwTripData& trip : node.trips) {
        if (trip.externalCenterline.isEmpty()
            || tree.excludedExternalOwners().contains(trip.id)) {
            continue;
        }
        addFileStations(trip.id, cwTrip::scopePrefix(trip, tripLabels),
                        trip.externalCenterline, trip.externalStations);
    }
    addFileStations(node.id, QString(), node.externalCenterline, node.externalStations);

    QStringList errors;
    const QList<cwFixStation> validFixes = cwSurvexExporterUtils::validateFixStations(
        node.fixStations, stationNamesLower, errors);
    for (const QString& message : std::as_const(errors)) {
        Errors.append(message);
    }

    // Cavern rejects a second fix on a station at different coordinates
    // (error 46) and fails the whole run, so the file's own fix stays in
    // force and the node's is dropped.
    QList<cwFixStation> writtenFixes;
    writtenFixes.reserve(validFixes.size());
    for (const cwFixStation& fix : validFixes) {
        const auto fixingFile =
            fileFixedStations.constFind(cwStation::canonicalKey(fix.stationName().trimmed()));
        if (fixingFile == fileFixedStations.constEnd()) {
            writtenFixes.append(fix);
        } else {
            Errors.append(cwFixStationDiagnostics::fileFixCollisionMessage(
                fixingFile.value(), fix.stationName().trimmed()));
        }
    }

    // A sourced root has no native stations; the file's own first station
    // stands in for them when the file fixes nothing.
    const QString nativeOrFileStation = node.externalCenterline.isEmpty()
        ? firstValidStation
        : originStation(node.externalCenterline, node.externalStations,
                        ExportOptions.externalFixedStations.value(node.id));
    const QString fallbackStation = anchoredAbove ? QString() : nativeOrFileStation;
    cwSurvexExporterUtils::writeFixStations(stream, writtenFixes, fallbackStation, globalCS, scope);
    return { writtenFixes, !writtenFixes.isEmpty() || !fallbackStation.isEmpty() };
}

QString cwSurvexExporterCaveTask::originStation(const cwExternalCenterline& centerline,
                                                const QStringList& externalStations,
                                                const QStringList& fileFixedStations)
{
    const bool survexEntry = cwExternalCenterlineScanner::formatFor(centerline.entryFile())
                             == cwExternalCenterlineScanner::Format::Survex;
    if (!survexEntry || externalStations.isEmpty() || !fileFixedStations.isEmpty()) {
        return QString();
    }
    return externalStations.first();
}

QString cwSurvexExporterCaveTask::writeStandaloneHeader(QTextStream& stream)
{
    // Survex requires *cs out whenever any *cs appears, and the cave block is
    // about to emit one for its fixes. Exported from the region this comes from
    // the region writer; exported on its own, the cave has to name it itself or
    // cavern rejects the *fix outright. Any node in the cave's subtree can carry
    // that fix.
    const QString outputCS = cwSurvexExporterUtils::shareableCSForNodes(QList<cwCaveData>{Cave});
    if (outputCS.isEmpty()) {
        return QString();
    }

    cwSurvexCS::writeCsLine(stream, sidecars(), outputCS, true);
    stream << Qt::endl;
    return outputCS;
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
