/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterCaveTask.h"
#include "cwCompassMakFile.h"
#include "cwCompassMakTranslator.h"
#include "cwExternalCenterlineScanner.h"
#include "cwFixStationDiagnostics.h"
#include "cwLocalProjection.h"
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
                                                 const QSet<QUuid>& excludedExternalOwners,
                                                 const QList<cwEquate>& equates) :
    m_nodes(nodes),
    m_labels(labels),
    m_excludedExternalOwners(excludedExternalOwners),
    m_equates(equates)
{
    for (const cwCaveData& node : m_nodes) {
        index(node, false);
    }
}

// Returns whether \a node emits. A container under an excluded owner has no
// scope, and neither does a node below a sourced root's own level: the root is
// one *include, and nothing beneath it is a block. The trips inside a sourced
// subtree keep a scope through the root (indexSourcedSubtree).
bool cwSurvexExporterCaveTask::DriverTree::index(const cwCaveData& node, bool scopeClosedAbove)
{
    const bool scopeClosed = scopeClosedAbove || m_excludedExternalOwners.contains(node.id);
    const bool sourced = !node.externalCenterline.isEmpty();

    if (sourced && isSourceRoot(node)) {
        indexSourcedSubtree(node, node, scopeClosed);
        const bool emits = !m_excludedExternalOwners.contains(node.id);
        if (emits) {
            m_emittingNodeIds.insert(node.id);
        }
        return emits;
    }

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

// Registers every node of \a root's subtree as a scope-less owner (none is a
// block) and every trip as an owner the root's block names, pre-order.
void cwSurvexExporterCaveTask::DriverTree::indexSourcedSubtree(const cwCaveData& root,
                                                               const cwCaveData& node,
                                                               bool scopeClosed)
{
    m_nodeOwners.insert(node.id, {&node, nullptr, &node == &root && !scopeClosed, &root});
    for (const cwTripData& trip : node.trips) {
        m_tripOwners.insert(trip.id, {&node, &trip, !scopeClosed, &root});
        if (isNativeTrip(trip) && chunksHoldStations(trip)) {
            m_sourcedNativeTrips[root.id].append({&node, &trip});
        }
    }
    for (const cwCaveData& child : node.nodes) {
        indexSourcedSubtree(root, child, scopeClosed);
    }
}

// A derived trip's station sits in its unit set, named below its node's cavern
// path and its own scope; a native trip's below its label in the root's block.
QString cwSurvexExporterCaveTask::DriverTree::sourcedTripOperand(const Owner& owner,
                                                                 const QString& tail) const
{
    const QString rootPrefix = m_labels.prefix(owner.sourceRoot->id);
    if (rootPrefix.isEmpty()) {
        return QString();
    }

    if (!owner.trip->sourcePath.isEmpty()) {
        // The unit spells each name the way cavern reads it from the file, so
        // that spelling, not the handle's, is what the tie names.
        const QString key = cwStation::canonicalKey(tail);
        const auto spelled = std::find_if(owner.trip->externalStations.cbegin(),
                                          owner.trip->externalStations.cend(),
                                          [&key](const QString& station) {
                                              return cwStation::canonicalKey(station) == key;
                                          });
        if (spelled == owner.trip->externalStations.cend()) {
            return QString();
        }
        return cwStation::canonicalKey(rootPrefix + m_labels.cavernPath(owner.node->id)
                                       + cwTrip::scopePrefix(*owner.trip, m_labels, owner.node->id))
               + *spelled;
    }

    const QString tripScope = cwTrip::scopePrefix(*owner.trip, m_labels, owner.node->id);
    if (tripScope.isEmpty()) {
        return QString();
    }
    return cwStation::canonicalKey(rootPrefix + tripScope + tail);
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
    if (ownerIt == owners.constEnd() || !ownerIt->hasScope) {
        return QString();
    }
    // Inside a sourced subtree the root's block is the one that opens a scope.
    const cwCaveData* block = ownerIt->sourceRoot != nullptr ? ownerIt->sourceRoot : ownerIt->node;
    if (!emits(block->id)) {
        return QString();
    }

    const Owner& owner = ownerIt.value();
    if (owner.trip != nullptr && owner.sourceRoot != nullptr) {
        return sourcedTripOperand(owner, handle.tail());
    }

    const QString nodePrefix = m_labels.prefix(owner.node->id);
    if (nodePrefix.isEmpty()) {
        return QString();
    }

    // Cavern reads a name the driver writes under *case lower, so the
    // canonical spelling names the same station.
    if (owner.trip == nullptr) {
        return cwStation::canonicalKey(nodePrefix + handle.tail());
    }
    return cwStation::canonicalKey(nodePrefix
                                   + cwTrip::scopePrefix(*owner.trip, m_labels, owner.node->id)
                                   + handle.tail());
}

// Walks the tree in writeNodeBlock's output order, validating each written
// node's fixes and listing the fallbacks today's top-down rule would write.
// Then it joins stations the way cavern does (a chunk's shots, each attached
// file taken as one piece, every written tie) and keeps a fallback only for a
// group of stations that no real fix and no earlier fallback reaches.
class cwSurvexExporterCaveTask::AnchorPlan::Builder
{
public:
    Builder(AnchorPlan& plan, const DriverTree& tree,
            const QHash<QUuid, QStringList>& externalFixedStations) :
        m_plan(plan),
        m_tree(tree),
        m_externalFixedStations(externalFixedStations)
    {
    }

    void build()
    {
        for (const cwCaveData& node : m_tree.nodes()) {
            walkNode(node, false);
        }
        joinTies();
        chooseFallbacks();
    }

private:
    // The station keys validation accepts for one node, relative to the
    // node's block.
    struct NodeStations {
        //! Each chunk's valid stations: its shots join them.
        QList<QStringList> chunkKeys;
        QString firstValidStation;
        QString firstValidKey;
        //! Keyed by the attached file's owner: a trip, or the node itself.
        QHash<QUuid, QSet<QString>> fileKeys;
    };

    // A fallback today's top-down rule writes, unless its group has an anchor.
    struct Fallback {
        QUuid blockId;
        QString station;
        //! The station's region-qualified canonical key.
        QString key;
        bool attachedFile = false;
    };

    AnchorPlan& m_plan;
    const DriverTree& m_tree;
    const QHash<QUuid, QStringList>& m_externalFixedStations;
    cwSurvexExporterUtils::StationKeyUnion m_stations;
    //! Region-qualified keys of stations a real fix holds in place.
    QList<QString> m_realAnchors;
    //! In output order.
    QList<Fallback> m_fallbacks;

    template<typename Keys>
    void joinAll(const QString& prefix, const Keys& keys)
    {
        if (keys.isEmpty()) {
            return;
        }
        const QString first = prefix + *keys.cbegin();
        for (const QString& key : keys) {
            m_stations.join(first, prefix + key);
        }
    }

    void joinChunks(const QString& prefix, const QList<QStringList>& chunkKeys)
    {
        for (const QStringList& keys : chunkKeys) {
            joinAll(prefix, keys);
        }
    }

    // An attached file is one piece: cavern may still find it split, but
    // nothing short of solving it tells.
    void addFile(const QString& prefix, const QUuid& ownerId, const QString& scope,
                 QSet<QString> keys)
    {
        const QSet<QString> fixedKeys = cwSurvexExporterUtils::scopedStationKeys(
            scope, m_externalFixedStations.value(ownerId));
        keys.unite(fixedKeys);
        joinAll(prefix, keys);
        if (!fixedKeys.isEmpty()) {
            m_realAnchors.append(prefix + *fixedKeys.cbegin());
        }
    }

    void walkNode(const cwCaveData& node, bool anchoredAbove)
    {
        if (!m_tree.emits(node.id)) {
            return;
        }

        NodeStations stations;
        const NodeFixes fixes = validate(node, stations);
        m_plan.m_fixes.insert(node.id, fixes);
        const QString prefix = cwStation::canonicalKey(m_tree.labels().prefix(node.id));
        for (const cwFixStation& fix : fixes.kept) {
            m_realAnchors.append(prefix + cwStation::canonicalKey(fix.stationName().trimmed()));
        }
        joinChunks(prefix, stations.chunkKeys);

        if (!node.externalCenterline.isEmpty()) {
            walkSourcedRoot(node, prefix, fixes, stations, anchoredAbove);
            return;
        }

        const bool takesFallback = !anchoredAbove && fixes.kept.isEmpty()
                                   && !stations.firstValidStation.isEmpty();
        if (takesFallback) {
            m_fallbacks.append({node.id, stations.firstValidStation,
                                prefix + stations.firstValidKey, false});
        }
        const bool anchored = anchoredAbove || !fixes.kept.isEmpty() || takesFallback;

        for (const cwTripData& trip : node.trips) {
            if (trip.externalCenterline.isEmpty()
                || m_tree.excludedExternalOwners().contains(trip.id)) {
                continue;
            }
            const QString tripScope = cwTrip::scopePrefix(trip, m_tree.labels(), node.id);
            addFile(prefix, trip.id, tripScope, stations.fileKeys.value(trip.id));
            if (!anchored) {
                addFileFallback(trip.id, prefix + cwStation::canonicalKey(tripScope),
                                trip.externalCenterline, trip.externalStations);
            }
        }

        for (const cwCaveData& child : node.nodes) {
            walkNode(child, anchored);
        }
    }

    void addFileFallback(const QUuid& ownerId, const QString& scopeKey,
                         const cwExternalCenterline& centerline, const QStringList& externalStations)
    {
        const QString origin = originStation(centerline, externalStations,
                                             m_externalFixedStations.value(ownerId));
        if (!origin.isEmpty()) {
            m_fallbacks.append({ownerId, origin, scopeKey + cwStation::canonicalKey(origin), true});
        }
    }

    // A sourced root is one block: its *include, whose stations include every
    // derived trip's, and its subtree's native trips.
    void walkSourcedRoot(const cwCaveData& root, const QString& prefix, const NodeFixes& fixes,
                         const NodeStations& stations, bool anchoredAbove)
    {
        QSet<QString> fileKeys = stations.fileKeys.value(root.id);
        addDerivedStations(root, fileKeys);
        addFile(prefix, root.id, QString(), fileKeys);
        if (!anchoredAbove && fixes.kept.isEmpty()) {
            addFileFallback(root.id, prefix, root.externalCenterline, root.externalStations);
        }
    }

    // A derived trip's stations, relative to the root's block, named as
    // DriverTree::operand names them.
    void addDerivedStations(const cwCaveData& node, QSet<QString>& keys) const
    {
        const cwScopeLabels& labels = m_tree.labels();
        for (const cwTripData& trip : node.trips) {
            if (trip.sourcePath.isEmpty()) {
                continue;
            }
            keys.unite(cwSurvexExporterUtils::scopedStationKeys(
                labels.cavernPath(node.id) + cwTrip::scopePrefix(trip, labels, node.id),
                trip.externalStations));
        }
        for (const cwCaveData& child : node.nodes) {
            addDerivedStations(child, keys);
        }
    }

    // Validates \a node's fixes against the station names its block knows
    // (writeFixStations' doc comment), filling \a stations with those names.
    NodeFixes validate(const cwCaveData& node, NodeStations& stations) const
    {
        const cwScopeLabels& labels = m_tree.labels();
        QSet<QString> stationNamesLower;
        const auto addChunkStations = [&](const cwTripData& trip, const QString& tripScope) {
            for (const cwSurveyChunkData& chunk : trip.chunks) {
                QStringList& chunkKeys = stations.chunkKeys.emplaceBack();
                for (const cwStation& station : chunk.stations) {
                    if (station.isValid()) {
                        const QString key = cwStation::canonicalKey(tripScope + station.name());
                        chunkKeys.append(key);
                        stationNamesLower.insert(key);
                        if (stations.firstValidStation.isEmpty()) {
                            stations.firstValidStation = station.name();
                            stations.firstValidKey = key;
                        }
                    }
                }
            }
        };
        if (isSourceRoot(node)) {
            // Every native trip of the subtree sits under its label in the
            // root's block, so the root's fixes may name any of them.
            for (const DriverTree::SourcedTrip& sourced : m_tree.sourcedNativeTrips(node.id)) {
                addChunkStations(*sourced.trip,
                                 cwTrip::scopePrefix(*sourced.trip, labels, sourced.node->id));
            }
        } else {
            for (const cwTripData& trip : node.trips) {
                // Empty for a native trip in a native node.
                const QString tripScope = trip.externalCenterline.isEmpty()
                    ? cwTrip::scopePrefix(trip, labels, node.id)
                    : QString();
                addChunkStations(trip, tripScope);
            }
        }

        // The stations the attached files fix themselves, under the same keys,
        // each with the name of the file that fixes it.
        QHash<QString, QString> fileFixedStations;
        // The stations of attached Compass .mak files, under the same keys,
        // each with its owner and the owner's scope: a fix on one is written
        // inside the Survex the .mak is translated into, where its case is kept.
        struct MakOwner {
            QUuid ownerId;
            QString scope;
        };
        QHash<QString, MakOwner> makStationOwners;
        const auto addFileStations = [&](const QUuid& ownerId, const QString& scope,
                                         const cwExternalCenterline& centerline,
                                         const QStringList& fileStations) {
            const QSet<QString> stationKeys =
                cwSurvexExporterUtils::scopedStationKeys(scope, fileStations);
            stations.fileKeys.insert(ownerId, stationKeys);
            stationNamesLower.unite(stationKeys);
            if (cwCompassMakFile::isMakFile(centerline.entryFile())) {
                for (const QString& key : stationKeys) {
                    makStationOwners.insert(key, {ownerId, scope});
                }
            }
            const QString fileName = QFileInfo(centerline.entryFile()).fileName();
            const QSet<QString> fixedKeys = cwSurvexExporterUtils::scopedStationKeys(
                scope, m_externalFixedStations.value(ownerId));
            for (const QString& key : fixedKeys) {
                fileFixedStations.insert(key, fileName);
            }
        };

        for (const cwTripData& trip : node.trips) {
            if (trip.externalCenterline.isEmpty()
                || m_tree.excludedExternalOwners().contains(trip.id)) {
                continue;
            }
            addFileStations(trip.id, cwTrip::scopePrefix(trip, labels, node.id),
                            trip.externalCenterline, trip.externalStations);
        }
        addFileStations(node.id, QString(), node.externalCenterline, node.externalStations);

        NodeFixes fixes;
        const QList<cwFixStation> validFixes = cwSurvexExporterUtils::validateFixStations(
            node.fixStations, stationNamesLower, fixes.errors);

        // Cavern rejects a second fix on a station at different coordinates
        // (error 46) and fails the whole run, so the file's own fix stays in
        // force and the node's is dropped.
        fixes.kept.reserve(validFixes.size());
        for (const cwFixStation& fix : validFixes) {
            const auto fixingFile =
                fileFixedStations.constFind(cwStation::canonicalKey(fix.stationName().trimmed()));
            if (fixingFile == fileFixedStations.constEnd()) {
                fixes.kept.append(fix);
            } else {
                fixes.errors.append(cwFixStationDiagnostics::fileFixCollisionMessage(
                    fixingFile.value(), fix.stationName().trimmed()));
            }
        }

        for (const cwFixStation& fix : std::as_const(fixes.kept)) {
            const QString name = fix.stationName().trimmed();
            const auto makOwner = makStationOwners.constFind(cwStation::canonicalKey(name));
            if (makOwner == makStationOwners.constEnd()) {
                fixes.direct.append(fix);
                continue;
            }
            cwFixStation inFileScope = fix;
            inFileScope.setStationName(name.mid(makOwner->scope.size()));
            fixes.makFixes[makOwner->ownerId].append(inFileScope);
        }
        return fixes;
    }

    // Joins the operands of every tie writeEquateLine writes. An operand that
    // names no station of its block joins nothing: cavern creates that
    // station rather than reach the block through it.
    void joinTies()
    {
        for (const cwEquate& equate : m_tree.equates()) {
            if (!equate.isValid()) {
                continue;
            }
            const QList<cwStationHandle> handles = equate.stations();
            QStringList operands;
            operands.reserve(handles.size());
            for (const cwStationHandle& handle : handles) {
                const QString operand = m_tree.operand(handle);
                if (operand.isEmpty()) {
                    operands.clear();
                    break;
                }
                operands.append(cwStation::canonicalKey(operand));
            }
            for (const QString& operand : std::as_const(operands)) {
                m_stations.join(operands.first(), operand);
            }
        }
    }

    void chooseFallbacks()
    {
        QSet<QString> anchoredGroups;
        for (const QString& anchor : std::as_const(m_realAnchors)) {
            anchoredGroups.insert(m_stations.root(anchor));
        }
        for (const Fallback& fallback : std::as_const(m_fallbacks)) {
            const QString group = m_stations.root(fallback.key);
            if (anchoredGroups.contains(group)) {
                continue;
            }
            anchoredGroups.insert(group);
            m_plan.m_fallbackStations.insert(fallback.blockId, fallback.station);
            if (fallback.attachedFile) {
                m_plan.m_originFixedOwners.insert(fallback.blockId);
            }
        }
    }
};

cwSurvexExporterCaveTask::AnchorPlan::AnchorPlan(const DriverTree& tree,
                                                 const QHash<QUuid, QStringList>& externalFixedStations)
{
    Builder(*this, tree, externalFixedStations).build();
}

const cwSurvexExporterCaveTask::AnchorPlan::NodeFixes&
cwSurvexExporterCaveTask::AnchorPlan::fixes(const QUuid& nodeId) const
{
    static const NodeFixes kNoFixes;
    const auto it = m_fixes.constFind(nodeId);
    return it == m_fixes.constEnd() ? kNoFixes : it.value();
}

QString cwSurvexExporterCaveTask::AnchorPlan::fallbackStation(const QUuid& blockId) const
{
    return m_fallbackStations.value(blockId);
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
    const DriverTree tree(nodes, cwScopeLabels::forNode(cave), ExportOptions.excludedExternalOwners,
                          m_equates);

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

    const AnchorPlan plan(tree, ExportOptions.externalFixedStations);
    if (!writeNode(stream, nodes.first(), tree, plan, globalCS)) {
        return false;
    }

    // Ties emit after the cave's block closes, fully qualified, as the region
    // driver does. A tie reaching outside this cave renders no operand there,
    // so writeEquateLine drops it.
    writeEquates(stream, tree.equates(), tree);
    return true;
}

void cwSurvexExporterCaveTask::setEquates(const QList<cwEquate>& equates)
{
    m_equates = equates;
}

bool cwSurvexExporterCaveTask::writeNode(QTextStream& stream,
                                         const cwCaveData& node,
                                         const DriverTree& tree,
                                         const AnchorPlan& plan,
                                         const QString& globalCS)
{
    TotalProgress = 0;
    // Each top-level block starts with no input *cs in scope: the region
    // block above names only *cs out.
    const cwSurvexExporterUtils::CsScope csScope(sidecars());
    return writeNodeBlock(stream, node, tree, plan, globalCS, Inherited(), csScope);
}

bool cwSurvexExporterCaveTask::writeNodeBlock(QTextStream& stream,
                                              const cwCaveData& node,
                                              const DriverTree& tree,
                                              const AnchorPlan& plan,
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

    // Sourced root: the node's own fixes, the *include, then the subtree's
    // native trips; calibrations and child nodes are skipped. The included
    // file carries its own *cs, *fix, and *begin/*end structure; emitting our
    // own would either silently shadow theirs (no-op) or fight them (cavern
    // error).
    if (!node.externalCenterline.isEmpty()) {
        if (inherited.autoDeclinationInScope) {
            cwSurvexExporterUtils::writeDeclinationReset(stream);
        }
        // The node's own fixes name the file's stations in this block's scope,
        // except a station the file fixes itself, whose fix validation drops.
        // A file that fixes nothing sits at the origin unless a tie anchors
        // it, like a native node with no fix.
        cwSurvexExporterUtils::CsScope includeScope(enclosingScope);
        cwSurvexExporterUtils::CsScope fixesScope(includeScope);
        QString fixesText;
        QTextStream fixesStream(&fixesText);
        const AnchorPlan::NodeFixes& written =
            writeFixStations(fixesStream, node, plan, globalCS, fixesScope);
        fixesStream.flush();
        // A system the node's own fixes name would stay in force at the
        // *include and read the file's bare fixes in it. With no system
        // chosen for those, an anonymous block ends it after the fixes, so
        // only the system writeIncludeSystem names reads them.
        const bool fixesLeaveSystem = fixesScope.system() != includeScope.system()
                                      && node.externalCenterline.fixCoordinateSystem().isEmpty();
        if (fixesLeaveSystem) {
            stream << "*begin" << Qt::endl << fixesText << "*end" << Qt::endl;
        } else {
            stream << fixesText;
        }
        cwSurvexExporterUtils::CsScope& scopeAtInclude = fixesLeaveSystem ? includeScope : fixesScope;
        writeIncludeSystem(stream, node.id, node.externalCenterline, globalCS, scopeAtInclude);
        if (!writeExternalInclude(stream, node.id,
                                  ExportOptions.caveAttachmentDirs,
                                  node.externalCenterline.entryFile(),
                                  node.name, globalCS, scopeAtInclude,
                                  written.makFixes.value(node.id))) {
            return false;
        }

        // The native trips anywhere in the subtree, each its own block in the
        // root's scope under a label no scope of the file takes (§7.2 step 5).
        // The reset above took any auto declination out of scope.
        for (const DriverTree::SourcedTrip& sourced : tree.sourcedNativeTrips(node.id)) {
            const QString tripLabel = labels.tripLabels(sourced.node->id).value(sourced.trip->id);
            stream << "*begin " << tripLabel << " ; " << sourced.trip->name << Qt::endl;
            TripExporter->writeTrip(stream, *sourced.trip, false, inherited.gridConvergence);
            TotalProgress += cwSurvexExporter::stationCount(*sourced.trip);
            stream << "*end " << tripLabel << Qt::endl << Qt::endl;
        }

        writeEnd();
        return true;
    }

    // Cavern scopes *cs to its *begin block and copies the enclosing one in,
    // so this block starts with whatever system its parent left in force.
    cwSurvexExporterUtils::CsScope csScope(enclosingScope);

    // A system this block leaves in force would read the bare fixes of an
    // *include below it whose file has none chosen. Such a block writes its
    // fixes, and each auto declination's *cs, in anonymous blocks that end
    // them, so only the system writeIncludeSystem names reads those fixes.
    const bool shielded = scopeReachesInheritingInclude(node, tree);

    Inherited here = inherited;
    cwSurvexExporterUtils::CsScope fixesScope(csScope);
    QString fixesText;
    QTextStream fixesStream(&fixesText);
    const AnchorPlan::NodeFixes& written =
        writeFixStations(fixesStream, node, plan, globalCS, fixesScope);
    fixesStream.flush();
    const bool fixesLeaveSystem = shielded && fixesScope.system() != csScope.system();
    if (fixesLeaveSystem) {
        stream << "*begin" << Qt::endl << fixesText << "*end" << Qt::endl;
    } else {
        stream << fixesText;
    }
    cwSurvexExporterUtils::CsScope& nodeScope = fixesLeaveSystem ? csScope : fixesScope;

    // A node with a location of its own declares `*declination auto` for its
    // whole subtree and sets the grid convergence below it. A node without one
    // inherits both from the nearest located ancestor. The location comes from
    // the fixes just written: a dropped fix's *cs would land after the fallback
    // *fix, which cavern rejects ("fixed before CS command first used").
    // A shielded node leaves the location undeclared, and each auto trip, or
    // the first unshielded block below, declares it in a scope of its own.
    const auto ownLocation = cwSurvexExporterUtils::makeDeclinationContext(written.kept);
    if (ownLocation.has_value()) {
        here.undeclaredLocation = ownLocation;
        // One convergence for the whole block: it is a property of the grid at
        // the node's location, and every trip inside is solved on the same grid.
        here.gridConvergence = cwSurvexExporterUtils::gridConvergenceForBlock(ownLocation, globalCS);
    }
    if (!shielded && here.undeclaredLocation.has_value() && subtreeUsesAutoDeclination(node)) {
        cwSurvexExporterUtils::writeDeclinationAuto(stream, *here.undeclaredLocation, nodeScope);
        here.autoDeclinationInScope = true;
        here.undeclaredLocation.reset();
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
            cwSurvexExporterUtils::CsScope includeScope(nodeScope);
            const QString tripOriginStation = plan.fallbackStation(tripData.id);
            if (!tripOriginStation.isEmpty()) {
                cwSurvexExporterUtils::writeFixStations(stream, {}, tripOriginStation, globalCS,
                                                        includeScope);
            }
            writeIncludeSystem(stream, tripData.id, tripData.externalCenterline, globalCS,
                               includeScope);
            if (!writeExternalInclude(stream, tripData.id,
                                      ExportOptions.tripAttachmentDirs,
                                      tripData.externalCenterline.entryFile(),
                                      tripData.name, globalCS, includeScope,
                                      written.makFixes.value(tripData.id))) {
                return false;
            }
            stream << "*end " << tripLabel << Qt::endl << Qt::endl;
            continue;
        }

        const bool declaresOwnAuto = !here.autoDeclinationInScope
                                     && here.undeclaredLocation.has_value()
                                     && tripData.calibrations.autoDeclination();
        if (declaresOwnAuto) {
            stream << "*begin" << Qt::endl;
            cwSurvexExporterUtils::CsScope tripScope(nodeScope);
            cwSurvexExporterUtils::writeDeclinationAuto(stream, *here.undeclaredLocation, tripScope);
            TripExporter->writeTrip(stream, tripData, true, here.gridConvergence);
            stream << "*end" << Qt::endl;
        } else {
            TripExporter->writeTrip(stream, tripData, here.autoDeclinationInScope, here.gridConvergence);
        }
        TotalProgress += cwSurvexExporter::stationCount(tripData);
        stream << Qt::endl;
    }

    for (const cwCaveData& child : node.nodes) {
        if (!writeNodeBlock(stream, child, tree, plan, globalCS, here, nodeScope)) {
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
 * can't bind. Two distinct handles can still render to the same string (a
 * station two derived units share), so the line is de-duplicated and skipped
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

    // An operand naming a Compass or Walls station keeps the file's case,
    // which cavern's default *case lower would fold away; the block keeps
    // the *case preserve to this one line.
    const bool keepsCase = std::any_of(distinct.cbegin(), distinct.cend(),
                                       [](const QString& operand) {
                                           return operand != cwStation::canonicalKey(operand);
                                       });
    if (keepsCase) {
        stream << "*begin" << Qt::endl << "*case preserve" << Qt::endl;
    }
    stream << "*equate " << distinct.join(QLatin1Char(' ')) << Qt::endl;
    if (keepsCase) {
        stream << "*end" << Qt::endl;
    }
}

/**
 * Emit the *cs / *fix block for the node from \a plan. The plan validated the
 * snapshot's fixStations before output started, against the station names the
 * node's block knows: its native chunk stations, each attached trip's
 * harvested stations under the trip's scope ("<tripLabel>.doghill.d1"), and
 * its own attached file's harvested stations ("doghill.d1"). Those are the
 * names the node's solved network carries, so the Fix Stations page and the
 * driver agree on one spelling, and the *fix written from this block resolves
 * to the file's station inside its *include. A fix on an attached Compass
 * .mak's station is returned in NodeFixes::makFixes rather than written here.
 * Rejected fixes, and fixes on a station an attached file fixes itself, are
 * left out and their reasons appended to Errors. The node writes
 * `*fix <firstStation> 0 0 0` only when the plan gives it the fallback: it
 * keeps no valid fix, no enclosing block anchors it, and no validated fix,
 * self-fixing file or earlier fallback reaches its station.
 * cwScopeStationListModel's node mode restates this name set on the live tree
 * for the station autocomplete; keep the two in step.
 */
const cwSurvexExporterCaveTask::AnchorPlan::NodeFixes&
cwSurvexExporterCaveTask::writeFixStations(QTextStream &stream, const cwCaveData &node,
                                           const AnchorPlan& plan,
                                           const QString& globalCS,
                                           cwSurvexExporterUtils::CsScope& scope)
{
    const AnchorPlan::NodeFixes& fixes = plan.fixes(node.id);
    Errors.append(fixes.errors);
    cwSurvexExporterUtils::writeFixStations(stream, fixes.direct, plan.fallbackStation(node.id),
                                            globalCS, scope);
    return fixes;
}

bool cwSurvexExporterCaveTask::readsInheritedSystem(const QUuid& ownerId,
                                                    const cwExternalCenterline& centerline) const
{
    return centerline.fixCoordinateSystem().isEmpty()
           && !ExportOptions.externalFixedStations.value(ownerId).isEmpty();
}

bool cwSurvexExporterCaveTask::scopeReachesInheritingInclude(const cwCaveData& node,
                                                             const DriverTree& tree) const
{
    if (!tree.emits(node.id)) {
        return false;
    }
    if (!node.externalCenterline.isEmpty()) {
        return readsInheritedSystem(node.id, node.externalCenterline);
    }
    const bool tripReaches = std::any_of(node.trips.cbegin(), node.trips.cend(),
                                         [this, &tree](const cwTripData& trip) {
                                             return !trip.externalCenterline.isEmpty()
                                                    && !tree.excludedExternalOwners().contains(trip.id)
                                                    && readsInheritedSystem(trip.id, trip.externalCenterline);
                                         });
    return tripReaches
           || std::any_of(node.nodes.cbegin(), node.nodes.cend(),
                          [this, &tree](const cwCaveData& child) {
                              return scopeReachesInheritingInclude(child, tree);
                          });
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

void cwSurvexExporterCaveTask::writeIncludeSystem(QTextStream& stream, const QUuid& ownerId,
                                                  const cwExternalCenterline& centerline,
                                                  const QString& globalCS,
                                                  cwSurvexExporterUtils::CsScope& scope)
{
    const QString fileOutput = ExportOptions.externalOutputSystems.value(ownerId);
    if (globalCS.isEmpty()) {
        // The file's own *cs out, met later, names the same system and is a
        // no-op. With no output system at all, cavern reads the fixes as
        // written and refuses any input *cs, chosen or not.
        if (fileOutput.isEmpty()) {
            return;
        }
        scope.writeFileOutputSystem(stream, fileOutput);
        scope.ensure(stream, centerline.fixCoordinateSystem());
        return;
    }

    // The file's own *cs still wins inside its blocks; this only reaches the
    // fixes that name none: the system chosen for them, else the file's own
    // *cs out, else the output system moved so the first of them lands on the
    // origin.
    const QString chosen = centerline.fixCoordinateSystem();
    if (!chosen.isEmpty()) {
        scope.ensure(stream, chosen);
        return;
    }
    if (!fileOutput.isEmpty()) {
        scope.ensure(stream, fileOutput);
        return;
    }
    const auto origin = ExportOptions.externalBareFixOrigins.constFind(ownerId);
    if (origin != ExportOptions.externalBareFixOrigins.constEnd()) {
        scope.ensure(stream, cwLocalProjection::translated(globalCS, origin.value()));
    }
}

bool cwSurvexExporterCaveTask::writeExternalInclude(QTextStream& stream,
                                                    const QUuid& ownerId,
                                                    const QHash<QUuid, QString>& attachmentDirs,
                                                    const QString& entryFile,
                                                    const QString& ownerLabel,
                                                    const QString& globalCS,
                                                    cwSurvexExporterUtils::CsScope& scope,
                                                    const QList<cwFixStation>& makFixes)
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

    if (cwCompassMakFile::isMakFile(absolutePath)) {
        const auto translated =
            cwCompassMakTranslator::translate({absolutePath, globalCS, makFixes}, scope);
        if (translated.has_value()) {
            stream << *translated;
            return true;
        }
        // A .mak the translation cannot spell is read by cavern itself, which
        // places its fixes at their raw numbers.
        cwSurvexExporterUtils::writeFixStations(stream, makFixes, QString(), globalCS, scope);
    }

    stream << "*include \"" << absolutePath << "\"" << Qt::endl;
    return true;
}
