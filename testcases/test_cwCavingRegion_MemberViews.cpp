/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

// Std
#include <algorithm>
#include <functional>
#include <limits>
#include <memory>

namespace {

constexpr int kShotsPerFile = 80;
// Enough legs per file that station-proportional work outweighs the fix
// station validator's revalidation, which grows with nodes squared.
constexpr int kFanOutShotsPerFile = 160;
constexpr int kSmallTreeFiles = 12;
constexpr int kLargeTreeFiles = 48;
constexpr int kNativeTreeFiles = 6;
constexpr int kStallRepeats = 3;
constexpr int kBinaryTreeChildren = 2;

// Each derived node's view holds its whole subtree, so a fan-out costs
// stations times depth. Eight children per file keeps the small and large trees
// at the same depth, so four times the files grows a view built from members
// about fourfold and one built by scanning the whole root about sixteenfold.
constexpr int kWideTreeChildren = 8;
constexpr double kMaxStallGrowth = 8.0;

//! The derived nodes under \a root: every node below it that is no source root.
QList<const cwSurveyNode*> derivedNodes(const cwSurveyNode* root)
{
    QList<const cwSurveyNode*> nodes;
    root->walk([&nodes, root](const cwSurveyNode* node) {
        if (node != root && !node->isSourceRoot()) {
            nodes.append(node);
        }
    });
    return nodes;
}

//! \a key, a canonical name in the source root's view, as \a nodeCavernPath's
//! node names it.
QString memberLocalKey(const QString& key, const QString& nodeCavernPath)
{
    return key.startsWith(nodeCavernPath) ? key.sliced(nodeCavernPath.size()) : key;
}

//! \a node's network built from the public pieces: the source root's view kept
//! to node->sourceMembers(), member names local to the node. A shot to a
//! non-member stays on the member's neighbor list without a station entry.
cwSurveyNetwork referenceNetwork(const cwSurveyNode* node)
{
    const cwSurveyNode::SourceMembers members = node->sourceMembers();
    const QString cavernPath = cwStation::canonicalKey(node->cavernPath());
    const cwSurveyNetwork rootNetwork = node->sourceRoot()->scopedNetwork();
    cwSurveyNetwork network;
    for (const QString& station : rootNetwork.stations()) {
        if (!members.contains(station)) {
            continue;
        }
        const QString local = memberLocalKey(station, cavernPath);
        for (const QString& neighbor : rootNetwork.neighbors(station)) {
            if (members.contains(neighbor)) {
                network.addShot(local, memberLocalKey(neighbor, cavernPath));
            } else {
                network.addNeighbor(local, neighbor);
            }
        }
        if (rootNetwork.hasPosition(station)) {
            network.setPosition(local, rootNetwork.position(station));
        }
    }
    return network;
}

//! \a node's positions built the same way as referenceNetwork().
QMap<QString, QVector3D> referencePositions(const cwSurveyNode* node)
{
    const cwSurveyNode::SourceMembers members = node->sourceMembers();
    const QString cavernPath = cwStation::canonicalKey(node->cavernPath());
    const QMap<QString, QVector3D> rootPositions = node->sourceRoot()->scopedPositions().positions();
    QMap<QString, QVector3D> positions;
    for (auto it = rootPositions.constBegin(); it != rootPositions.constEnd(); ++it) {
        if (members.contains(it.key())) {
            positions.insert(memberLocalKey(it.key(), cavernPath), it.value());
        }
    }
    return positions;
}

QStringList sorted(QStringList list)
{
    list.sort();
    return list;
}

//! Checks \a node's member views against the reference, station by station.
void checkMemberViews(const cwSurveyNode* node)
{
    INFO("node " << node->name().toStdString() << " (" << node->cavernPath().toStdString() << ")");
    const cwSurveyNetwork expected = referenceNetwork(node);
    const cwSurveyNetwork actual = node->scopedNetwork();
    CHECK(actual.stationCount() == expected.stationCount());
    const QStringList stations = sorted(expected.stations());
    CHECK(sorted(actual.stations()) == stations);
    for (const QString& station : stations) {
        INFO("station " << station.toStdString());
        CHECK(sorted(actual.neighbors(station)) == sorted(expected.neighbors(station)));
        CHECK(actual.hasPosition(station) == expected.hasPosition(station));
        CHECK(actual.position(station) == expected.position(station));
    }
    CHECK(node->scopedPositions().positions() == referencePositions(node));
}

//! Attaches \a source under the region's root, waits for the solve, and checks
//! every derived node's member views. Returns the source root.
cwSurveyNode* attachAndCheck(SavedProjectFixture* fixture, const QString& source)
{
    cwSurveyNode* root = attachSourceThroughManager(
        fixture, fixture->project->cavingRegion()->rootNode(), source);
    drainPipelines(fixture);
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    REQUIRE(root->scopedNetwork().stationCount() > 0);

    const QList<const cwSurveyNode*> nodes = derivedNodes(root);
    REQUIRE_FALSE(nodes.isEmpty());
    for (const cwSurveyNode* node : nodes) {
        checkMemberViews(node);
    }
    return root;
}

std::unique_ptr<SavedProjectFixture> makeProject(const QString& name)
{
    return makeSavedProject(name, QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
}

//! The shortest of kStallRepeats stalls of one member-view fan-out on \a
//! region: invalidateMemberViews(), then every trip's solvedStations(), as the
//! 3D label view reads them.
qint64 memberViewFanOutStall(cwCavingRegion* region)
{
    const QList<cwTrip*> trips = region->rootNode()->allTrips();
    qint64 shortest = std::numeric_limits<qint64>::max();
    for (int repeat = 0; repeat < kStallRepeats; ++repeat) {
        const qint64 stall = longestMainThreadStall([region, &trips]() {
            region->invalidateMemberViews();
            qsizetype labels = 0;
            for (const cwTrip* trip : trips) {
                labels += trip->solvedStations().size();
            }
            CHECK(labels > 0);
        });
        shortest = std::min(shortest, stall);
    }
    return shortest;
}

struct FanOut {
    qint64 stallMs = 0;
    int nodes = 0;
    int stations = 0;
};

FanOut measureGeneratedTree(int files)
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeSurvexTree(sourceDir.path(), files, kFanOutShotsPerFile, kWideTreeChildren);

    auto fixture = makeProject(QStringLiteral("member-views-fanout-%1").arg(files));
    cwSurveyNode* root = attachSourceThroughManager(
        fixture.get(), fixture->project->cavingRegion()->rootNode(), entry);
    drainPipelines(fixture.get());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());

