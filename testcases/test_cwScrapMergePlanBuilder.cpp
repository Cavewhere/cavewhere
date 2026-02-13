//Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

//Our includes
#include "cwNote.h"
#include "cwScrap.h"
#include "cwScrapMergeApplier.h"
#include "cwScrapMergePlanBuilder.h"
#include "cwSurveyNoteModel.h"

#include <memory>

namespace {

cwNoteStation makeStation(const QUuid& id, const QString& name, const QPointF& position)
{
    cwNoteStation station;
    station.setId(id);
    station.setName(name);
    station.setPositionOnNote(position);
    return station;
}

cwLead makeLead(const QUuid& id,
                const QString& description,
                const QPointF& position,
                const QSizeF& size,
                bool completed)
{
    cwLead lead;
    lead.setId(id);
    lead.setDescription(description);
    lead.setPositionOnNote(position);
    lead.setSize(size);
    lead.setCompleted(completed);
    return lead;
}

}

TEST_CASE("cwScrap merge plan builder accepts note reorder with stable identities", "[cwScrapMerge][sync]") {
    cwSurveyNoteModel noteModel;

    auto addTriangleScrap = [](const QPointF& origin) -> cwScrap* {
        auto* scrap = new cwScrap();
        scrap->insertPoint(0, origin + QPointF(0.10, 0.10));
        scrap->insertPoint(1, origin + QPointF(0.35, 0.10));
        scrap->insertPoint(2, origin + QPointF(0.22, 0.35));
        scrap->close();
        return scrap;
    };

    auto* firstNote = new cwNote(&noteModel);
    auto* secondNote = new cwNote(&noteModel);
    firstNote->addScrap(addTriangleScrap(QPointF(0.0, 0.0)));
    secondNote->addScrap(addTriangleScrap(QPointF(0.5, 0.0)));
    noteModel.addNotes({firstNote, secondNote});

    cwSurveyNoteModelData loadedNoteModelData;
    loadedNoteModelData.notes = {
        secondNote->data(),
        firstNote->data()
    };

    const auto preparation =
        cwScrapMergePlanBuilder::buildNoteStructuralMergePreparation(&noteModel, loadedNoteModelData);
    REQUIRE_FALSE(preparation.hasError());
    REQUIRE(preparation.value().orderedNotes.size() == 2);
    CHECK(preparation.value().orderedNotes.at(0) == secondNote);
    CHECK(preparation.value().orderedNotes.at(1) == firstNote);
    REQUIRE(preparation.value().plans.size() == 2);
}

TEST_CASE("cwScrap merge plan builder rejects ambiguous note identities", "[cwScrapMerge][sync]") {
    cwSurveyNoteModel noteModel;

    auto addTriangleScrap = [](const QPointF& origin) -> cwScrap* {
        auto* scrap = new cwScrap();
        scrap->insertPoint(0, origin + QPointF(0.10, 0.10));
        scrap->insertPoint(1, origin + QPointF(0.35, 0.10));
        scrap->insertPoint(2, origin + QPointF(0.22, 0.35));
        scrap->close();
        return scrap;
    };

    auto* firstNote = new cwNote(&noteModel);
    auto* secondNote = new cwNote(&noteModel);
    firstNote->addScrap(addTriangleScrap(QPointF(0.0, 0.0)));
    secondNote->addScrap(addTriangleScrap(QPointF(0.5, 0.0)));
    noteModel.addNotes({firstNote, secondNote});

    SECTION("duplicate loaded note ids") {
        cwSurveyNoteModelData loadedNoteModelData = noteModel.data();
        REQUIRE(loadedNoteModelData.notes.size() == 2);
        loadedNoteModelData.notes[1].id = loadedNoteModelData.notes[0].id;

        const auto preparation =
            cwScrapMergePlanBuilder::buildNoteStructuralMergePreparation(&noteModel, loadedNoteModelData);
        CHECK(preparation.hasError());
    }

    SECTION("null loaded note id") {
        cwSurveyNoteModelData loadedNoteModelData = noteModel.data();
        REQUIRE(loadedNoteModelData.notes.size() == 2);
        loadedNoteModelData.notes[1].id = QUuid();

        const auto preparation =
            cwScrapMergePlanBuilder::buildNoteStructuralMergePreparation(&noteModel, loadedNoteModelData);
        CHECK(preparation.hasError());
    }

    SECTION("duplicate current note ids") {
        secondNote->setId(firstNote->id());
        cwSurveyNoteModelData loadedNoteModelData = noteModel.data();

        const auto preparation =
            cwScrapMergePlanBuilder::buildNoteStructuralMergePreparation(&noteModel, loadedNoteModelData);
        CHECK(preparation.hasError());
    }
}

