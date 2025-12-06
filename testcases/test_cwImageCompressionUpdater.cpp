//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRootData.h"
#include "TestHelper.h"
#include "cwRegionTreeModel.h"
#include "cwNote.h"
#include "cwScrap.h"


TEST_CASE("cwRootData should automatically update compression for notes", "[cwImageCompressionUpdater]") {

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto checkNotesAreGood = [&rootData, project]() {
        //Test that the cwNotes have mipmaps
        const QList<cwNote*> notes = rootData->regionTreeModel()->all<cwNote*>(QModelIndex(), &cwRegionTreeModel::note);

        CHECK(notes.size() == 3);

        for(cwNote* note : notes) {
            CHECK(QFileInfo::exists(project->absolutePath(note)));
        }
    };

    SECTION("Update on load") {
        fileToProject(project, "://datasets/test_cwImageCompressionUpdater/NotesNeedUpdating.cw");
        rootData->futureManagerModel()->waitForFinished();
        checkNotesAreGood();
    }
}

