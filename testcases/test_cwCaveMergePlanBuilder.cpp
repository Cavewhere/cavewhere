// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwCave.h"
#include "cwCaveMergeApplier.h"
#include "cwCaveMergePlanBuilder.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"

TEST_CASE("cwCave merge plan builder maps loaded caves by stable id", "[cwCaveMerge][sync]")
{
    cwCave firstCave;
    cwCave secondCave;

    const cwCaveData firstLoaded = firstCave.data();
    const cwCaveData secondLoaded = secondCave.data();

    const auto preparation = cwCaveMergePlanBuilder::build({&firstCave, &secondCave},
                                                            {&secondLoaded, &firstLoaded},
                                                            {});
    REQUIRE_FALSE(preparation.hasError());
    REQUIRE(preparation.value().plans.size() == 2);
    CHECK(preparation.value().plans[0].currentCave == &secondCave);
    CHECK(preparation.value().plans[0].loadedCaveData == &secondLoaded);
    CHECK(preparation.value().plans[1].currentCave == &firstCave);
    CHECK(preparation.value().plans[1].loadedCaveData == &firstLoaded);
}

TEST_CASE("cwCave merge plan builder rejects ambiguous loaded cave ids", "[cwCaveMerge][sync]")
{
    cwCave firstCave;
    cwCave secondCave;

    cwCaveData firstLoaded = firstCave.data();
    cwCaveData secondLoaded = secondCave.data();
    secondLoaded.id = firstLoaded.id;

    const auto preparation = cwCaveMergePlanBuilder::build({&firstCave, &secondCave},
                                                            {&firstLoaded, &secondLoaded},
                                                            {});
    CHECK(preparation.hasError());
    CHECK(preparation.errorMessage() == QStringLiteral("Ambiguous loaded cave ids."));
}

TEST_CASE("cwCave merge applier merges name without replacing trips", "[cwCaveMerge][sync]")
{
    cwCave currentCave;
    currentCave.setName(QStringLiteral("base-name"));

    auto* trip = new cwTrip();
    currentCave.addTrip(trip);
    const QUuid tripIdBeforeMerge = trip->id();

    cwCaveData loadedCaveData = currentCave.data();
    loadedCaveData.name = QStringLiteral("remote-name");

    cwCaveData baseCaveData = currentCave.data();
    baseCaveData.name = QStringLiteral("base-name");

    cwCaveMergePlan plan;
    plan.currentCave = &currentCave;
    plan.loadedCaveData = &loadedCaveData;
    plan.baseCaveData = baseCaveData;

    REQUIRE_FALSE(cwCaveMergeApplier::applyCaveMergePlan(plan).hasError());
    CHECK(currentCave.name() == QStringLiteral("remote-name"));
    REQUIRE(currentCave.tripCount() == 1);
    CHECK(currentCave.trip(0)->id() == tripIdBeforeMerge);
}

TEST_CASE("cwCave merge applier keeps local name on conflict", "[cwCaveMerge][sync]")
{
    cwCave currentCave;
    currentCave.setName(QStringLiteral("ours-name"));

    cwCaveData loadedCaveData = currentCave.data();
    loadedCaveData.name = QStringLiteral("remote-name");

    cwCaveData baseCaveData = currentCave.data();
    baseCaveData.name = QStringLiteral("base-name");

    cwCaveMergePlan plan;
    plan.currentCave = &currentCave;
    plan.loadedCaveData = &loadedCaveData;
    plan.baseCaveData = baseCaveData;

    REQUIRE_FALSE(cwCaveMergeApplier::applyCaveMergePlan(plan).hasError());
    CHECK(currentCave.name() == QStringLiteral("ours-name"));
}