TEST_CASE("cwScrap merge applier applies deterministic payload by planned scrap order", "[cwScrapMerge][sync]") {
    auto addTriangleScrap = [](cwNote* note, const QPointF& origin) -> cwScrap* {
        auto* scrap = new cwScrap();
        scrap->insertPoint(0, origin + QPointF(0.10, 0.10));
        scrap->insertPoint(1, origin + QPointF(0.35, 0.10));
        scrap->insertPoint(2, origin + QPointF(0.22, 0.35));
        scrap->close();
        note->addScrap(scrap);
        return scrap;
    };

    auto note = std::make_unique<cwNote>();
    cwScrap* const firstScrap = addTriangleScrap(note.get(), QPointF(0.0, 0.0));
    cwScrap* const secondScrap = addTriangleScrap(note.get(), QPointF(0.5, 0.0));
    REQUIRE(firstScrap != nullptr);
    REQUIRE(secondScrap != nullptr);
    REQUIRE(firstScrap->id() != secondScrap->id());

    cwNoteData loadedNoteData = note->data();
    REQUIRE(loadedNoteData.scraps.size() == 2);
    std::swap(loadedNoteData.scraps[0], loadedNoteData.scraps[1]);
    REQUIRE(loadedNoteData.scraps[0].outlinePoints.size() > 1);
    loadedNoteData.scraps[0].outlinePoints[1] = QPointF(0.77, 0.55);

    const QUuid reorderedFirstId = loadedNoteData.scraps[0].id;
    REQUIRE(!reorderedFirstId.isNull());

    cwNoteStructuralMergePlan plan;
    plan.note = note.get();
    plan.loadedNoteData = &loadedNoteData;
    plan.mergedScrapOrder = {
        loadedNoteData.scraps[0].id,
        loadedNoteData.scraps[1].id
    };

    const auto applyResult = cwScrapMergeApplier::applyNoteStructuralMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());
    CHECK_FALSE(applyResult.value().geometryConflictKeptOurs);

    REQUIRE(note->scraps().size() == 2);
    CHECK(note->scrap(0)->id() == reorderedFirstId);
    CHECK(note->scrap(1)->id() != reorderedFirstId);
    REQUIRE(note->scrap(0)->points().size() > 1);
    CHECK(note->scrap(0)->points().at(1) == QPointF(0.77, 0.55));
}