    cwCavingRegion* region = fixture->project->cavingRegion();
    FanOut fanOut;
    fanOut.nodes = region->rootNode()->allNodes().size();
    fanOut.stations = root->scopedNetwork().stationCount();

    // Every node relays one scopedNetworkChanged per fan-out.
    std::vector<std::unique_ptr<QSignalSpy>> spies;
    for (const cwSurveyNode* node : region->rootNode()->allNodes()) {
        spies.push_back(std::make_unique<QSignalSpy>(node, &cwSurveyNode::scopedNetworkChanged));
    }
    region->invalidateMemberViews();
    int relays = 0;
    for (const auto& spy : spies) {
        relays += spy->count();
    }
    CHECK(relays == fanOut.nodes);

    fanOut.stallMs = memberViewFanOutStall(region);
    return fanOut;
}

} // namespace

TEST_CASE("A derived node's member views keep exactly its members' stations",
          "[cwCavingRegion][MemberViews]")
{
    // survex_blocks nests blocks under blocks that hold shots, so the attach
    // derives nodes (survex_master's leaf blocks are trips).
    SECTION("survex_blocks") {
        auto fixture = makeProject(QStringLiteral("member-views-survex"));
        attachAndCheck(fixture.get(), fixturePath(QStringLiteral("survex_blocks.svx")));
    }

    SECTION("compass_project") {
        auto fixture = makeProject(QStringLiteral("member-views-compass"));
        attachAndCheck(fixture.get(), fixturePath(QStringLiteral("compass_project/PROJECT.MAK")));
    }

    SECTION("walls_project") {
        auto fixture = makeProject(QStringLiteral("member-views-walls"));
        attachAndCheck(fixture.get(), fixturePath(QStringLiteral("walls_project/KAUA.WPJ")));
    }

    SECTION("a generated tree with a native trip under a derived node") {
        QTemporaryDir sourceDir;
        REQUIRE(sourceDir.isValid());
        const QString entry = writeSurvexTree(sourceDir.path(), kNativeTreeFiles, kShotsPerFile,
                                                 kBinaryTreeChildren);

        auto fixture = makeProject(QStringLiteral("member-views-native"));
        cwSurveyNode* root = attachAndCheck(fixture.get(), entry);

        const QList<const cwSurveyNode*> nodes = derivedNodes(root);
        auto* host = qobject_cast<cwCave*>(const_cast<cwSurveyNode*>(nodes.at(1)));
        REQUIRE(host != nullptr);
        cwTrip* native = addNativeTripWithShot(host, QStringLiteral("Dig"),
                                               QStringLiteral("d1"), QStringLiteral("d2"));
        // Tied to the host's own survey, so the solve places it.
        REQUIRE(host->tripCount() > 0);
        const cwTrip* derived = host->trip(0);
        REQUIRE_FALSE(derived->externalStations().isEmpty());
        REQUIRE(fixture->project->cavingRegion()->tieStations(
            native->stationHandle(QStringLiteral("d1")),
            derived->stationHandle(derived->externalStations().first())));
        drainPipelines(fixture.get());
        REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());

        // The native trip's stations are in the root's view, so the prefix
        // rule has keys to claim.
        const QString prefix = cwStation::canonicalKey(native->scopePrefix());
        REQUIRE_FALSE(prefix.isEmpty());
        const QStringList rootStations = root->scopedNetwork().stations();
        REQUIRE(std::any_of(rootStations.cbegin(), rootStations.cend(),
                            [&prefix](const QString& station) { return station.startsWith(prefix); }));
        REQUIRE_FALSE(host->sourceMembers().scopePrefixes.isEmpty());

        for (const cwSurveyNode* node : derivedNodes(root)) {
            checkMemberViews(node);
        }
        CHECK(host->scopedNetwork().hasStation(prefix + QStringLiteral("d1")));
    }
}

