//Catch includes
#include <catch2/catch_test_macros.hpp>
#include "LoadProjectHelper.h"

//Qt includes
#include <QSignalSpy>
#include <QAbstractItemModelTester>
#include <QUndoStack>

//Our includes
#include "cwRegionTreeModel.h"
#include "cwProject.h"
#include "TestHelper.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwCavingRegion.h"
#include "cwNoteLiDAR.h"
#include "cwSurveyNode.h"

TEST_CASE("cwRegionTreeModel all function should work correctly", "[cwRegionTreeModel]") {

    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    fileToProject(project.get(), testcasesDatasetPath("test_cwRegionTreeModel/testAllFunction.cw"));

    auto regionModel = std::make_unique<cwRegionTreeModel>();
    regionModel->setCavingRegion(project->cavingRegion());
    QList<cwScrap*> allScraps = regionModel->all<cwScrap*>(QModelIndex(), &cwRegionTreeModel::scrap);

    CHECK(allScraps.size() == 3);
    CHECK(QSet<cwScrap*>(allScraps.begin(), allScraps.end()).size() == 3); //Make sure they're all unique

    REQUIRE(project->cavingRegion()->caves().size() == 1);
    cwCave* cave = project->cavingRegion()->caves().at(0);

    REQUIRE(cave->tripCount() == 2);
    cwTrip* firstTrip = cave->trips().at(0);
    cwTrip* secondTrip = cave->trips().at(1);

    QModelIndex firstTripIndex = regionModel->index(firstTrip);
    CHECK(firstTripIndex.isValid());

    QModelIndex secondTripIndex = regionModel->index(secondTrip);
    CHECK(secondTripIndex.isValid());

    auto trip1Scraps = regionModel->all<cwScrap*>(firstTripIndex, &cwRegionTreeModel::scrap);

    REQUIRE(firstTrip->notes()->notes().size() == 1);
    auto firstNote = firstTrip->notes()->notes().at(0);
    CHECK(firstNote->scraps() == trip1Scraps);

    auto trip2Scraps = regionModel->all<cwScrap*>(secondTripIndex, &cwRegionTreeModel::scrap);

    REQUIRE(secondTrip->notes()->notes().size() == 1);
    auto secondNote = secondTrip->notes()->notes().at(0);
    CHECK(secondNote->scraps() == trip2Scraps);
}

TEST_CASE("cwRegionTreeModel object function should work correctly", "[cwRegionTreeModel]") {

    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    fileToProject(project.get(), testcasesDatasetPath("test_cwRegionTreeModel/testAllFunction.cw"));

    auto regionModel = std::make_unique<cwRegionTreeModel>();
    regionModel->setCavingRegion(project->cavingRegion());

    REQUIRE(project->cavingRegion()->caves().size() == 1);
    cwCave* cave = project->cavingRegion()->caves().at(0);

    REQUIRE(cave->tripCount() == 2);
    cwTrip* firstTrip = cave->trips().at(0);

    QModelIndex firstTripIndex = regionModel->index(firstTrip);
    CHECK(firstTripIndex.isValid());

    int count = regionModel->rowCount(firstTripIndex);
    CHECK(count == static_cast<int>(cwRegionTreeModel::TripRows::NumberOfRows));

    auto notesIndex = regionModel->index(static_cast<int>(cwRegionTreeModel::TripRows::NotesModel), 0, firstTripIndex);
    count = regionModel->rowCount(notesIndex);
    CHECK(count == 1);

    QList<cwNote*> notes = regionModel->objects<cwNote*>(notesIndex,
                                                         0,
                                                         count - 1,
                                                         &cwRegionTreeModel::note);

    CHECK(notes.size() == 1);
    CHECK(notes == firstTrip->notes()->notes());
}

// Small helper: fetch typed data out of model index
template <typename T>
static T* objAt(const QAbstractItemModel* m, const QModelIndex& idx, int role)
{
    QVariant v = m->data(idx, role);
    if (!v.isValid() || !v.canConvert<T*>()) { return nullptr; }
    return v.value<T*>();
}

