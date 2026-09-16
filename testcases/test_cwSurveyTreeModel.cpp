/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QAbstractItemModelTester>
#include <QDateTime>
#include <QSignalSpy>
#include <QUndoStack>
#include <QUuid>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwLength.h"
#include "cwSurveyNode.h"
#include "cwSurveyTreeFilterModel.h"
#include "cwSurveyChunk.h"
#include "cwSurveyTreeModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwUnits.h"

namespace {

    //! A node with a name and a kind, parented nowhere yet
    cwCave* makeNode(const QString& name, cwSurveyNode::Kind kind)
    {
        auto node = new cwCave();
        node->setName(name);
        node->setKind(kind);
        return node;
    }

    //! A trip with a name and a date, parented nowhere yet
    cwTrip* makeTrip(const QString& name, const QDate& date)
    {
        auto trip = new cwTrip();
        trip->setName(name);
        trip->setDate(QDateTime(date, QTime()));
        return trip;
    }

    //! True when \a spy saw a dataChanged that covers \a column of \a rowIndex's
    //! row carrying \a role. A change announced with no roles covers every role.
    bool sawChange(const QSignalSpy& spy, const QModelIndex& rowIndex, int column, int role)
    {
        for(const QList<QVariant>& arguments : spy) {
            const QModelIndex topLeft = arguments.at(0).value<QModelIndex>();
            const QModelIndex bottomRight = arguments.at(1).value<QModelIndex>();
            const QList<int> roles = arguments.at(2).value<QList<int>>();

            if(topLeft.parent() == rowIndex.parent()
                    && topLeft.row() == rowIndex.row()
                    && topLeft.column() <= column
                    && bottomRight.column() >= column
                    && (roles.isEmpty() || roles.contains(role))) {
                return true;
            }
        }

        return false;
    }

    const QDate kTopo1Date(2020, 1, 1);
    const QDate kTopo2Date(2020, 2, 2);
    const QDate kTopo3Date(2020, 3, 3);
    const QDate kSurveyBDate(2021, 5, 5);

    constexpr double kAlphaLength = 123.5;
    constexpr double kAlphaDepth = 42.0;

    constexpr double kShotDistance = 10.0;
    //The bearing and the inclination only have to be readable; the assertions
    //ride on the distance and the station names.
    constexpr double kShotCompass = 15.0;
    constexpr double kShotClino = 0.0;
    constexpr double kDeclination = 3.5;

    //! Gives \a trip one shot from A1 to A2, so its length task and its
    //! used-station task each have something to report.
    void addShot(cwTrip* trip)
    {
        trip->addNewChunk();
        cwSurveyChunk* chunk = trip->chunk(trip->chunkCount() - 1);
        chunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("A1"));
        chunk->setData(cwSurveyChunk::StationNameRole, 1, QStringLiteral("A2"));
        chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, kShotDistance);
        chunk->setData(cwSurveyChunk::ShotCompassRole, 0, kShotCompass);
        chunk->setData(cwSurveyChunk::ShotClinoRole, 0, kShotClino);
    }

    //! Runs the event loop until \a predicate holds or \a spy stops hearing
    //! anything, so a test waits on the tasks' own signals instead of a sleep.
    template<typename Predicate>
    bool waitFor(QSignalSpy& spy, Predicate predicate)
    {
        constexpr int kWaitMilliseconds = 2000;
        while(!predicate() && spy.wait(kWaitMilliseconds)) {}
        return predicate();
    }
}

