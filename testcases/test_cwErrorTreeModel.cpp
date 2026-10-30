/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwErrorTreeModel.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "TestHelper.h"

//Qt includes
#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>

//Std includes
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

namespace {

constexpr int kSuppressedRole = static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole);
constexpr int kMessageRole = static_cast<int>(cwErrorListModel::ErrorRoles::MessageRole);
constexpr int kKindRole = static_cast<int>(cwErrorTreeModel::Roles::KindRole);
constexpr int kErrorModelRole = static_cast<int>(cwErrorTreeModel::Roles::ErrorModelRole);

const QString kDisagreementKind = QStringLiteral("Backsight disagreement");
const QString kMissingBacksightKind = QStringLiteral("Missing backsight");
const QString kMissingReadingKind = QStringLiteral("Missing reading");
const QString kMissingLrudKind = QStringLiteral("Missing LRUD");

//A trip's chunk holds about a hundred cell models with warnings
constexpr int kCellsPerChunk = 100;
constexpr int kSmallLeafCount = 500;
constexpr int kGrowthFactor = 4;
//Linear growth is kGrowthFactor; the rest is room for a loaded machine
constexpr double kMaxGrowthRatio = 6.0;
constexpr int kTimingRepeats = 3;
constexpr int kBurstCellCount = 10;

cwError makeWarning(const QString& message, cwErrorTypeId id, bool suppressed = false)
{
    cwError error(message, cwError::Warning);
    error.setErrorTypeId(static_cast<int>(id));
    error.setSupressed(suppressed);
    return error;
}

//! A trip-shaped root -> chunk -> cell tree with one warning per cell. The
//! vectors free cells before chunks and chunks before the root.
struct SyntheticTree
{
    cwErrorModel root;
    std::vector<std::unique_ptr<cwErrorModel>> chunks;
    std::vector<std::unique_ptr<cwErrorModel>> cells;
};

//! Builds chunks under the root, enough to hold \a cellCount cells at
//! kCellsPerChunk, and \a cellCount warning cells that wait for attachCells
std::unique_ptr<SyntheticTree> makeTree(int cellCount)
{
    auto tree = std::make_unique<SyntheticTree>();
    const int chunkCount = (cellCount + kCellsPerChunk - 1) / kCellsPerChunk;
    for(int i = 0; i < chunkCount; ++i) {
        auto chunk = std::make_unique<cwErrorModel>();
        chunk->setParentModel(&tree->root);
        tree->chunks.push_back(std::move(chunk));
    }

    tree->cells.reserve(static_cast<size_t>(cellCount));
    for(int i = 0; i < cellCount; ++i) {
        auto cell = std::make_unique<cwErrorModel>();
        cell->errors()->append(makeWarning(QStringLiteral("cell %1").arg(i),
                                           cwErrorTypeId::SurveyMissingLeft));
        tree->cells.push_back(std::move(cell));
    }
    return tree;
}

//! Puts every cell under the tree's chunks
void attachCells(SyntheticTree* tree)
{
    for(size_t i = 0; i < tree->cells.size(); ++i) {
        tree->cells.at(i)->setParentModel(
            tree->chunks.at(i / static_cast<size_t>(kCellsPerChunk)).get());
    }
}

//! Fastest of kTimingRepeats runs of setRoot over \a leafCount cells, until
//! the count has settled
qint64 setRootNanoseconds(int leafCount)
{
    qint64 fastest = std::numeric_limits<qint64>::max();
    for(int repeat = 0; repeat < kTimingRepeats; ++repeat) {
        auto source = makeTree(leafCount);
        attachCells(source.get());

        cwErrorTreeModel tree;
        QElapsedTimer timer;
        timer.start();
        tree.setRoot(&source->root);
        REQUIRE(waitUntil([&tree, leafCount]() { return tree.count() == leafCount; }));
        fastest = std::min(fastest, timer.nsecsElapsed());
    }
    return fastest;
}