TEST_CASE("cwScrap merge applier merges stations and leads by stable id with local precedence", "[cwScrapMerge][sync]")
{
    auto note = std::make_unique<cwNote>();
    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    const QUuid sharedStationId = QUuid::createUuid();
    const QUuid localOnlyStationId = QUuid::createUuid();
    const QUuid remoteOnlyStationId = QUuid::createUuid();
    const QUuid sharedLeadId = QUuid::createUuid();
    const QUuid localOnlyLeadId = QUuid::createUuid();
    const QUuid remoteOnlyLeadId = QUuid::createUuid();

    scrap->setStations({
        makeStation(sharedStationId, QStringLiteral("ours-shared"), QPointF(1.0, 1.0)),
        makeStation(localOnlyStationId, QStringLiteral("ours-local"), QPointF(2.0, 2.0))
    });
    scrap->setLeads({
        makeLead(sharedLeadId, QStringLiteral("ours-shared"), QPointF(3.0, 3.0), QSizeF(1.0, 1.5), false),
        makeLead(localOnlyLeadId, QStringLiteral("ours-local"), QPointF(4.0, 4.0), QSizeF(2.0, 2.5), true)
    });

    cwNoteData loadedNoteData = note->data();
    REQUIRE(loadedNoteData.scraps.size() == 1);
    auto& loadedScrap = loadedNoteData.scraps[0];
    loadedScrap.stations = {
        makeStation(sharedStationId, QStringLiteral("theirs-shared"), QPointF(10.0, 10.0)),
        makeStation(remoteOnlyStationId, QStringLiteral("theirs-remote"), QPointF(20.0, 20.0))
    };
    loadedScrap.leads = {
        makeLead(sharedLeadId, QStringLiteral("theirs-shared"), QPointF(30.0, 30.0), QSizeF(3.0, 3.5), true),
        makeLead(remoteOnlyLeadId, QStringLiteral("theirs-remote"), QPointF(40.0, 40.0), QSizeF(4.0, 4.5), false)
    };

    cwNoteStructuralMergePlan plan;
    plan.note = note.get();
    plan.loadedNoteData = &loadedNoteData;
    plan.mergedScrapOrder = {loadedScrap.id};
    const auto applyResult = cwScrapMergeApplier::applyNoteStructuralMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());
    CHECK_FALSE(applyResult.value().geometryConflictKeptOurs);

    REQUIRE(note->scraps().size() == 1);
    const QList<cwNoteStation> mergedStations = note->scrap(0)->stations();
    REQUIRE(mergedStations.size() == 3);
    CHECK(mergedStations[0].id() == sharedStationId);
    CHECK(mergedStations[0].name() == QStringLiteral("ours-shared"));
    CHECK(mergedStations[0].positionOnNote() == QPointF(1.0, 1.0));
    CHECK(mergedStations[1].id() == localOnlyStationId);
    CHECK(mergedStations[1].name() == QStringLiteral("ours-local"));
    CHECK(mergedStations[2].id() == remoteOnlyStationId);
    CHECK(mergedStations[2].name() == QStringLiteral("theirs-remote"));

    const QList<cwLead> mergedLeads = note->scrap(0)->leads();
    REQUIRE(mergedLeads.size() == 3);
    CHECK(mergedLeads[0].id() == sharedLeadId);
    CHECK(mergedLeads[0].desciption() == QStringLiteral("ours-shared"));
    CHECK(mergedLeads[0].positionOnNote() == QPointF(3.0, 3.0));
    CHECK(mergedLeads[1].id() == localOnlyLeadId);
    CHECK(mergedLeads[1].desciption() == QStringLiteral("ours-local"));
    CHECK(mergedLeads[2].id() == remoteOnlyLeadId);
    CHECK(mergedLeads[2].desciption() == QStringLiteral("theirs-remote"));
}

TEST_CASE("cwScrap merge applier falls back to loaded stations and leads when ids are ambiguous", "[cwScrapMerge][sync]")
{
    auto note = std::make_unique<cwNote>();
    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    const QUuid sharedStationId = QUuid::createUuid();
    const QUuid sharedLeadId = QUuid::createUuid();

    scrap->setStations({makeStation(sharedStationId, QStringLiteral("ours-station"), QPointF(1.0, 1.0))});
    scrap->setLeads({makeLead(sharedLeadId, QStringLiteral("ours-lead"), QPointF(2.0, 2.0), QSizeF(1.0, 1.0), false)});

    cwNoteData loadedNoteData = note->data();
    REQUIRE(loadedNoteData.scraps.size() == 1);
    auto& loadedScrap = loadedNoteData.scraps[0];

    loadedScrap.stations = {
        makeStation(sharedStationId, QStringLiteral("theirs-station-a"), QPointF(10.0, 10.0)),
        makeStation(sharedStationId, QStringLiteral("theirs-station-b"), QPointF(11.0, 11.0))
    };

    loadedScrap.leads = {
        makeLead(sharedLeadId, QStringLiteral("theirs-lead-a"), QPointF(12.0, 12.0), QSizeF(2.0, 2.0), true),
        makeLead(QUuid(), QStringLiteral("theirs-lead-b"), QPointF(13.0, 13.0), QSizeF(3.0, 3.0), true)
    };

    cwNoteStructuralMergePlan plan;
    plan.note = note.get();
    plan.loadedNoteData = &loadedNoteData;
    plan.mergedScrapOrder = {loadedScrap.id};
    const auto applyResult = cwScrapMergeApplier::applyNoteStructuralMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());
    CHECK_FALSE(applyResult.value().geometryConflictKeptOurs);

    const QList<cwNoteStation> mergedStations = note->scrap(0)->stations();
    REQUIRE(mergedStations.size() == 2);
    CHECK(mergedStations[0].name() == QStringLiteral("theirs-station-a"));
    CHECK(mergedStations[1].name() == QStringLiteral("theirs-station-b"));

    const QList<cwLead> mergedLeads = note->scrap(0)->leads();
    REQUIRE(mergedLeads.size() == 2);
    CHECK(mergedLeads[0].desciption() == QStringLiteral("theirs-lead-a"));
    CHECK(mergedLeads[1].desciption() == QStringLiteral("theirs-lead-b"));
}