TEST_CASE("cwCave merge plan builder plans a changed child node and its nested trip",
          "[cwCaveMerge][sync]")
{
    cwCave parentNode;
    parentNode.setName(QStringLiteral("Kentucky field seasons"));
    parentNode.setKind(cwSurveyNode::Kind::Folder);

    auto* childNode = new cwCave();
    childNode->setName(QStringLiteral("Side Cave"));
    parentNode.addNode(childNode);

    auto* nestedTrip = new cwTrip();
    nestedTrip->setName(QStringLiteral("Entrance survey"));
    childNode->addTrip(nestedTrip);

    cwCaveData loadedParent = parentNode.data();
    REQUIRE(loadedParent.nodes.size() == 1);
    loadedParent.nodes[0].kind = cwSurveyNode::Kind::Cave;
    REQUIRE(loadedParent.nodes.at(0).trips.size() == 1);
    loadedParent.nodes[0].trips[0].name = QStringLiteral("Entrance survey 2");

    const auto preparation = cwCaveMergePlanBuilder::build({&parentNode}, {&loadedParent}, {});
    REQUIRE_FALSE(preparation.hasError());

    //One plan for the parent node and one for its child; the nested trip travels in the
    //child's plan, where cwTripSyncMergeHandler picks it up by id.
    REQUIRE(preparation.value().plans.size() == 2);
    CHECK(preparation.value().plans.at(0).currentCave == &parentNode);
    CHECK(preparation.value().plans.at(1).currentCave == childNode);
    CHECK(preparation.value().plans.at(1).loadedCaveData == &loadedParent.nodes.at(0));
    CHECK(preparation.value().plans.at(1).loadedCaveData->trips.at(0).name
          == QStringLiteral("Entrance survey 2"));
}

TEST_CASE("cwCave merge applier merges a node's kind and source scalars",
          "[cwCaveMerge][sync]")
{
    cwCave currentNode;
    currentNode.setName(QStringLiteral("Side Cave"));
    currentNode.setKind(cwSurveyNode::Kind::Cave);

    const cwCaveData baseCaveData = currentNode.data();

    cwCaveData loadedCaveData = currentNode.data();
    loadedCaveData.kind = cwSurveyNode::Kind::Folder;
    loadedCaveData.readOnly = true;
    loadedCaveData.sourceId = QUuid::createUuid();
    loadedCaveData.sourcePath = QStringLiteral("BLOWING3.DAT");

    cwCaveMergePlan plan;
    plan.currentCave = &currentNode;
    plan.loadedCaveData = &loadedCaveData;
    plan.baseCaveData = baseCaveData;

    REQUIRE_FALSE(cwCaveMergeApplier::applyCaveMergePlan(plan).hasError());
    CHECK(currentNode.kind() == cwSurveyNode::Kind::Folder);
    CHECK(currentNode.isReadOnly());
    CHECK(currentNode.sourceId() == loadedCaveData.sourceId);
    CHECK(currentNode.sourcePath() == QStringLiteral("BLOWING3.DAT"));
}

TEST_CASE("cwCave merge plan builder keeps live values on an unchanged child node",
          "[cwCaveMerge][sync]")
{
    cwCave parentNode;
    parentNode.setName(QStringLiteral("Kentucky field seasons"));
    parentNode.setKind(cwSurveyNode::Kind::Folder);

    auto* childNode = new cwCave();
    childNode->setName(QStringLiteral("Side Cave"));
    parentNode.addNode(childNode);

    //The loaded copy of the child is the pre-edit disk state: only the parent's own
    //descriptor changed in this merge, so the child's live name must survive.
    cwCaveData loadedParent = parentNode.data();
    REQUIRE(loadedParent.nodes.size() == 1);
    childNode->setName(QStringLiteral("Renamed Side Cave"));

    const auto preparation = cwCaveMergePlanBuilder::build({&parentNode}, {&loadedParent}, {});
    REQUIRE_FALSE(preparation.hasError());
    REQUIRE(preparation.value().plans.size() == 2);

    for (const cwCaveMergePlan& plan : preparation.value().plans) {
        REQUIRE_FALSE(cwCaveMergeApplier::applyCaveMergePlan(plan).hasError());
    }

    CHECK(childNode->name() == QStringLiteral("Renamed Side Cave"));
}

TEST_CASE("cwCave merge applier keeps a locally changed kind on conflict", "[cwCaveMerge][sync]")
{
    cwCave currentNode;
    currentNode.setName(QStringLiteral("Side Cave"));

    const cwCaveData baseCaveData = currentNode.data();

    currentNode.setKind(cwSurveyNode::Kind::Folder);

    cwCaveData loadedCaveData = currentNode.data();
    loadedCaveData.kind = cwSurveyNode::Kind::Cave;

    cwCaveMergePlan plan;
    plan.currentCave = &currentNode;
    plan.loadedCaveData = &loadedCaveData;
    plan.baseCaveData = baseCaveData;

    REQUIRE_FALSE(cwCaveMergeApplier::applyCaveMergePlan(plan).hasError());
    CHECK(currentNode.kind() == cwSurveyNode::Kind::Folder);
}