//! Fastest of kTimingRepeats bursts of \a leafCount cells joining a tree the
//! model already tracks, until the count has settled
qint64 liveBurstNanoseconds(int leafCount)
{
    qint64 fastest = std::numeric_limits<qint64>::max();
    for(int repeat = 0; repeat < kTimingRepeats; ++repeat) {
        auto source = makeTree(leafCount);

        cwErrorTreeModel tree;
        tree.setRoot(&source->root);
        REQUIRE(waitForQueuedEvents());

        QElapsedTimer timer;
        timer.start();
        attachCells(source.get());
        REQUIRE(waitUntil([&tree, leafCount]() { return tree.count() == leafCount; }));
        fastest = std::min(fastest, timer.nsecsElapsed());
    }
    return fastest;
}

bool ownsMessage(const cwErrorModel* owner, const QString& message)
{
    const QList<cwError> errors = owner->errors()->toList();
    return std::any_of(errors.cbegin(), errors.cend(), [&message](const cwError& error) {
        return error.message() == message;
    });
}

QStringList messages(const QAbstractItemModel& model)
{
    QStringList found;
    for(int row = 0; row < model.rowCount(); ++row) {
        found.append(model.index(row, 0).data(kMessageRole).toString());
    }
    found.sort();
    return found;
}

}

TEST_CASE("cwErrorModel announces children as they join and leave", "[ErrorModelTest]")
{
    cwErrorModel parent;
    auto child = std::make_unique<cwErrorModel>();

    QSignalSpy addedSpy(&parent, &cwErrorModel::childModelAdded);
    QSignalSpy removedSpy(&parent, &cwErrorModel::childModelRemoved);

    child->setParentModel(&parent);
    REQUIRE(addedSpy.size() == 1);
    CHECK(addedSpy.at(0).at(0).value<cwErrorModel*>() == child.get());

    child->setParentModel(nullptr);
    REQUIRE(removedSpy.size() == 1);
    CHECK(removedSpy.at(0).at(0).value<cwErrorModel*>() == child.get());

    child->setParentModel(&parent);
    child.reset();
    CHECK(removedSpy.size() == 2);
}

