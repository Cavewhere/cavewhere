//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace Catch;

//Our includes
#include "cwNote.h"
#include "cwScrap.h"
#include "cwScrapMergeApplier.h"
#include "cwScrapMergePlanBuilder.h"
#include "cwSurveyNoteModel.h"

#include <memory>

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
    REQUIRE(preparation.has_value());
    REQUIRE(preparation->orderedNotes.size() == 2);
    CHECK(preparation->orderedNotes.at(0) == secondNote);
    CHECK(preparation->orderedNotes.at(1) == firstNote);
    REQUIRE(preparation->plans.size() == 2);
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
        CHECK_FALSE(preparation.has_value());
    }

    SECTION("null loaded note id") {
        cwSurveyNoteModelData loadedNoteModelData = noteModel.data();
        REQUIRE(loadedNoteModelData.notes.size() == 2);
        loadedNoteModelData.notes[1].id = QUuid();

        const auto preparation =
            cwScrapMergePlanBuilder::buildNoteStructuralMergePreparation(&noteModel, loadedNoteModelData);
        CHECK_FALSE(preparation.has_value());
    }

    SECTION("duplicate current note ids") {
        secondNote->setId(firstNote->id());
        cwSurveyNoteModelData loadedNoteModelData = noteModel.data();

        const auto preparation =
            cwScrapMergePlanBuilder::buildNoteStructuralMergePreparation(&noteModel, loadedNoteModelData);
        CHECK_FALSE(preparation.has_value());
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
    loadedNoteData.name = QStringLiteral("Remote Ordered Note");
    loadedNoteData.rotate = 42.0;

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

    cwScrapMergeApplier::applyNoteStructuralMergePlan(plan);

    CHECK(note->name() == QStringLiteral("Remote Ordered Note"));
    CHECK(note->rotate() == Catch::Approx(42.0));
    REQUIRE(note->scraps().size() == 2);
    CHECK(note->scrap(0)->id() == reorderedFirstId);
    CHECK(note->scrap(1)->id() != reorderedFirstId);
    REQUIRE(note->scrap(0)->points().size() > 1);
    CHECK(note->scrap(0)->points().at(1) == QPointF(0.77, 0.55));
}