TEST_CASE("LiDAR rows are exposed under Trip and react to insert/remove", "[cwRegionTreeModel][LiDAR]") {
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    fileToProject(project.get(), testcasesDatasetPath("test_cwRegionTreeModel/testAllFunction.cw"));

    auto regionModel = std::make_unique<cwRegionTreeModel>();
    regionModel->setCavingRegion(project->cavingRegion());

    REQUIRE(project->cavingRegion()->caves().size() == 1);
    cwCave* cave = project->cavingRegion()->caves().at(0);
    REQUIRE(cave->tripCount() >= 1);
    cwTrip* trip = cave->trips().at(0);

    // Trip node exists
    QModelIndex tripIdx = regionModel->index(trip);
    REQUIRE(tripIdx.isValid());
    CHECK(regionModel->data(tripIdx, cwRegionTreeModel::TypeRole).toInt() ==
          cwRegionTreeModel::TripType);

    // The Trip has two fixed rows: NotesModel (0) and NotesLiDARModel (1)
    CHECK(regionModel->rowCount(tripIdx) ==
          static_cast<int>(cwRegionTreeModel::TripRows::NumberOfRows));

    // Grab the NotesLiDAR container index (row = NotesLiDARModel)
    const int lidarRow = static_cast<int>(cwRegionTreeModel::TripRows::NotesLiDARModel);
    QModelIndex lidarContainerIdx = regionModel->index(lidarRow, 0, tripIdx);
    REQUIRE(lidarContainerIdx.isValid());
    CHECK(regionModel->data(lidarContainerIdx, cwRegionTreeModel::TypeRole).toInt() ==
          cwRegionTreeModel::NotesLiDARType);

    // The container’s object payload should be the trip’s cwSurveyNoteLiDARModel
    auto* lidarModel =
        objAt<cwSurveyNoteLiDARModel>(regionModel.get(), lidarContainerIdx, cwRegionTreeModel::ObjectRole);
    REQUIRE(lidarModel != nullptr);
    CHECK(lidarModel == trip->notesLiDAR());

    // Expect initially zero or whatever your dataset loads; just read it:
    const int initialCount = regionModel->rowCount(lidarContainerIdx);

    // Wire spies on the *tree model* to confirm propagated inserts/removes
    QSignalSpy spyAboutToInsert(regionModel.get(), &QAbstractItemModel::rowsAboutToBeInserted);
    QSignalSpy spyInserted(regionModel.get(), &QAbstractItemModel::rowsInserted);
    QSignalSpy spyAboutToRemove(regionModel.get(), &QAbstractItemModel::rowsAboutToBeRemoved);
    QSignalSpy spyRemoved(regionModel.get(), &QAbstractItemModel::rowsRemoved);

    SECTION("add LiDAR notes propagates to RegionTreeModel and indexes resolve to cwNoteLiDAR*") {
        // Create two LiDAR notes
        auto* n0 = new cwNoteLiDAR(lidarModel);
        n0->setFilename(QStringLiteral("scan_000.las"));
        auto* n1 = new cwNoteLiDAR(lidarModel);
        n1->setFilename(QStringLiteral("scan_001.las"));

        // ---- Adjust the following add API to your model if different ----
        // Prefer a single append/add API that emits rowsAboutToBeInserted/rowsInserted:
        // e.g. lidarModel->addNote(n0); lidarModel->addNote(n1);
        // or lidarModel->appendNotes({n0, n1});
        // -----------------------------------------------------------------
        lidarModel->addNotes({n0});
        lidarModel->addNotes({n1});

        // RegionTreeModel should have observed two insert transactions.
        // Depending on your model, adds may be coalesced; allow >=1.
        REQUIRE(spyAboutToInsert.count() >= 1);
        REQUIRE(spyInserted.count()      >= 1);

        // Check parent indexes for inserts are the LiDAR container
        for (const auto& rec : spyAboutToInsert) {
            auto parent = rec.at(0).value<QModelIndex>();
            CHECK(parent == lidarContainerIdx);
        }
        for (const auto& rec : spyInserted) {
            auto parent = rec.at(0).value<QModelIndex>();
            CHECK(parent == lidarContainerIdx);
        }

        // Row count reflects adds
        const int afterAdds = regionModel->rowCount(lidarContainerIdx);
        CHECK(afterAdds == initialCount + 2);

        // Validate child nodes resolve to cwNoteLiDAR* and parent() returns container
        for (int r = 0; r < afterAdds; ++r) {
            QModelIndex child = regionModel->index(r, 0, lidarContainerIdx);
            REQUIRE(child.isValid());
            CHECK(regionModel->data(child, cwRegionTreeModel::TypeRole).toInt() ==
                  cwRegionTreeModel::NoteLiDARType);

            auto* noteLiDAR = objAt<cwNoteLiDAR>(regionModel.get(), child, cwRegionTreeModel::ObjectRole);
            REQUIRE(noteLiDAR != nullptr);

            // parent() of a LiDAR note must be the LiDAR model container
            QModelIndex p = regionModel->parent(child);
            CHECK(p == lidarContainerIdx);
        }
    }

    SECTION("remove LiDAR notes propagates to RegionTreeModel") {
        // Ensure we have at least one item to remove: add one
        auto* n0 = new cwNoteLiDAR(lidarModel);
        n0->setFilename(QStringLiteral("scan_100.las"));

        lidarModel->addNotes({n0});

        const int countBeforeRemove = regionModel->rowCount(lidarContainerIdx);
        REQUIRE(countBeforeRemove >= initialCount + 1);

        // Remove the last row for simplicity
        const int rowToRemove = countBeforeRemove - 1;

        // ---- Adjust the following remove API to your model if different ----
        // e.g. lidarModel->remove(rowToRemove); or removeNote(n0);
        bool okRemove = false;
        // Try common row-based API first:
        okRemove = QMetaObject::invokeMethod(lidarModel, "removeNote",
                                             Q_ARG(int, 0));

        REQUIRE(okRemove);

        // RegionTreeModel should have observed a remove transaction
        REQUIRE(spyAboutToRemove.count() >= 1);
        REQUIRE(spyRemoved.count()       >= 1);

        // Parents on remove signals should be the LiDAR container
        for (const auto& rec : spyAboutToRemove) {
            auto parent = rec.at(0).value<QModelIndex>();
            CHECK(parent == lidarContainerIdx);
        }
        for (const auto& rec : spyRemoved) {
            auto parent = rec.at(0).value<QModelIndex>();
            CHECK(parent == lidarContainerIdx);
        }

        const int afterRemove = regionModel->rowCount(lidarContainerIdx);
        CHECK(afterRemove == countBeforeRemove - 1);
    }

    SECTION("index(parent(model)) contract for LiDAR container") {
        // The container’s parent() must be the Trip index
        QModelIndex parentOfContainer = regionModel->parent(lidarContainerIdx);
        REQUIRE(parentOfContainer.isValid());
        // qDebug() << "ParentContainer:" << parentOfContainer << tripIdx;
        CHECK(parentOfContainer == tripIdx);

        // And its data(ObjectRole) is the model itself
        auto* modelInTree =
            objAt<cwSurveyNoteLiDARModel>(regionModel.get(), lidarContainerIdx, cwRegionTreeModel::ObjectRole);
        CHECK(modelInTree == lidarModel);
    }
}

