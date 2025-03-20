//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRootData.h"
#include "cwTaskManagerModel.h"
#include "TestHelper.h"
#include "cwRegionTreeModel.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwImageDatabase.h"

TEST_CASE("cwRootData should automatically update compression for notes", "[cwImageCompressionUpdater]") {

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto checkNotesAreGood = [&rootData, project]() {
        //Test that the cwNotes have mipmaps
        QList<cwNote*> notes = rootData->regionTreeModel()->all<cwNote*>(QModelIndex(), &cwRegionTreeModel::note);

        CHECK(notes.size() == 3);

        cwImageDatabase imageDatabase(project->filename());
        for(cwNote* note : notes) {
            CHECK(imageDatabase.imageExists(note->image().original()));
        }
    };

    SECTION("Update on load") {
        fileToProject(project, "://datasets/test_cwImageCompressionUpdater/NotesNeedUpdating.cw");
        rootData->futureManagerModel()->waitForFinished();
        checkNotesAreGood();
    }
}

TEST_CASE("cwRootData should automatically update compression for scaps", "[cwImageCompressionUpdater]") {

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto checkScrapsAreGood = [&rootData, project]() {
        //Test that the cwNotes have mipmaps
        QList<cwScrap*> scraps = rootData->regionTreeModel()->all<cwScrap*>(QModelIndex(), &cwRegionTreeModel::scrap);

        CHECK(scraps.size() == 1);

        cwImageDatabase imageDatabase(project->filename());
        for(cwScrap* scrap : scraps) {
            CHECK(imageDatabase.imageExists(scrap->triangulationData().croppedImage().original()));
        }
    };

    SECTION("Update on load") {
        fileToProject(project, "://datasets/test_cwImageCompressionUpdater/ScrapsNeedUpdating.cw");
        rootData->futureManagerModel()->waitForFinished();
        rootData->taskManagerModel()->waitForTasks();
        checkScrapsAreGood();
    }
}