TEST_CASE("cwErrorTreeModel flattens the tree and tracks child models live", "[ErrorModelTest][cwErrorTreeModel]")
{
    cwErrorModel root;
    auto chunk = std::make_unique<cwErrorModel>();
    chunk->setParentModel(&root);

    root.errors()->append(makeWarning(QStringLiteral("root"), cwErrorTypeId::None));
    chunk->errors()->append(makeWarning(QStringLiteral("chunk"), cwErrorTypeId::None));

    cwErrorTreeModel tree;
    QAbstractItemModelTester tester(&tree, QAbstractItemModelTester::FailureReportingMode::Fatal);
    tree.setRoot(&root);

    CHECK(messages(tree) == QStringList({QStringLiteral("chunk"), QStringLiteral("root")}));
    REQUIRE(waitUntil([&tree]() { return tree.count() == 2; }));

    QSignalSpy countSpy(&tree, &cwErrorTreeModel::countChanged);

    //A cell model created after the root was set shows up, along with errors
    //added later. Adds arrive in a queued pass.
    auto cell = std::make_unique<cwErrorModel>();
    cell->errors()->append(makeWarning(QStringLiteral("cell left"), cwErrorTypeId::SurveyMissingLeft));
    cell->setParentModel(chunk.get());
    cell->errors()->append(makeWarning(QStringLiteral("cell right"), cwErrorTypeId::SurveyMissingRight));

    const QStringList allMessages({QStringLiteral("cell left"), QStringLiteral("cell right"),
                                   QStringLiteral("chunk"), QStringLiteral("root")});
    REQUIRE(waitUntil([&tree, &allMessages]() { return messages(tree) == allMessages; }));
    REQUIRE(waitUntil([&tree]() { return tree.count() == 4; }));
    CHECK(countSpy.size() >= 1);

    //Every row names the leaf model it came from
    for(int row = 0; row < tree.rowCount(); ++row) {
        const QModelIndex index = tree.index(row, 0);
        const auto owner = index.data(kErrorModelRole).value<cwErrorModel*>();
        const QString message = index.data(kMessageRole).toString();
        if(message.startsWith(QStringLiteral("cell"))) {
            CHECK(owner == cell.get());
        } else if(message == QStringLiteral("chunk")) {
            CHECK(owner == chunk.get());
        } else {
            CHECK(owner == &root);
        }
    }

    SECTION("a detached cell model leaves the list") {
        //Removals are synchronous, so a view never reads a detached source
        cell->setParentModel(nullptr);
        CHECK(messages(tree) == QStringList({QStringLiteral("chunk"), QStringLiteral("root")}));
        CHECK(waitUntil([&tree]() { return tree.count() == 2; }));
    }

    SECTION("a deleted chunk takes its cells with it") {
        //The chunk owns its cell models, the way cwSurveyChunk parents them
        cell.release()->setParent(chunk.get());
        chunk.reset();
        CHECK(messages(tree) == QStringList({QStringLiteral("root")}));
        CHECK(waitUntil([&tree]() { return tree.count() == 1; }));
    }

    SECTION("a destroyed root empties the list") {
        auto ownedRoot = std::make_unique<cwErrorModel>();
        auto ownedChild = std::make_unique<cwErrorModel>();
        ownedChild->setParentModel(ownedRoot.get());
        ownedChild->errors()->append(makeWarning(QStringLiteral("orphan"), cwErrorTypeId::None));

        tree.setRoot(ownedRoot.get());
        REQUIRE(waitUntil([&tree]() { return tree.count() == 1; }));

        ownedRoot.reset();
        CHECK(tree.root() == nullptr);
        CHECK(tree.rowCount() == 0);
        CHECK(waitUntil([&tree]() { return tree.count() == 0; }));

        //The surviving child is no longer a source
        ownedChild->errors()->append(makeWarning(QStringLiteral("late"), cwErrorTypeId::None));
        CHECK(tree.rowCount() == 0);
        REQUIRE(waitForQueuedEvents());
        CHECK(tree.rowCount() == 0);
        CHECK(tree.count() == 0);
    }
}

TEST_CASE("cwErrorTreeModel filters to ignored warnings and to one kind", "[ErrorModelTest][cwErrorTreeModel]")
{
    cwErrorModel root;
    cwErrorModel cell;
    cell.setParentModel(&root);

    cell.errors()->append(makeWarning(QStringLiteral("left"), cwErrorTypeId::SurveyMissingLeft, true));
    cell.errors()->append(makeWarning(QStringLiteral("up"), cwErrorTypeId::SurveyMissingUp, false));
    cell.errors()->append(makeWarning(QStringLiteral("compass"), cwErrorTypeId::SurveyCompassDisagreement, true));
    cell.errors()->append(cwError(QStringLiteral("fatal"), cwError::Fatal));

    cwErrorTreeModel tree;
    QAbstractItemModelTester tester(&tree, QAbstractItemModelTester::FailureReportingMode::Fatal);
    tree.setRoot(&root);
    REQUIRE(waitUntil([&tree]() { return tree.count() == 4; }));

    tree.setIgnoredOnly(true);
    CHECK(messages(tree) == QStringList({QStringLiteral("compass"), QStringLiteral("left")}));
    REQUIRE(waitUntil([&tree]() { return tree.count() == 2; }));

    tree.setKindFilter(kMissingLrudKind);
    CHECK(messages(tree) == QStringList({QStringLiteral("left")}));
    CHECK(tree.index(0, 0).data(kKindRole).toString() == kMissingLrudKind);

    SECTION("ignoring a warning in the source brings it in") {
        cwErrorListModel* list = cell.errors();
        list->setData(list->index(1), true, kSuppressedRole);
        CHECK(messages(tree) == QStringList({QStringLiteral("left"), QStringLiteral("up")}));
        CHECK(waitUntil([&tree]() { return tree.count() == 2; }));
    }

    SECTION("showing through the proxy reaches the source and drops the row") {
        QSignalSpy countSpy(&tree, &cwErrorTreeModel::countChanged);
        CHECK(tree.setData(tree.index(0, 0), false, kSuppressedRole));
        CHECK_FALSE(cell.errors()->at(0).suppressed());
        CHECK(tree.rowCount() == 0);
        CHECK(waitUntil([&tree]() { return tree.count() == 0; }));
        CHECK(countSpy.size() >= 1);
    }

    SECTION("the kind label comes from the id's family") {
        tree.setKindFilter(QString());
        for(int row = 0; row < tree.rowCount(); ++row) {
            const QModelIndex index = tree.index(row, 0);
            const QString message = index.data(kMessageRole).toString();
            const QString expected = message == QStringLiteral("compass") ? kDisagreementKind : kMissingLrudKind;
            CHECK(index.data(kKindRole).toString() == expected);
        }
    }
}

