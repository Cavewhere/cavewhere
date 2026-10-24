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

//Qt includes
#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QSignalSpy>

//Std includes
#include <memory>

namespace {

constexpr int kSuppressedRole = static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole);
constexpr int kMessageRole = static_cast<int>(cwErrorListModel::ErrorRoles::MessageRole);
constexpr int kKindRole = static_cast<int>(cwErrorTreeModel::Roles::KindRole);
constexpr int kErrorModelRole = static_cast<int>(cwErrorTreeModel::Roles::ErrorModelRole);

const QString kDisagreementKind = QStringLiteral("Backsight disagreement");
const QString kMissingBacksightKind = QStringLiteral("Missing backsight");
const QString kMissingReadingKind = QStringLiteral("Missing reading");
const QString kMissingLrudKind = QStringLiteral("Missing LRUD");

cwError makeWarning(const QString& message, cwErrorTypeId id, bool suppressed = false)
{
    cwError error(message, cwError::Warning);
    error.setErrorTypeId(static_cast<int>(id));
    error.setSupressed(suppressed);
    return error;
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
    CHECK(tree.count() == 2);

    QSignalSpy countSpy(&tree, &cwErrorTreeModel::countChanged);

    //A cell model created after the root was set shows up, along with errors added later
    auto cell = std::make_unique<cwErrorModel>();
    cell->errors()->append(makeWarning(QStringLiteral("cell left"), cwErrorTypeId::SurveyMissingLeft));
    cell->setParentModel(chunk.get());
    cell->errors()->append(makeWarning(QStringLiteral("cell right"), cwErrorTypeId::SurveyMissingRight));

    CHECK(messages(tree) == QStringList({QStringLiteral("cell left"), QStringLiteral("cell right"),
                                         QStringLiteral("chunk"), QStringLiteral("root")}));
    CHECK(tree.count() == 4);
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
        cell->setParentModel(nullptr);
        CHECK(messages(tree) == QStringList({QStringLiteral("chunk"), QStringLiteral("root")}));
        CHECK(tree.count() == 2);
    }

    SECTION("a deleted chunk takes its cells with it") {
        //The chunk owns its cell models, the way cwSurveyChunk parents them
        cell.release()->setParent(chunk.get());
        chunk.reset();
        CHECK(messages(tree) == QStringList({QStringLiteral("root")}));
        CHECK(tree.count() == 1);
    }

    SECTION("a destroyed root empties the list") {
        auto ownedRoot = std::make_unique<cwErrorModel>();
        auto ownedChild = std::make_unique<cwErrorModel>();
        ownedChild->setParentModel(ownedRoot.get());
        ownedChild->errors()->append(makeWarning(QStringLiteral("orphan"), cwErrorTypeId::None));

        tree.setRoot(ownedRoot.get());
        CHECK(tree.count() == 1);

        ownedRoot.reset();
        CHECK(tree.root() == nullptr);
        CHECK(tree.rowCount() == 0);
        CHECK(tree.count() == 0);

        //The surviving child is no longer a source
        ownedChild->errors()->append(makeWarning(QStringLiteral("late"), cwErrorTypeId::None));
        CHECK(tree.rowCount() == 0);
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
    CHECK(tree.count() == 4);

    tree.setIgnoredOnly(true);
    CHECK(messages(tree) == QStringList({QStringLiteral("compass"), QStringLiteral("left")}));
    CHECK(tree.count() == 2);

    tree.setKindFilter(kMissingLrudKind);
    CHECK(messages(tree) == QStringList({QStringLiteral("left")}));
    CHECK(tree.index(0, 0).data(kKindRole).toString() == kMissingLrudKind);

    SECTION("ignoring a warning in the source brings it in") {
        cwErrorListModel* list = cell.errors();
        list->setData(list->index(1), true, kSuppressedRole);
        CHECK(messages(tree) == QStringList({QStringLiteral("left"), QStringLiteral("up")}));
        CHECK(tree.count() == 2);
    }

    SECTION("showing through the proxy reaches the source and drops the row") {
        QSignalSpy countSpy(&tree, &cwErrorTreeModel::countChanged);
        CHECK(tree.setData(tree.index(0, 0), false, kSuppressedRole));
        CHECK_FALSE(cell.errors()->at(0).suppressed());
        CHECK(tree.rowCount() == 0);
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
    CHECK(tree.kinds().isEmpty());

    QSignalSpy kindsSpy(&tree, &cwErrorTreeModel::kindsChanged);

    cell.errors()->append(makeWarning(QStringLiteral("left"), cwErrorTypeId::SurveyMissingLeft, true));
    cell.errors()->append(makeWarning(QStringLiteral("right"), cwErrorTypeId::SurveyMissingRight, true));
    CHECK(tree.kinds() == QStringList({kMissingLrudKind}));
    CHECK(kindsSpy.size() >= 1);

    cell.errors()->append(makeWarning(QStringLiteral("clino"), cwErrorTypeId::SurveyMissingClino, true));
    cell.errors()->append(makeWarning(QStringLiteral("back"), cwErrorTypeId::SurveyMissingBackCompass, true));
    cell.errors()->append(makeWarning(QStringLiteral("disagree"), cwErrorTypeId::SurveyClinoDisagreement, true));
    //Warnings that still show are not part of the ignored kinds
    cell.errors()->append(makeWarning(QStringLiteral("open"), cwErrorTypeId::SurveyCompassDisagreement, false));

    CHECK(tree.kinds() == QStringList({kDisagreementKind,
                                       kMissingBacksightKind,
                                       kMissingReadingKind,
                                       kMissingLrudKind}));

    kindsSpy.clear();
    cwErrorListModel* list = cell.errors();
    list->setData(list->index(4), false, kSuppressedRole);
    CHECK(tree.kinds() == QStringList({kMissingBacksightKind, kMissingReadingKind, kMissingLrudKind}));
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
    CHECK(tree.count() == 0);

    CHECK(trip->ignoreAllWarnings() == 5);
    CHECK(tree.count() == 5);
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
    CHECK(tree.count() == 4);

    //Showing one group through the trip leaves the other
    CHECK(trip->clearIgnoredWarnings(cwErrorTreeModel::typeIdsForKind(kMissingLrudKind)) == 3);
    CHECK(tree.count() == 1);
    CHECK(tree.kinds() == QStringList({kDisagreementKind}));

    //Removing the chunk empties the list
    trip->removeChunk(chunk);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(tree.count() == 0);
    CHECK(tree.kinds().isEmpty());
}