static QModelIndex idxTripNotesContainer(const cwRegionTreeModel& m, const QModelIndex& tripIdx) {
    const int notesRow = static_cast<int>(cwRegionTreeModel::TripRows::NotesModel);
    return m.index(notesRow, 0, tripIdx);
}

static QModelIndex idxTripNotesLidarContainer(const cwRegionTreeModel& m, const QModelIndex& tripIdx) {
    const int lidarRow = static_cast<int>(cwRegionTreeModel::TripRows::NotesLiDARModel);
    return m.index(lidarRow, 0, tripIdx);
}

TEST_CASE("cwRegionTreeModel::parent correctness across all levels", "[cwRegionTreeModel]") {
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    fileToProject(project.get(), testcasesDatasetPath("test_cwRegionTreeModel/testAllFunction.cw"));

    auto model = std::make_unique<cwRegionTreeModel>();
    model->setCavingRegion(project->cavingRegion());

    // --- Root (Region) ---
    // Root is represented by invalid QModelIndex(). Its parent is invalid by definition.
    CHECK(model->parent(QModelIndex()) == QModelIndex());

    // Expect at least one cave from dataset
    REQUIRE(project->cavingRegion()->caves().size() >= 1);
    const int caveCount = model->rowCount(QModelIndex());
    REQUIRE(caveCount == project->cavingRegion()->caves().size());

    for (int caveRow = 0; caveRow < caveCount; ++caveRow) {
        // --- Cave level ---
        const QModelIndex caveIdx = model->index(caveRow, 0, QModelIndex());
        REQUIRE(caveIdx.isValid());
        CHECK(model->data(caveIdx, cwRegionTreeModel::TypeRole).toInt() == cwRegionTreeModel::CaveType);

        // Parent of cave is Region (invalid index)
        CHECK(model->parent(caveIdx) == QModelIndex());

        // Trips under cave
        const int tripCount = model->rowCount(caveIdx);
        REQUIRE(tripCount >= 0);

        for (int tripRow = 0; tripRow < tripCount; ++tripRow) {
            // --- Trip level ---
            const QModelIndex tripIdx = model->index(tripRow, 0, caveIdx);
            REQUIRE(tripIdx.isValid());
            CHECK(model->data(tripIdx, cwRegionTreeModel::TypeRole).toInt() == cwRegionTreeModel::TripType);

            // Parent of trip is its cave (same caveIdx)
            CHECK(model->parent(tripIdx) == caveIdx);

            // --- Notes containers under trip ---
            const QModelIndex notesContainer = idxTripNotesContainer(*model, tripIdx);
            REQUIRE(notesContainer.isValid());
            CHECK(model->data(notesContainer, cwRegionTreeModel::TypeRole).toInt() == cwRegionTreeModel::NotesType);

            // Parent of Notes container MUST be the trip index
           CHECK(model->parent(notesContainer) == tripIdx);

            const QModelIndex lidarContainer = idxTripNotesLidarContainer(*model, tripIdx);
            REQUIRE(lidarContainer.isValid());
            CHECK(model->data(lidarContainer, cwRegionTreeModel::TypeRole).toInt() == cwRegionTreeModel::NotesLiDARType);

            // Parent of LiDAR container MUST be the trip index
            CHECK(model->parent(lidarContainer) == tripIdx);

            // --- Paper notes under Notes container ---
            const int paperNoteCount = model->rowCount(notesContainer);
            for (int noteRow = 0; noteRow < paperNoteCount; ++noteRow) {
                const QModelIndex noteIdx = model->index(noteRow, 0, notesContainer);
                REQUIRE(noteIdx.isValid());
                CHECK(model->data(noteIdx, cwRegionTreeModel::TypeRole).toInt() == cwRegionTreeModel::NoteType);

                // Parent of a note MUST be the notesContainer (row 0 under trip)
                CHECK(model->parent(noteIdx) == notesContainer);

                // --- Scraps under note ---
                const int scrapCount = model->rowCount(noteIdx);
                for (int scrapRow = 0; scrapRow < scrapCount; ++scrapRow) {
                    const QModelIndex scrapIdx = model->index(scrapRow, 0, noteIdx);
                    REQUIRE(scrapIdx.isValid());
                    CHECK(model->data(scrapIdx, cwRegionTreeModel::TypeRole).toInt() == cwRegionTreeModel::ScrapType);

                    // Parent of scrap MUST be noteIdx
                    CHECK(model->parent(scrapIdx) == noteIdx);
                }
            }

            // --- LiDAR notes under LiDAR container ---
            // Ensure there is at least one LiDAR note to test parentage even if dataset has none:
            if (model->rowCount(lidarContainer) == 0) {
                // Add one LiDAR note via the model; RegionTreeModel already connects signals
                auto* lidarModel = model->notesLiDARModel(lidarContainer);
                REQUIRE(lidarModel != nullptr);

                auto* nl = new cwNoteLiDAR(lidarModel);
                nl->setFilename(QStringLiteral("parent_test.las"));
                lidarModel->addNotes({ nl });

                // sanity: now >= 1
                REQUIRE(model->rowCount(lidarContainer) >= 1);
            }

            const int lidarNoteCount = model->rowCount(lidarContainer);
            for (int lrow = 0; lrow < lidarNoteCount; ++lrow) {
                const QModelIndex lidarNoteIdx = model->index(lrow, 0, lidarContainer);
                REQUIRE(lidarNoteIdx.isValid());
                CHECK(model->data(lidarNoteIdx, cwRegionTreeModel::TypeRole).toInt() == cwRegionTreeModel::NoteLiDARType);

                // Parent of LiDAR note MUST be lidarContainer (row 1 under trip)
                CHECK(model->parent(lidarNoteIdx) == lidarContainer);
            }
        }
    }
}