TEST_CASE("cwErrorTreeModel lists the kinds present, rare kinds first, live", "[ErrorModelTest][cwErrorTreeModel]")
{
    cwErrorModel root;
    cwErrorModel cell;
    cell.setParentModel(&root);

    cwErrorTreeModel tree;
    tree.setIgnoredOnly(true);
    tree.setRoot(&root);
    REQUIRE(waitForQueuedEvents());
    CHECK(tree.kinds().isEmpty());

    QSignalSpy kindsSpy(&tree, &cwErrorTreeModel::kindsChanged);

    cell.errors()->append(makeWarning(QStringLiteral("left"), cwErrorTypeId::SurveyMissingLeft, true));
    cell.errors()->append(makeWarning(QStringLiteral("right"), cwErrorTypeId::SurveyMissingRight, true));
    REQUIRE(waitUntil([&tree]() { return tree.kinds() == QStringList({kMissingLrudKind}); }));
    CHECK(kindsSpy.size() >= 1);

    cell.errors()->append(makeWarning(QStringLiteral("clino"), cwErrorTypeId::SurveyMissingClino, true));
    cell.errors()->append(makeWarning(QStringLiteral("back"), cwErrorTypeId::SurveyMissingBackCompass, true));
    cell.errors()->append(makeWarning(QStringLiteral("disagree"), cwErrorTypeId::SurveyClinoDisagreement, true));
    //Warnings that still show are not part of the ignored kinds
    cell.errors()->append(makeWarning(QStringLiteral("open"), cwErrorTypeId::SurveyCompassDisagreement, false));

    const QStringList everyKind({kDisagreementKind,
                                 kMissingBacksightKind,
                                 kMissingReadingKind,
                                 kMissingLrudKind});
    REQUIRE(waitUntil([&tree, &everyKind]() { return tree.kinds() == everyKind; }));

    kindsSpy.clear();
    cwErrorListModel* list = cell.errors();
    list->setData(list->index(4), false, kSuppressedRole);
    const QStringList withoutDisagreement({kMissingBacksightKind, kMissingReadingKind, kMissingLrudKind});
    CHECK(waitUntil([&tree, &withoutDisagreement]() { return tree.kinds() == withoutDisagreement; }));
    CHECK(kindsSpy.size() >= 1);

    CHECK(cwErrorTreeModel::typeIdsForKind(kMissingLrudKind)
          == QList<int>({static_cast<int>(cwErrorTypeId::SurveyMissingLeft),
                         static_cast<int>(cwErrorTypeId::SurveyMissingRight),
                         static_cast<int>(cwErrorTypeId::SurveyMissingUp),
                         static_cast<int>(cwErrorTypeId::SurveyMissingDown)}));
    CHECK(cwErrorTreeModel::typeIdsForKind(kDisagreementKind)
          == QList<int>({static_cast<int>(cwErrorTypeId::SurveyCompassDisagreement),
                         static_cast<int>(cwErrorTypeId::SurveyClinoDisagreement)}));
}

