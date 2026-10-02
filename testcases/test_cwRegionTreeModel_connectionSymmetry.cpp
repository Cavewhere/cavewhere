// Catch includes
#include <catch2/catch_test_macros.hpp>

// Qt includes
#include <QSignalSpy>
#include <QUndoStack>
#include <QtGlobal>
#include <QStringList>

// Our includes
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNoteLiDAR.h"

namespace {

// Captures qWarning() text so a test can assert on the duplicate-connection
// message that cwUniqueConnectionChecker emits in debug builds.
QStringList g_capturedWarnings;
QtMessageHandler g_previousHandler = nullptr;

void captureWarnings(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    if(type == QtWarningMsg) {
        g_capturedWarnings.append(msg);
    }
    if(g_previousHandler != nullptr) {
        g_previousHandler(type, context, msg);
    }
}

} // namespace

// Reproduces cavewhere#576. cwRegionTreeModel::addTripConnections() registers the
// trip AND its note/lidar/sketch models in m_connectionChecker, but
// removeTripConnections() only removes the trip. Removing then re-adding the same
// trip (git checkout / sync reconcile, or an undo/redo of a trip removal) therefore
// re-adds a note model the checker still believes is connected:
//
//   QFATAL/warning: "cwSurveyNoteModel is already connected!"
//
// and, because addTripConnections `continue`s on that collision, the trip's
// note/lidar/sketch row signals are never re-wired.
TEST_CASE("cwRegionTreeModel re-adds a trip's note-model connections symmetrically (#576)",
          "[cwRegionTreeModel]") {
    QUndoStack undoStack;

    auto region = std::make_unique<cwCavingRegion>();

    // Parent the cave, then give the region (and its children) a real undo stack so
    // a trip removal is recorded as an undoable command instead of being executed
    // and deleted immediately (which would destroy the trip we want to re-add).
    auto cave = new cwCave();
    region->addCave(cave);
    region->setUndoStack(&undoStack);

    auto trip = new cwTrip();
    cave->addTrip(trip);

    cwSurveyNoteModel* notes = trip->notes();
    cwSurveyNoteLiDARModel* lidarModel = trip->notesLiDAR();
    REQUIRE(notes != nullptr);
    REQUIRE(lidarModel != nullptr);

    auto model = std::make_unique<cwRegionTreeModel>();
    model->setCavingRegion(region.get());

    REQUIRE(model->index(trip).isValid());

    // Remove the trip. The RemoveTripCommand on the undo stack keeps the trip (and
    // its note/lidar/sketch models) alive so the undo below re-adds the very same
    // objects.
    REQUIRE(cave->tripCount() == 1);
    cave->removeTrip(0);
    REQUIRE(cave->tripCount() == 0);

    // Re-add the same trip and capture anything logged while addTripConnections runs.
    g_capturedWarnings.clear();
    g_previousHandler = qInstallMessageHandler(captureWarnings);

    undoStack.undo();

    qInstallMessageHandler(g_previousHandler);
    g_previousHandler = nullptr;

    REQUIRE(cave->tripCount() == 1);
    REQUIRE(cave->trip(0) == trip);
    REQUIRE(trip->notes() == notes);
    REQUIRE(trip->notesLiDAR() == lidarModel);

    // The checker must not report the note model as already-connected. (Only fires
    // in QT_DEBUG builds, where cwUniqueConnectionChecker is active.)
    for(const QString& warning : std::as_const(g_capturedWarnings)) {
        INFO("Logged warning: " << warning.toStdString());
        CHECK_FALSE(warning.contains(QStringLiteral("already connected")));
    }

    // Build-independent consequence: the trip's LiDAR-model row signals must still
    // propagate through the tree model after the re-add. With the bug, the collision
    // makes addTripConnections skip re-wiring this connection, so no rows appear.
    QModelIndex reTripIndex = model->index(trip);
    REQUIRE(reTripIndex.isValid());

    const int lidarRow = static_cast<int>(cwRegionTreeModel::TripRows::NotesLiDARModel);
    QModelIndex lidarContainer = model->index(lidarRow, 0, reTripIndex);
    REQUIRE(lidarContainer.isValid());

    const int beforeCount = model->rowCount(lidarContainer);

    QSignalSpy insertedSpy(model.get(), &QAbstractItemModel::rowsInserted);
    REQUIRE(insertedSpy.isValid());

    auto* lidarNote = new cwNoteLiDAR(lidarModel);
    lidarNote->setFilename(QStringLiteral("scan_after_readd.las"));
    lidarModel->addNotes({lidarNote});

    CHECK(insertedSpy.size() >= 1);
    CHECK(model->rowCount(lidarContainer) == beforeCount + 1);
}