TEST_CASE("cwRegionTreeModel lists a node's child nodes before its trips",
          "[cwRegionTreeModel]") {
    // A depth-3 tree: Fisher Ridge > Upper Level > Crawl, with a trip at every
    // level. The tree is built before the cave joins the region, so the model
    // hears about the whole subtree through one insert.
    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));

    cwTrip* caveTrip = new cwTrip();
    caveTrip->setName(QStringLiteral("Entrance Series"));
    cave->addTrip(caveTrip);

    cwCave* folder = new cwCave();
    folder->setName(QStringLiteral("Upper Level"));
    cave->addNode(folder);

    cwTrip* folderTrip = new cwTrip();
    folderTrip->setName(QStringLiteral("Topo 1"));
    folder->addTrip(folderTrip);

    cwCave* section = new cwCave();
    section->setName(QStringLiteral("Crawl"));
    folder->addNode(section);

    cwTrip* sectionTrip = new cwTrip();
    sectionTrip->setName(QStringLiteral("Topo 2"));
    section->addTrip(sectionTrip);

    region.addCave(cave);

    cwRegionTreeModel model;
    model.setCavingRegion(&region);

    //Validates index(), parent(), rowCount() and data() over the whole tree
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);

    SECTION("rows and parents follow the tree") {
        REQUIRE(model.rowCount(QModelIndex()) == 1);

        const QModelIndex caveIndex = model.index(0, 0, QModelIndex());
        REQUIRE(caveIndex.isValid());
        CHECK(model.data(caveIndex, cwRegionTreeModel::TypeRole).toInt() == cwRegionTreeModel::NodeType);
        CHECK(model.node(caveIndex) == cave);
        CHECK(model.cave(caveIndex) == cave);
        CHECK(model.index(cave) == caveIndex);
        CHECK(model.parent(caveIndex) == QModelIndex());

        //One child node, then one trip
        REQUIRE(model.rowCount(caveIndex) == 2);

        const QModelIndex folderIndex = model.index(0, 0, caveIndex);
        CHECK(model.node(folderIndex) == folder);
        CHECK(model.index(folder) == folderIndex);
        CHECK(model.parent(folderIndex) == caveIndex);

        const QModelIndex caveTripIndex = model.index(1, 0, caveIndex);
        CHECK(model.trip(caveTripIndex) == caveTrip);
        CHECK(model.index(caveTrip) == caveTripIndex);
        CHECK(model.parent(caveTripIndex) == caveIndex);

        REQUIRE(model.rowCount(folderIndex) == 2);
        const QModelIndex sectionIndex = model.index(0, 0, folderIndex);
        CHECK(model.node(sectionIndex) == section);
        CHECK(model.parent(sectionIndex) == folderIndex);
        CHECK(model.trip(model.index(1, 0, folderIndex)) == folderTrip);

        REQUIRE(model.rowCount(sectionIndex) == 1);
        const QModelIndex sectionTripIndex = model.index(0, 0, sectionIndex);
        CHECK(model.trip(sectionTripIndex) == sectionTrip);
        CHECK(model.index(sectionTrip) == sectionTripIndex);
        CHECK(model.parent(sectionTripIndex) == sectionIndex);

        //The region's root owns no row of its own
        CHECK(model.index(region.rootNode()) == QModelIndex());
    }

    SECTION("all<>() walks the rows in the tree's document order") {
        const QList<cwTrip*> trips = model.all<cwTrip*>(QModelIndex(), &cwRegionTreeModel::trip);
        CHECK(trips == region.rootNode()->allTrips());
        CHECK(trips == QList<cwTrip*>({sectionTrip, folderTrip, caveTrip}));

        const QList<cwCave*> nodes = model.all<cwCave*>(QModelIndex(), &cwRegionTreeModel::cave);
        CHECK(nodes == QList<cwCave*>({cave, folder, section}));
    }

    SECTION("a node and a trip added to a deep node become rows") {
        const QModelIndex sectionIndex = model.index(section);
        REQUIRE(sectionIndex.isValid());

        QSignalSpy aboutToInsert(&model, &QAbstractItemModel::rowsAboutToBeInserted);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

        cwCave* deepNode = new cwCave();
        deepNode->setName(QStringLiteral("Sump"));
        section->addNode(deepNode);

        REQUIRE(aboutToInsert.count() == 1);
        REQUIRE(inserted.count() == 1);
        CHECK(aboutToInsert.at(0).at(0).value<QModelIndex>() == sectionIndex);
        CHECK(inserted.at(0).at(1).toInt() == 0); //Child nodes come first

        REQUIRE(model.rowCount(sectionIndex) == 2);
        CHECK(model.node(model.index(0, 0, sectionIndex)) == deepNode);
        CHECK(model.trip(model.index(1, 0, sectionIndex)) == sectionTrip);
        CHECK(model.index(sectionTrip) == model.index(1, 0, sectionIndex));

        cwTrip* deepTrip = new cwTrip();
        deepTrip->setName(QStringLiteral("Topo 3"));
        section->addTrip(deepTrip);

        REQUIRE(inserted.count() == 2);
        CHECK(inserted.at(1).at(0).value<QModelIndex>() == sectionIndex);
        CHECK(inserted.at(1).at(1).toInt() == 2); //After the child node and the first trip
        CHECK(model.trip(model.index(2, 0, sectionIndex)) == deepTrip);
    }
}