TEST_CASE("A derived node's member view keeps a shot to a non-member as an edge, not a station",
          "[cwCavingRegion][MemberViews]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeSurvexTree(sourceDir.path(), kNativeTreeFiles, kShotsPerFile,
                                          kBinaryTreeChildren);
    auto fixture = makeProject(QStringLiteral("member-views-foreign-edge"));
    cwSurveyNode* root = attachSourceThroughManager(
        fixture.get(), fixture->project->cavingRegion()->rootNode(), entry);
    drainPipelines(fixture.get());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    cwCavingRegion* region = fixture->project->cavingRegion();

    // A derived node with a member station in the root's view, and a station
    // of the same root outside its members.
    const QStringList rootStations = root->scopedNetwork().stations();
    const cwSurveyNode* node = nullptr;
    QString memberRootKey;
    QString siblingRootKey;
    for (const cwSurveyNode* candidate : derivedNodes(root)) {
        const cwSurveyNode::SourceMembers members = candidate->sourceMembers();
        const auto member = std::find_if(rootStations.cbegin(), rootStations.cend(),
                                         [&members](const QString& station) { return members.contains(station); });
        const auto sibling = std::find_if(rootStations.cbegin(), rootStations.cend(),
                                          [&members](const QString& station) { return !members.contains(station); });
        if (member != rootStations.cend() && sibling != rootStations.cend()) {
            node = candidate;
            memberRootKey = *member;
            siblingRootKey = *sibling;
            break;
        }
    }
    REQUIRE(node != nullptr);

    const QString rootPrefix = cwStation::canonicalKey(root->scopePrefix());
    REQUIRE_FALSE(rootPrefix.isEmpty());
    const QString memberRegionKey = rootPrefix + memberRootKey;
    const QString siblingRegionKey = rootPrefix + siblingRootKey;
    REQUIRE(region->network().hasStation(memberRegionKey));
    REQUIRE(region->network().hasStation(siblingRegionKey));

    // An invented cave outside the root, and a sibling of the same root.
    const QString foreign = QStringLiteral("beta.b2");
    cwSurveyNetwork network = region->network();
    network.addShot(memberRegionKey, foreign);
    network.addShot(memberRegionKey, siblingRegionKey);
    region->setNetwork(network);

    const cwSurveyNode::SourceMembers members = node->sourceMembers();
    const QString cavernPath = cwStation::canonicalKey(node->cavernPath());
    const QString local = memberLocalKey(memberRootKey, cavernPath);
    INFO("node " << node->name().toStdString() << ", member " << local.toStdString()
                 << ", sibling " << siblingRootKey.toStdString());

    const cwSurveyNetwork view = node->scopedNetwork();
    CHECK_FALSE(view.hasStation(foreign));
    CHECK_FALSE(view.hasStation(siblingRootKey));
    const QStringList memberNeighbors = view.neighbors(local);
    CHECK(memberNeighbors.contains(foreign));
    CHECK(memberNeighbors.contains(siblingRootKey));
    for (const QString& station : view.stations()) {
        INFO("station " << station.toStdString());
        CHECK((members.contains(station) || members.contains(cavernPath + station)));
    }

    const cwSurveyNetwork rootAfter = root->scopedNetwork();
    CHECK_FALSE(rootAfter.hasStation(foreign));
    CHECK(rootAfter.neighbors(memberRootKey).contains(foreign));
    CHECK(rootAfter.hasStation(siblingRootKey));
}

TEST_CASE("A member-view fan-out grows with the stations, not with nodes times stations",
          "[cwCavingRegion][MemberViews]")
{
    const FanOut small = measureGeneratedTree(kSmallTreeFiles);
    const FanOut large = measureGeneratedTree(kLargeTreeFiles);

    INFO("small: " << small.nodes << " nodes, " << small.stations << " stations, "
                   << small.stallMs << " ms stall");
    INFO("large: " << large.nodes << " nodes, " << large.stations << " stations, "
                   << large.stallMs << " ms stall");
    REQUIRE(small.stallMs > 0);
    const double growth = double(large.stallMs) / double(small.stallMs);
    INFO("growth " << growth);
    CHECK(growth <= kMaxStallGrowth);
}