TEST_CASE("cwSurveyTreeModel is a tree of nodes and trips", "[SurveyTreeModel]") {

    // region
    //  └── Alpha Cave                 (Cave)
    //       ├── Upper Level           (Folder)
    //       │    ├── Crawl Section    (Cave)
    //       │    │    └── Topo 3
    //       │    └── Topo 2
    //       └── Topo 1
    //  └── Beta Cave                  (Cave)
    //       └── Survey B
    QUndoStack undoStack;
    cwCavingRegion region;
    region.setUndoStack(&undoStack);

    cwCave* alpha = makeNode(QStringLiteral("Alpha Cave"), cwSurveyNode::Kind::Cave);
    alpha->length()->setValue(kAlphaLength);
    alpha->length()->setUnit(cwUnits::Meters);
    alpha->depth()->setValue(kAlphaDepth);
    alpha->depth()->setUnit(cwUnits::Meters);

    cwCave* upperLevel = makeNode(QStringLiteral("Upper Level"), cwSurveyNode::Kind::Folder);
    cwCave* crawlSection = makeNode(QStringLiteral("Crawl Section"), cwSurveyNode::Kind::Cave);

    cwTrip* topo1 = makeTrip(QStringLiteral("Topo 1"), kTopo1Date);
    cwTrip* topo2 = makeTrip(QStringLiteral("Topo 2"), kTopo2Date);
    cwTrip* topo3 = makeTrip(QStringLiteral("Topo 3"), kTopo3Date);

    crawlSection->addTrip(topo3);
    upperLevel->addNode(crawlSection);
    upperLevel->addTrip(topo2);
    alpha->addNode(upperLevel);
    alpha->addTrip(topo1);

    cwCave* beta = makeNode(QStringLiteral("Beta Cave"), cwSurveyNode::Kind::Cave);
    cwTrip* surveyB = makeTrip(QStringLiteral("Survey B"), kSurveyBDate);
    beta->addTrip(surveyB);

    region.addCave(alpha);
    region.addCave(beta);

    cwSurveyTreeModel model;
    model.setRegion(&region);

    //Validates index(), parent(), rowCount() and data() across every insert and
    //remove the sections below make
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);

    const QModelIndex alphaIndex = model.indexOf(alpha);
    const QModelIndex upperIndex = model.indexOf(upperLevel);
    const QModelIndex crawlIndex = model.indexOf(crawlSection);
    const QModelIndex betaIndex = model.indexOf(beta);

    SECTION("child-node rows come before trip rows at every level") {
        CHECK(model.columnCount() == cwSurveyTreeModel::Decl + 1);

        REQUIRE(model.rowCount() == 2);
        CHECK(model.index(0, cwSurveyTreeModel::Name) == alphaIndex);
        CHECK(model.index(1, cwSurveyTreeModel::Name) == betaIndex);
        CHECK(model.parent(alphaIndex) == QModelIndex());

        REQUIRE(model.rowCount(alphaIndex) == 2);
        CHECK(model.index(0, cwSurveyTreeModel::Name, alphaIndex) == upperIndex);
        CHECK(model.objectFor(model.index(1, cwSurveyTreeModel::Name, alphaIndex)) == topo1);
        CHECK(model.parent(upperIndex) == alphaIndex);

        REQUIRE(model.rowCount(upperIndex) == 2);
        CHECK(model.index(0, cwSurveyTreeModel::Name, upperIndex) == crawlIndex);
        CHECK(model.objectFor(model.index(1, cwSurveyTreeModel::Name, upperIndex)) == topo2);
        CHECK(model.parent(crawlIndex) == upperIndex);

        REQUIRE(model.rowCount(crawlIndex) == 1);
        const QModelIndex topo3Index = model.index(0, cwSurveyTreeModel::Name, crawlIndex);
        CHECK(model.objectFor(topo3Index) == topo3);
        CHECK(model.indexOf(topo3) == topo3Index);
        CHECK(model.rowCount(topo3Index) == 0);
        CHECK(model.parent(topo3Index) == crawlIndex);

        REQUIRE(model.rowCount(betaIndex) == 1);

        //The region's root owns no row of its own
        CHECK(model.indexOf(region.rootNode()) == QModelIndex());
        CHECK(model.objectFor(QModelIndex()) == nullptr);
    }

    SECTION("the Name column carries the display text and the header titles") {
        CHECK(model.data(alphaIndex, Qt::DisplayRole).toString() == QStringLiteral("Alpha Cave"));
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::NameRole).toString() == QStringLiteral("Alpha Cave"));

        const QModelIndex alphaKind = model.index(alphaIndex.row(), cwSurveyTreeModel::Kind);
        CHECK(model.data(alphaKind, Qt::DisplayRole).isValid() == false);

        CHECK(model.headerData(cwSurveyTreeModel::Name, Qt::Horizontal).toString() == QStringLiteral("Name"));
        CHECK(model.headerData(cwSurveyTreeModel::Kind, Qt::Horizontal).toString() == QStringLiteral("Kind"));
        CHECK(model.headerData(cwSurveyTreeModel::Trips, Qt::Horizontal).toString() == QStringLiteral("Trips"));
        CHECK(model.headerData(cwSurveyTreeModel::Stations, Qt::Horizontal).toString() == QStringLiteral("Stations"));
        CHECK(model.headerData(cwSurveyTreeModel::Length, Qt::Horizontal).toString() == QStringLiteral("Length"));
        CHECK(model.headerData(cwSurveyTreeModel::Depth, Qt::Horizontal).toString() == QStringLiteral("Depth"));
        CHECK(model.headerData(cwSurveyTreeModel::Date, Qt::Horizontal).toString() == QStringLiteral("Date"));
        CHECK(model.headerData(cwSurveyTreeModel::Decl, Qt::Horizontal).toString() == QStringLiteral("Decl"));

        //Every column the enum names has a title, and the enum names every
        //column the model has
        for(int column = cwSurveyTreeModel::Name; column <= cwSurveyTreeModel::Decl; column++) {
            INFO("Column: " << column);
            CHECK(model.headerData(column, Qt::Horizontal).toString().isEmpty() == false);
        }
        CHECK(model.headerData(cwSurveyTreeModel::Decl + 1, Qt::Horizontal).isValid() == false);
    }

    SECTION("a node row folds its subtree, a trip row stands for itself") {
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::RowTypeRole).toInt() == cwSurveyTreeModel::Node);
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::KindLabelRole).toString() == QStringLiteral("Cave"));
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::MutedRole).toBool() == false);
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::TripCountRole).toInt() == 3);

        //A node holds trips from many days, so its Date cell names none
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::DateRole).toDateTime().isValid() == false);
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::LengthRole).value<cwLength*>() == alpha->length());
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::DepthValueRole).value<cwLength*>() == alpha->depth());
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::ObjectRole).value<QObject*>() == alpha);
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::IsSourcedRole).toBool() == false);
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::IsSourceRootRole).toBool() == false);
        CHECK(model.isSourceRootIndex(alphaIndex) == false);

        //A Folder's stats are its children's, so its row reads muted
        CHECK(model.data(upperIndex, cwSurveyTreeModel::KindLabelRole).toString() == QStringLiteral("Folder"));
        CHECK(model.data(upperIndex, cwSurveyTreeModel::MutedRole).toBool());
        CHECK(model.data(upperIndex, cwSurveyTreeModel::TripCountRole).toInt() == 2);

        const QModelIndex topo3Index = model.indexOf(topo3);
        CHECK(model.data(topo3Index, cwSurveyTreeModel::RowTypeRole).toInt() == cwSurveyTreeModel::Trip);
        CHECK(model.data(topo3Index, cwSurveyTreeModel::NameRole).toString() == QStringLiteral("Topo 3"));
        CHECK(model.data(topo3Index, cwSurveyTreeModel::MutedRole).toBool());
        CHECK(model.data(topo3Index, cwSurveyTreeModel::TripCountRole).toInt() == 0);
        CHECK(model.data(topo3Index, cwSurveyTreeModel::KindLabelRole).toString().isEmpty());
        //A trip has no cwLength object of its own: its Length cell is the number
        //its length task adds up
        CHECK(model.data(topo3Index, cwSurveyTreeModel::LengthRole).value<cwLength*>() == nullptr);
        CHECK(model.data(topo3Index, cwSurveyTreeModel::LengthRole).toDouble() == 0.0);
        CHECK(model.data(topo3Index, cwSurveyTreeModel::DepthValueRole).value<cwLength*>() == nullptr);
        CHECK(model.data(topo3Index, cwSurveyTreeModel::DateRole).toDateTime()
              == QDateTime(kTopo3Date, QTime()));

        cwCave* empty = makeNode(QStringLiteral("Empty Cave"), cwSurveyNode::Kind::Cave);
        region.addCave(empty);
        const QModelIndex emptyIndex = model.indexOf(empty);
        REQUIRE(emptyIndex.isValid());
        CHECK(model.data(emptyIndex, cwSurveyTreeModel::DateRole).toDateTime().isValid() == false);
        CHECK(model.data(emptyIndex, cwSurveyTreeModel::TripCountRole).toInt() == 0);
        CHECK(model.data(emptyIndex, cwSurveyTreeModel::StationCountRole).toInt() == 0);
    }

    SECTION("every Kind has its own label") {
        CHECK(cwSurveyTreeModel::kindLabel(cwSurveyNode::Kind::Cave) == QStringLiteral("Cave"));
        CHECK(cwSurveyTreeModel::kindLabel(cwSurveyNode::Kind::Folder) == QStringLiteral("Folder"));
        CHECK(cwSurveyTreeModel::kindLabel(cwSurveyNode::Kind::CompassProject) == QStringLiteral("Compass project"));
        CHECK(cwSurveyTreeModel::kindLabel(cwSurveyNode::Kind::CompassFile) == QStringLiteral("Compass file"));
        CHECK(cwSurveyTreeModel::kindLabel(cwSurveyNode::Kind::WallsBook) == QStringLiteral("Walls book"));
        CHECK(cwSurveyTreeModel::kindLabel(cwSurveyNode::Kind::SurvexFile) == QStringLiteral("Survex file"));
        CHECK(cwSurveyTreeModel::kindLabel(cwSurveyNode::Kind::SurvexBlock) == QStringLiteral("Survex block"));
    }

    SECTION("a node inserted with a subtree announces one pair of rows") {
        cwCave* wing = makeNode(QStringLiteral("East Wing"), cwSurveyNode::Kind::Cave);
        cwCave* wingChild = makeNode(QStringLiteral("Sump"), cwSurveyNode::Kind::Cave);
        wingChild->addTrip(makeTrip(QStringLiteral("Topo 4"), QDate(2022, 6, 6)));
        wing->addNode(wingChild);
        wing->addTrip(makeTrip(QStringLiteral("Topo 5"), QDate(2022, 7, 7)));

        QSignalSpy aboutToInsert(&model, &QAbstractItemModel::rowsAboutToBeInserted);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

        upperLevel->addNode(wing);

        //One pair, on the parent index — the subtree rides in with its parent row
        REQUIRE(aboutToInsert.count() == 1);
        REQUIRE(inserted.count() == 1);
        CHECK(inserted.at(0).at(0).value<QModelIndex>() == upperIndex);
        CHECK(inserted.at(0).at(1).toInt() == 1);
        CHECK(inserted.at(0).at(2).toInt() == 1);

        //The new node sits after Crawl Section and ahead of Upper Level's trip
        REQUIRE(model.rowCount(upperIndex) == 3);
        const QModelIndex wingIndex = model.index(1, cwSurveyTreeModel::Name, upperIndex);
        CHECK(model.objectFor(wingIndex) == wing);
        CHECK(model.objectFor(model.index(2, cwSurveyTreeModel::Name, upperIndex)) == topo2);

        REQUIRE(model.rowCount(wingIndex) == 2);
        CHECK(model.objectFor(model.index(0, cwSurveyTreeModel::Name, wingIndex)) == wingChild);
        CHECK(model.rowCount(model.index(0, cwSurveyTreeModel::Name, wingIndex)) == 1);

        //The counts the new subtree brought reach every ancestor row
        CHECK(model.data(upperIndex, cwSurveyTreeModel::TripCountRole).toInt() == 4);
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::TripCountRole).toInt() == 5);
        CHECK(sawChange(dataChanged, upperIndex, cwSurveyTreeModel::Trips, cwSurveyTreeModel::TripCountRole));
        CHECK(sawChange(dataChanged, alphaIndex, cwSurveyTreeModel::Trips, cwSurveyTreeModel::TripCountRole));

        //A trip added to the new node is announced through the new connections
        QSignalSpy insertedAfter(&model, &QAbstractItemModel::rowsInserted);
        wingChild->addTrip(makeTrip(QStringLiteral("Topo 6"), QDate(2022, 8, 8)));
        CHECK(insertedAfter.count() == 1);
        CHECK(model.rowCount(model.index(0, cwSurveyTreeModel::Name, wingIndex)) == 2);
    }

    SECTION("removing a node takes its subtree out in one pair of rows") {
        QSignalSpy aboutToRemove(&model, &QAbstractItemModel::rowsAboutToBeRemoved);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

        alpha->removeNode(alpha->indexOfNode(upperLevel));

        REQUIRE(aboutToRemove.count() == 1);
        REQUIRE(removed.count() == 1);
        CHECK(removed.at(0).at(0).value<QModelIndex>() == alphaIndex);
        CHECK(removed.at(0).at(1).toInt() == 0);
        CHECK(removed.at(0).at(2).toInt() == 0);

        REQUIRE(model.rowCount(alphaIndex) == 1);
        CHECK(model.objectFor(model.index(0, cwSurveyTreeModel::Name, alphaIndex)) == topo1);
        CHECK(model.indexOf(upperLevel) == QModelIndex());
        CHECK(model.indexOf(topo2) == QModelIndex());
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::TripCountRole).toInt() == 1);

        //The removed subtree is unwired, so its edits move no row here
        QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);
        topo2->setName(QStringLiteral("Renamed while detached"));
        CHECK(dataChanged.count() == 0);
    }

    SECTION("a trip row is inserted after the child-node rows and removed again") {
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

        auto topo7 = makeTrip(QStringLiteral("Topo 7"), QDate(2023, 9, 9));
        alpha->addTrip(topo7);

        REQUIRE(inserted.count() == 1);
        CHECK(inserted.at(0).at(0).value<QModelIndex>() == alphaIndex);
        CHECK(inserted.at(0).at(1).toInt() == 2);
        REQUIRE(model.rowCount(alphaIndex) == 3);
        CHECK(model.objectFor(model.index(2, cwSurveyTreeModel::Name, alphaIndex)) == topo7);

        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        alpha->removeTrip(alpha->indexOf(topo7));

        REQUIRE(removed.count() == 1);
        CHECK(removed.at(0).at(1).toInt() == 2);
        CHECK(model.rowCount(alphaIndex) == 2);
    }

    SECTION("a trip row carries its own length, station count and declination") {
        const QModelIndex topo3Index = model.indexOf(topo3);
        QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

        addShot(topo3);

        //The length and the stations arrive from their tasks
        REQUIRE(waitFor(dataChanged, [&]() {
            return model.data(topo3Index, cwSurveyTreeModel::StationCountRole).toInt() == 2
                    && model.data(topo3Index, cwSurveyTreeModel::LengthRole).toDouble() > 0.0;
        }));

        CHECK(model.data(topo3Index, cwSurveyTreeModel::LengthRole).toDouble()
              == Catch::Approx(kShotDistance));
        CHECK(sawChange(dataChanged, topo3Index, cwSurveyTreeModel::Length, cwSurveyTreeModel::LengthRole));
        CHECK(sawChange(dataChanged, topo3Index, cwSurveyTreeModel::Stations, cwSurveyTreeModel::StationCountRole));

        //A node row sums the stations of its subtree, and every ancestor hears it
        CHECK(model.data(crawlIndex, cwSurveyTreeModel::StationCountRole).toInt() == 2);
        CHECK(model.data(upperIndex, cwSurveyTreeModel::StationCountRole).toInt() == 2);
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::StationCountRole).toInt() == 2);
        CHECK(model.data(betaIndex, cwSurveyTreeModel::StationCountRole).toInt() == 0);

        CHECK(sawChange(dataChanged, crawlIndex, cwSurveyTreeModel::Stations, cwSurveyTreeModel::StationCountRole));
        CHECK(sawChange(dataChanged, upperIndex, cwSurveyTreeModel::Stations, cwSurveyTreeModel::StationCountRole));
        CHECK(sawChange(dataChanged, alphaIndex, cwSurveyTreeModel::Stations, cwSurveyTreeModel::StationCountRole));
        CHECK(sawChange(dataChanged, betaIndex, cwSurveyTreeModel::Stations, cwSurveyTreeModel::StationCountRole) == false);

        //A node row keeps the cwLength object its own solve fills in
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::LengthRole).value<cwLength*>() == alpha->length());

        //A declination belongs to the trip's calibration. Auto is on by default
        //and resolves to nothing without a fix station, which is the "manual"
        //the cave page shows.
        dataChanged.clear();
        CHECK(model.data(topo3Index, cwSurveyTreeModel::AutoDeclinationRole).toBool() == false);

        topo3->calibrations()->setAutoDeclination(false);
        CHECK(sawChange(dataChanged, topo3Index, cwSurveyTreeModel::Decl, cwSurveyTreeModel::AutoDeclinationRole));

        dataChanged.clear();
        topo3->calibrations()->setDeclinationManual(kDeclination);
        CHECK(model.data(topo3Index, cwSurveyTreeModel::DeclinationRole).toDouble()
              == Catch::Approx(kDeclination));
        CHECK(sawChange(dataChanged, topo3Index, cwSurveyTreeModel::Decl, cwSurveyTreeModel::DeclinationRole));

        //A node row states zero and manual, so a delegate's typed cell always
        //has a value to show
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::DeclinationRole).toDouble()
              == Catch::Approx(0.0));
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::AutoDeclinationRole).toBool() == false);
    }

    SECTION("a rename moves the Name cell of the row that owns the name") {
        QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

        const QModelIndex topo3Index = model.indexOf(topo3);
        topo3->setName(QStringLiteral("Topo 3 redone"));

        CHECK(sawChange(dataChanged, topo3Index, cwSurveyTreeModel::Name, cwSurveyTreeModel::NameRole));
        CHECK(model.data(topo3Index, cwSurveyTreeModel::NameRole).toString()
              == QStringLiteral("Topo 3 redone"));

        dataChanged.clear();
        alpha->setName(QStringLiteral("Alpha Cave renamed"));
        CHECK(sawChange(dataChanged, alphaIndex, cwSurveyTreeModel::Name, cwSurveyTreeModel::NameRole));
    }

    SECTION("a trip date moves the trip's own Date cell and no other row") {
        QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

        const QModelIndex topo3Index = model.indexOf(topo3);
        const QDate newDate(2024, 4, 4);
        topo3->setDate(QDateTime(newDate, QTime()));

        CHECK(sawChange(dataChanged, topo3Index, cwSurveyTreeModel::Date, cwSurveyTreeModel::DateRole));
        CHECK(model.data(topo3Index, cwSurveyTreeModel::DateRole).toDateTime()
              == QDateTime(newDate, QTime()));

        //A date folds nowhere, so no ancestor row hears it
        CHECK(sawChange(dataChanged, crawlIndex, cwSurveyTreeModel::Date, cwSurveyTreeModel::DateRole) == false);
        CHECK(sawChange(dataChanged, upperIndex, cwSurveyTreeModel::Date, cwSurveyTreeModel::DateRole) == false);
        CHECK(sawChange(dataChanged, alphaIndex, cwSurveyTreeModel::Date, cwSurveyTreeModel::DateRole) == false);
        CHECK(model.data(alphaIndex, cwSurveyTreeModel::DateRole).toDateTime().isValid() == false);
    }

    SECTION("kind, source, length and depth each move their own cell") {
        QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

        beta->setKind(cwSurveyNode::Kind::Folder);
        CHECK(sawChange(dataChanged, betaIndex, cwSurveyTreeModel::Kind, cwSurveyTreeModel::KindLabelRole));
        CHECK(model.data(betaIndex, cwSurveyTreeModel::KindLabelRole).toString() == QStringLiteral("Folder"));
        CHECK(model.data(betaIndex, cwSurveyTreeModel::MutedRole).toBool());

        //A Folder draws its stat cells muted, so those cells hear the change too
        CHECK(sawChange(dataChanged, betaIndex, cwSurveyTreeModel::Trips, cwSurveyTreeModel::MutedRole));
        CHECK(sawChange(dataChanged, betaIndex, cwSurveyTreeModel::Stations, cwSurveyTreeModel::MutedRole));
        CHECK(sawChange(dataChanged, betaIndex, cwSurveyTreeModel::Length, cwSurveyTreeModel::MutedRole));
        CHECK(sawChange(dataChanged, betaIndex, cwSurveyTreeModel::Depth, cwSurveyTreeModel::MutedRole));

        dataChanged.clear();
        beta->setSourceId(QUuid::createUuid());
        CHECK(sawChange(dataChanged, betaIndex, cwSurveyTreeModel::Kind, cwSurveyTreeModel::IsSourceRootRole));
        CHECK(sawChange(dataChanged, betaIndex, cwSurveyTreeModel::Name, cwSurveyTreeModel::IsSourcedRole));

        CHECK(model.data(betaIndex, cwSurveyTreeModel::IsSourcedRole).toBool());
        CHECK(model.data(betaIndex, cwSurveyTreeModel::IsSourceRootRole).toBool());
        CHECK(model.isSourceRootIndex(betaIndex));

        //A node inside a source is not the row that owns the file
        dataChanged.clear();
        beta->setSourcePath(QStringLiteral("beta/level1"));
        CHECK(model.isSourceRootIndex(betaIndex) == false);

        dataChanged.clear();
        alpha->length()->setValue(kAlphaLength + 1.0);
        CHECK(sawChange(dataChanged, alphaIndex, cwSurveyTreeModel::Length, cwSurveyTreeModel::LengthRole));

        dataChanged.clear();
        alpha->depth()->setValue(kAlphaDepth + 1.0);
        CHECK(sawChange(dataChanged, alphaIndex, cwSurveyTreeModel::Depth, cwSurveyTreeModel::DepthValueRole));
    }

    SECTION("setRegion shows the second region's tree") {
        cwCavingRegion secondRegion;
        cwCave* onlyCave = makeNode(QStringLiteral("Second Cave"), cwSurveyNode::Kind::Cave);
        secondRegion.addCave(onlyCave);

        QSignalSpy aboutToReset(&model, &QAbstractItemModel::modelAboutToBeReset);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        QSignalSpy regionChanged(&model, &cwSurveyTreeModel::regionChanged);

        model.setRegion(&secondRegion);

        CHECK(aboutToReset.count() == 1);
        CHECK(reset.count() == 1);
        CHECK(regionChanged.count() == 1);
        CHECK(model.region() == &secondRegion);

        REQUIRE(model.rowCount() == 1);
        CHECK(model.objectFor(model.index(0, cwSurveyTreeModel::Name)) == onlyCave);
        CHECK(model.indexOf(alpha) == QModelIndex());

        //The first region's tree is unwired, so its edits move nothing here
        QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        alpha->setName(QStringLiteral("Alpha renamed"));
        alpha->addTrip(makeTrip(QStringLiteral("Topo 8"), QDate(2025, 1, 1)));
        CHECK(dataChanged.count() == 0);
        CHECK(inserted.count() == 0);

        //And the second region's tree is live
        onlyCave->addTrip(makeTrip(QStringLiteral("Second Trip"), QDate(2025, 2, 2)));
        CHECK(inserted.count() == 1);
        CHECK(model.rowCount(model.index(0, cwSurveyTreeModel::Name)) == 1);
    }

    SECTION("a destroyed region empties the model") {
        auto ownRegion = new cwCavingRegion();
        ownRegion->addCave(makeNode(QStringLiteral("Doomed Cave"), cwSurveyNode::Kind::Cave));

        cwSurveyTreeModel ownModel;
        QAbstractItemModelTester ownTester(&ownModel, QAbstractItemModelTester::FailureReportingMode::Fatal);
        ownModel.setRegion(ownRegion);
        REQUIRE(ownModel.rowCount() == 1);

        QSignalSpy reset(&ownModel, &QAbstractItemModel::modelReset);
        delete ownRegion;

        CHECK(reset.count() == 1);
        CHECK(ownModel.region() == nullptr);
        CHECK(ownModel.rowCount() == 0);
        CHECK(ownModel.index(0, cwSurveyTreeModel::Name) == QModelIndex());
    }

    SECTION("a model with no region holds no row") {
        cwSurveyTreeModel empty;
        QAbstractItemModelTester emptyTester(&empty, QAbstractItemModelTester::FailureReportingMode::Fatal);

        CHECK(empty.region() == nullptr);
        CHECK(empty.rowCount() == 0);
        CHECK(empty.columnCount() == cwSurveyTreeModel::Decl + 1);
        CHECK(empty.indexOf(alpha) == QModelIndex());
    }

    SECTION("the filter keeps a match's ancestors and drops everything else") {
        cwSurveyTreeFilterModel filter;
        filter.setSourceModel(&model);

        CHECK(filter.isRecursiveFilteringEnabled());
        CHECK(filter.filterCaseSensitivity() == Qt::CaseInsensitive);
        CHECK(filter.filterRole() == cwSurveyTreeModel::NameRole);
        CHECK(filter.sortColumn() == -1);

        //Everything is visible until a filter is typed
        CHECK(filter.rowCount() == 2);

        filter.setFilterText(QStringLiteral("topo"));
        CHECK(filter.filterText() == QStringLiteral("topo"));

        //Beta Cave holds no Topo, so its branch goes; Alpha's stays because
        //three of its descendants match
        REQUIRE(filter.rowCount() == 1);
        const QModelIndex alphaProxy = filter.mapFromSource(alphaIndex);
        REQUIRE(alphaProxy.isValid());
        CHECK(filter.index(0, cwSurveyTreeModel::Name) == alphaProxy);

        //Order is the source's: the child node ahead of the trip
        REQUIRE(filter.rowCount(alphaProxy) == 2);
        CHECK(filter.mapToSource(filter.index(0, cwSurveyTreeModel::Name, alphaProxy)) == upperIndex);
        CHECK(filter.data(filter.index(1, cwSurveyTreeModel::Name, alphaProxy),
                          cwSurveyTreeModel::NameRole).toString() == QStringLiteral("Topo 1"));

        const QModelIndex upperProxy = filter.mapFromSource(upperIndex);
        REQUIRE(filter.rowCount(upperProxy) == 2);
        const QModelIndex crawlProxy = filter.mapFromSource(crawlIndex);
        REQUIRE(filter.rowCount(crawlProxy) == 1);

        //Clearing the filter brings every row back
        filter.setFilterText(QString());
        CHECK(filter.rowCount() == 2);
        CHECK(filter.rowCount(filter.mapFromSource(alphaIndex)) == 2);
        CHECK(filter.rowCount(filter.mapFromSource(betaIndex)) == 1);
    }

    SECTION("a matching node does not carry its non-matching children") {
        cwSurveyTreeFilterModel filter;
        filter.setSourceModel(&model);

        //autoAcceptChildRows stays false: a row is shown when it or a descendant
        //matches, so Alpha's children, which match nothing, stay hidden
        CHECK(filter.autoAcceptChildRows() == false);

        filter.setFilterText(QStringLiteral("ALPHA"));

        REQUIRE(filter.rowCount() == 1);
        const QModelIndex alphaProxy = filter.index(0, cwSurveyTreeModel::Name);
        CHECK(filter.mapToSource(alphaProxy) == alphaIndex);
        CHECK(filter.rowCount(alphaProxy) == 0);
    }

    SECTION("the filter follows a rename") {
        cwSurveyTreeFilterModel filter;
        filter.setSourceModel(&model);
        filter.setFilterText(QStringLiteral("Beta"));

        REQUIRE(filter.rowCount() == 1);
        CHECK(filter.mapToSource(filter.index(0, cwSurveyTreeModel::Name)) == betaIndex);

        beta->setName(QStringLiteral("Gamma Cave"));
        CHECK(filter.rowCount() == 0);
    }
}