TEST_CASE("cwErrorTreeModel follows a trip's chunk cells and finds their cells", "[ErrorModelTest][cwErrorTreeModel][ignoredWarnings]")
{
    auto trip = std::make_unique<cwTrip>();
    auto* chunk = new cwSurveyChunk();
    chunk->appendShot(cwStation(QStringLiteral("A1")),
                      cwStation(QStringLiteral("A2")),
                      cwShot(QStringLiteral("10"),
                             QStringLiteral("10"),
                             QStringLiteral("195"),
                             QStringLiteral("0"),
                             QStringLiteral("0")));
    trip->addChunk(chunk);

    //A2 gets its LRUDs, so A1's four plus the compass disagreement remain
    for(auto role : {cwSurveyChunk::StationLeftRole, cwSurveyChunk::StationRightRole,
                     cwSurveyChunk::StationUpRole, cwSurveyChunk::StationDownRole}) {
        chunk->setData(role, 1, QStringLiteral("1"));
    }

    cwErrorTreeModel tree;
    tree.setIgnoredOnly(true);
    tree.setRoot(trip->errorModel());
    REQUIRE(waitForQueuedEvents());
    CHECK(tree.count() == 0);

    CHECK(trip->ignoreAllWarnings() == 5);
    REQUIRE(waitUntil([&tree]() { return tree.count() == 5; }));
    CHECK(tree.kinds() == QStringList({kDisagreementKind, kMissingLrudKind}));

    //Each row's leaf model maps back to the cell that owns it
    for(int row = 0; row < tree.rowCount(); ++row) {
        const QModelIndex index = tree.index(row, 0);
        auto* owner = index.data(kErrorModelRole).value<cwErrorModel*>();
        const cwSurveyChunkCell cell = chunk->cellIndexForErrorModel(owner);
        REQUIRE(cell.isValid());
        CHECK(chunk->errorsAt(cell.index(), cell.role()) == owner);
    }
    CHECK_FALSE(chunk->cellIndexForErrorModel(trip->errorModel()).isValid());
    CHECK_FALSE(chunk->cellIndexForErrorModel(nullptr).isValid());

    //Filling a box deletes its cell model; the row goes with it
    chunk->setData(cwSurveyChunk::StationLeftRole, 0, QStringLiteral("1"));
    CHECK(tree.rowCount() == 4);
    CHECK(waitUntil([&tree]() { return tree.count() == 4; }));

    //Showing one group through the trip leaves the other
    CHECK(trip->clearIgnoredWarnings(cwErrorTreeModel::typeIdsForKind(kMissingLrudKind)) == 3);
    REQUIRE(waitUntil([&tree]() { return tree.count() == 1; }));
    CHECK(tree.kinds() == QStringList({kDisagreementKind}));

    //Removing the chunk empties the list
    trip->removeChunk(chunk);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(tree.rowCount() == 0);
    REQUIRE(waitUntil([&tree]() { return tree.count() == 0; }));
    CHECK(tree.kinds().isEmpty());
}

TEST_CASE("cwErrorTreeModel groups the empty-shot warning under missing readings", "[ErrorModelTest][cwErrorTreeModel]")
{
    const int emptyShot = static_cast<int>(cwErrorTypeId::SurveyEmptyShot);
    CHECK(cwErrorTreeModel::kindForTypeId(emptyShot) == kMissingReadingKind);
    CHECK(cwErrorTreeModel::typeIdsForKind(kMissingReadingKind).contains(emptyShot));
}

TEST_CASE("cwErrorTreeModel sets a root of many cell models in time linear in the cells",
          "[ErrorModelTest][cwErrorTreeModel][errorTreeGrowth]")
{
    const int largeLeafCount = kSmallLeafCount * kGrowthFactor;
    const qint64 small = setRootNanoseconds(kSmallLeafCount);
    const qint64 large = setRootNanoseconds(largeLeafCount);
    INFO("setRoot: " << small << " ns at " << kSmallLeafCount << " cells, "
         << large << " ns at " << largeLeafCount);
    REQUIRE(small > 0);
    CHECK(static_cast<double>(large) / static_cast<double>(small) <= kMaxGrowthRatio);
}