TEST_CASE("cwScrap merge applier keeps local deletes when base marks remote-only ids as pre-existing", "[cwScrapMerge][sync]")
{
    auto note = std::make_unique<cwNote>();
    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    const QUuid keptStationId = QUuid::createUuid();
    const QUuid deletedStationId = QUuid::createUuid();
    const QUuid keptLeadId = QUuid::createUuid();
    const QUuid deletedLeadId = QUuid::createUuid();

    // Ours deleted deletedStationId/deletedLeadId and kept only keptStationId/keptLeadId.
    scrap->setStations({
        makeStation(keptStationId, QStringLiteral("ours-kept"), QPointF(1.0, 1.0))
    });
    scrap->setLeads({
        makeLead(keptLeadId, QStringLiteral("ours-kept"), QPointF(2.0, 2.0), QSizeF(1.0, 1.0), false)
    });

    cwNoteData loadedNoteData = note->data();
    REQUIRE(loadedNoteData.scraps.size() == 1);
    auto& loadedScrap = loadedNoteData.scraps[0];

    // Theirs still has deleted ids (remote kept/modified), and also includes the kept ids.
    loadedScrap.stations = {
        makeStation(keptStationId, QStringLiteral("theirs-kept"), QPointF(10.0, 10.0)),
        makeStation(deletedStationId, QStringLiteral("theirs-deleted"), QPointF(11.0, 11.0))
    };
    loadedScrap.leads = {
        makeLead(keptLeadId, QStringLiteral("theirs-kept"), QPointF(12.0, 12.0), QSizeF(2.0, 2.0), true),
        makeLead(deletedLeadId, QStringLiteral("theirs-deleted"), QPointF(13.0, 13.0), QSizeF(3.0, 3.0), true)
    };

    cwScrapBaseIdentityData baseIdentity;
    baseIdentity.stationIds = {keptStationId, deletedStationId};
    baseIdentity.leadIds = {keptLeadId, deletedLeadId};

    cwNoteStructuralMergePlan plan;
    plan.note = note.get();
    plan.loadedNoteData = &loadedNoteData;
    plan.mergedScrapOrder = {loadedScrap.id};
    plan.baseScrapIdentityByScrapId.insert(loadedScrap.id, baseIdentity);
    const auto applyResult = cwScrapMergeApplier::applyNoteStructuralMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());
    CHECK_FALSE(applyResult.value().geometryConflictKeptOurs);

    const QList<cwNoteStation> mergedStations = note->scrap(0)->stations();
    REQUIRE(mergedStations.size() == 1);
    CHECK(mergedStations[0].id() == keptStationId);
    CHECK(mergedStations[0].name() == QStringLiteral("ours-kept"));

    const QList<cwLead> mergedLeads = note->scrap(0)->leads();
    REQUIRE(mergedLeads.size() == 1);
    CHECK(mergedLeads[0].id() == keptLeadId);
    CHECK(mergedLeads[0].desciption() == QStringLiteral("ours-kept"));
}

TEST_CASE("cwScrap merge applier applies remote value when local matches base for shared id", "[cwScrapMerge][sync]")
{
    auto note = std::make_unique<cwNote>();
    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    const QUuid stationId = QUuid::createUuid();

    cwNoteStation oursStation = makeStation(stationId, QStringLiteral("A1"), QPointF(0.20, 0.20));
    scrap->setStations({oursStation});

    cwNoteData loadedNoteData = note->data();
    REQUIRE(loadedNoteData.scraps.size() == 1);
    auto& loadedScrap = loadedNoteData.scraps[0];
    loadedScrap.stations = {
        makeStation(stationId, QStringLiteral("A1-remote"), QPointF(0.44, 0.33))
    };

    cwScrapBaseIdentityData baseIdentity;
    baseIdentity.stationIds = {stationId};
    baseIdentity.stationsById.insert(stationId, oursStation);

    cwNoteStructuralMergePlan plan;
    plan.note = note.get();
    plan.loadedNoteData = &loadedNoteData;
    plan.mergedScrapOrder = {loadedScrap.id};
    plan.baseScrapIdentityByScrapId.insert(loadedScrap.id, baseIdentity);
    const auto applyResult = cwScrapMergeApplier::applyNoteStructuralMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());
    CHECK_FALSE(applyResult.value().geometryConflictKeptOurs);

    const QList<cwNoteStation> mergedStations = note->scrap(0)->stations();
    REQUIRE(mergedStations.size() == 1);
    CHECK(mergedStations[0].name() == QStringLiteral("A1-remote"));
    CHECK(mergedStations[0].positionOnNote() == QPointF(0.44, 0.33));
}