TEST_CASE("cwRegionTreeModel removes a nested node's rows and brings them back on undo",
          "[cwRegionTreeModel]") {
    cwCavingRegion region;
    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwCave* folder = new cwCave();
    folder->setName(QStringLiteral("Upper Level"));
    cave->addNode(folder);

    cwTrip* folderTrip = new cwTrip();
    folderTrip->setName(QStringLiteral("Topo 1"));
    folder->addTrip(folderTrip);

    cwRegionTreeModel model;
    model.setCavingRegion(&region);

    const QModelIndex caveIndex = model.index(cave);
    REQUIRE(model.rowCount(caveIndex) == 1);
    REQUIRE(model.rowCount(model.index(folder)) == 1);

    QSignalSpy aboutToRemove(&model, &QAbstractItemModel::rowsAboutToBeRemoved);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy aboutToInsert(&model, &QAbstractItemModel::rowsAboutToBeInserted);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

    cave->removeNode(0);

    //The trip row goes out before the node row that holds it
    REQUIRE(aboutToRemove.count() == 2);
    CHECK(removed.count() == aboutToRemove.count());
    CHECK(aboutToRemove.at(1).at(0).value<QModelIndex>() == caveIndex);
    CHECK(model.rowCount(caveIndex) == 0);

    //A node kept for undo still points at the parent it came from, but neither
    //it nor the trips under it owns a row any more
    CHECK(model.index(folder) == QModelIndex());
    CHECK(model.index(folderTrip) == QModelIndex());

    undoStack.undo();

    REQUIRE(aboutToInsert.count() == 2);
    CHECK(inserted.count() == aboutToInsert.count());
    REQUIRE(model.rowCount(caveIndex) == 1);

    //Undo re-inserts the same objects, so the indexes resolve again
    const QModelIndex folderIndex = model.index(0, 0, caveIndex);
    CHECK(model.node(folderIndex) == folder);
    CHECK(model.rowCount(folderIndex) == 1);
    CHECK(model.trip(model.index(0, 0, folderIndex)) == folderTrip);
}