TEST_CASE("cwErrorTreeModel takes in a burst of new cell models in time linear in the burst",
          "[ErrorModelTest][cwErrorTreeModel][errorTreeGrowth]")
{
    const int largeLeafCount = kSmallLeafCount * kGrowthFactor;
    const qint64 small = liveBurstNanoseconds(kSmallLeafCount);
    const qint64 large = liveBurstNanoseconds(largeLeafCount);
    INFO("live burst: " << small << " ns at " << kSmallLeafCount << " cells, "
         << large << " ns at " << largeLeafCount);
    REQUIRE(small > 0);
    CHECK(static_cast<double>(large) / static_cast<double>(small) <= kMaxGrowthRatio);
}

TEST_CASE("cwErrorTreeModel takes in a burst of new cell models in one insert and one count change",
          "[ErrorModelTest][cwErrorTreeModel]")
{
    auto source = makeTree(kBurstCellCount);

    cwErrorTreeModel tree;
    QAbstractItemModelTester tester(&tree, QAbstractItemModelTester::FailureReportingMode::Fatal);
    tree.setRoot(&source->root);
    REQUIRE(waitForQueuedEvents());
    REQUIRE(tree.count() == 0);

    QSignalSpy rowsInsertedSpy(&tree, &QAbstractItemModel::rowsInserted);
    QSignalSpy countSpy(&tree, &cwErrorTreeModel::countChanged);

    attachCells(source.get());
    REQUIRE(waitUntil([&tree]() { return tree.count() == kBurstCellCount; }));

    CHECK(tree.rowCount() == kBurstCellCount);
    CHECK(rowsInsertedSpy.size() == 1);
    CHECK(countSpy.size() == 1);

    //Each row still names its own cell
    for(int row = 0; row < tree.rowCount(); ++row) {
        const QModelIndex index = tree.index(row, 0);
        const auto owner = index.data(kErrorModelRole).value<cwErrorModel*>();
        REQUIRE(owner != nullptr);
        CHECK(ownsMessage(owner, index.data(kMessageRole).toString()));
    }
}

TEST_CASE("cwErrorTreeModel updates the count once for a burst of ignored warnings",
          "[ErrorModelTest][cwErrorTreeModel]")
{
    auto source = makeTree(kBurstCellCount);
    attachCells(source.get());

    cwErrorTreeModel tree;
    QAbstractItemModelTester tester(&tree, QAbstractItemModelTester::FailureReportingMode::Fatal);
    tree.setIgnoredOnly(true);
    tree.setRoot(&source->root);
    REQUIRE(waitForQueuedEvents());
    REQUIRE(tree.count() == 0);

    QSignalSpy countSpy(&tree, &cwErrorTreeModel::countChanged);
    QSignalSpy kindsSpy(&tree, &cwErrorTreeModel::kindsChanged);

    for(const auto& cell : source->cells) {
        cwErrorListModel* list = cell->errors();
        list->setData(list->index(0), true, kSuppressedRole);
    }
    CHECK(tree.rowCount() == kBurstCellCount);
    REQUIRE(waitUntil([&tree]() { return tree.count() == kBurstCellCount; }));

    CHECK(countSpy.size() == 1);
    CHECK(kindsSpy.size() == 1);
    CHECK(tree.kinds() == QStringList({kMissingLrudKind}));
}