TEST_CASE("cwScrap merge applier replaces geometry when only remote geometry changed", "[cwScrapMerge][sync]")
{
    auto note = std::make_unique<cwNote>();
    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    scrap->insertPoint(0, QPointF(0.10, 0.10));
    scrap->insertPoint(1, QPointF(0.30, 0.10));
    scrap->insertPoint(2, QPointF(0.20, 0.30));
    scrap->close();

    cwNoteData loadedNoteData = note->data();
    REQUIRE(loadedNoteData.scraps.size() == 1);
    auto& loadedScrap = loadedNoteData.scraps[0];
    loadedScrap.outlinePoints[1] = QPointF(0.42, 0.18);

    cwScrapBaseIdentityData baseIdentity;
    baseIdentity.hasGeometryData = true;
    baseIdentity.geometry.outlinePoints = note->scrap(0)->points();
    baseIdentity.geometry.transform.noteTransformation = note->scrap(0)->noteTransformation()->data();
    baseIdentity.geometry.transform.calculateNoteTransform = note->scrap(0)->calculateNoteTransform();
    baseIdentity.geometry.transform.viewType = cwScrapType::Plan;

    cwNoteStructuralMergePlan plan;
    plan.note = note.get();
    plan.loadedNoteData = &loadedNoteData;
    plan.mergedScrapOrder = {loadedScrap.id};
    plan.baseScrapIdentityByScrapId.insert(loadedScrap.id, baseIdentity);

    const auto applyResult = cwScrapMergeApplier::applyNoteStructuralMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());
    CHECK_FALSE(applyResult.value().geometryConflictKeptOurs);
    CHECK(note->scrap(0)->points().at(1) == QPointF(0.42, 0.18));
}

TEST_CASE("cwScrap merge applier keeps local geometry on concurrent geometry conflict", "[cwScrapMerge][sync]")
{
    auto note = std::make_unique<cwNote>();
    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    scrap->insertPoint(0, QPointF(0.10, 0.10));
    scrap->insertPoint(1, QPointF(0.30, 0.10));
    scrap->insertPoint(2, QPointF(0.20, 0.30));
    scrap->close();

    // Local geometry change.
    scrap->setPoint(1, QPointF(0.36, 0.14));

    cwNoteData loadedNoteData = note->data();
    REQUIRE(loadedNoteData.scraps.size() == 1);
    auto& loadedScrap = loadedNoteData.scraps[0];
    // Remote geometry change on same base.
    loadedScrap.outlinePoints[1] = QPointF(0.44, 0.20);

    cwScrapBaseIdentityData baseIdentity;
    baseIdentity.hasGeometryData = true;
    baseIdentity.geometry.outlinePoints = {
        QPointF(0.10, 0.10),
        QPointF(0.30, 0.10),
        QPointF(0.20, 0.30),
        QPointF(0.10, 0.10)
    };
    baseIdentity.geometry.transform.noteTransformation = note->scrap(0)->noteTransformation()->data();
    baseIdentity.geometry.transform.calculateNoteTransform = note->scrap(0)->calculateNoteTransform();
    baseIdentity.geometry.transform.viewType = cwScrapType::Plan;

    cwNoteStructuralMergePlan plan;
    plan.note = note.get();
    plan.loadedNoteData = &loadedNoteData;
    plan.mergedScrapOrder = {loadedScrap.id};
    plan.baseScrapIdentityByScrapId.insert(loadedScrap.id, baseIdentity);

    const auto applyResult = cwScrapMergeApplier::applyNoteStructuralMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());
    CHECK(applyResult.value().geometryConflictKeptOurs);
    CHECK(note->scrap(0)->points().at(1) == QPointF(0.36, 0.14));
}