TEST_CASE("cwErrorTreeModel drops a cell model that leaves before its queued add",
          "[ErrorModelTest][cwErrorTreeModel]")
{
    //Two chunks, so one survives when the other is destroyed
    auto source = makeTree(kCellsPerChunk + 1);

    cwErrorTreeModel tree;
    QAbstractItemModelTester tester(&tree, QAbstractItemModelTester::FailureReportingMode::Fatal);
    tree.setRoot(&source->root);
    REQUIRE(waitForQueuedEvents());

    QSignalSpy rowsInsertedSpy(&tree, &QAbstractItemModel::rowsInserted);
    cwErrorModel* chunk = source->chunks.front().get();

    SECTION("detached in the same turn") {
        cwErrorModel* cell = source->cells.front().get();
        cell->setParentModel(chunk);
        cell->setParentModel(nullptr);
    }

    SECTION("destroyed in the same turn") {
        source->cells.front()->setParentModel(chunk);
        source->cells.front().reset();
    }

    SECTION("its parent destroyed in the same turn") {
        //The chunk owns its cell, the way cwSurveyChunk parents them
        cwErrorModel* cell = source->cells.front().release();
        cell->setParent(chunk);
        cell->setParentModel(chunk);
        source->chunks.front().reset();
    }

    REQUIRE(waitForQueuedEvents());
    CHECK(rowsInsertedSpy.isEmpty());
    CHECK(tree.rowCount() == 0);
    CHECK(tree.count() == 0);

    //Later cells still arrive
    source->cells.back()->setParentModel(source->chunks.back().get());
    CHECK(waitUntil([&tree]() { return tree.count() == 1; }));
    CHECK(tree.rowCount() == 1);
}

TEST_CASE("cwErrorTreeModel shows rows added to and removed from a tracked cell in place",
          "[ErrorModelTest][cwErrorTreeModel]")
{
    constexpr int cellCount = 3;
    auto source = makeTree(cellCount);
    attachCells(source.get());
    cwErrorModel* middle = source->cells.at(1).get();

    cwErrorTreeModel tree;
    QAbstractItemModelTester tester(&tree, QAbstractItemModelTester::FailureReportingMode::Fatal);
    tree.setRoot(&source->root);
    CHECK(tree.rowCount() == cellCount);

    //A tracked cell's own new rows show at once, owned by that cell
    middle->errors()->append(makeWarning(QStringLiteral("middle extra"), cwErrorTypeId::SurveyMissingRight));
    CHECK(tree.rowCount() == cellCount + 1);
    CHECK(messages(tree) == QStringList({QStringLiteral("cell 0"), QStringLiteral("cell 1"),
                                         QStringLiteral("cell 2"), QStringLiteral("middle extra")}));
    for(int row = 0; row < tree.rowCount(); ++row) {
        const QModelIndex index = tree.index(row, 0);
        const QString message = index.data(kMessageRole).toString();
        const auto owner = index.data(kErrorModelRole).value<cwErrorModel*>();
        REQUIRE(owner != nullptr);
        CHECK(ownsMessage(owner, message));
    }
    //In place: right after the middle cell's first row, ahead of the last cell
    constexpr int middleExtraRow = 2;
    const QModelIndex middleExtra = tree.index(middleExtraRow, 0);
    CHECK(middleExtra.data(kMessageRole).toString() == QStringLiteral("middle extra"));
    CHECK(middleExtra.data(kErrorModelRole).value<cwErrorModel*>() == middle);
    CHECK(waitUntil([&tree]() { return tree.count() == cellCount + 1; }));

    //Rows after the middle cell still reach their own cell
    SECTION("ignoring every row through the tree reaches each owning cell") {
        for(int row = 0; row < tree.rowCount(); ++row) {
            CHECK(tree.setData(tree.index(row, 0), true, kSuppressedRole));
        }
        for(const auto& cell : source->cells) {
            for(int row = 0; row < cell->errors()->size(); ++row) {
                CHECK(cell->errors()->at(row).suppressed());
            }
        }
    }

    SECTION("a removed row leaves at once") {
        middle->errors()->remove(0);
        CHECK(messages(tree) == QStringList({QStringLiteral("cell 0"), QStringLiteral("cell 2"),
                                             QStringLiteral("middle extra")}));
        CHECK(waitUntil([&tree]() { return tree.count() == cellCount; }));
    }
}
