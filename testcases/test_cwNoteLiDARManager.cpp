// Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Qt includes
#include <QtTest/QSignalSpy>
#include <QCoreApplication>
#include <QResource>
#include <QVector3D>
#include <QUrl>
#include <QHash>
#include <QElapsedTimer>
#include <QEventLoop>

#include <algorithm>

// Project includes
#include "cwRootData.h"
#include "LoadProjectHelper.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNoteLiDAR.h"
#include "cwNoteLiDARManager.h"
#include "cwNoteLiDARTransformation.h"
#include "cwNoteTranformation.h"
#include "cwRegionSceneManager.h"
#include "cwScene.h"
#include "cwSceneVisibility.h"
#include "cwRenderTexturedItems.h"
#include "cwCavingRegion.h"
#include "GeoreferenceFixtureHelper.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStationPositionLookup.h"
#include "cwJobSettings.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwNoteLiDARStation.h"
#include "cwTripCalibration.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGridConvergence.h"
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"
#include "cwMath.h"
#include "cwUpdateCoordinator.h"
#include "cwProject.h"

TEST_CASE("cwNoteLiDARManager no crash when caves cleared during triangulation", "[cwNoteLiDARManager][Issue371]")
{
    cwJobSettings::initialize();

    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    TestHelper helper;
    helper.loadProjectFromZip(root->project(), testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
    root->project()->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();
    root->linePlotManager()->waitToFinish();

    auto* region = root->region();
    REQUIRE(region != nullptr);
    REQUIRE(region->caveCount() == 1);

    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    REQUIRE(cave->tripCount() == 1);

    auto* trip = cave->trip(0);
    auto* lidarModel = trip->notesLiDAR();
    REQUIRE(lidarModel != nullptr);
    REQUIRE(cave->stationPositionLookup().positions().size() == 10);

    const QString lidarFile = helper.copyToTempDir(testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
    REQUIRE_FALSE(lidarFile.isEmpty());

    QSignalSpy rowsInsertedSpy(lidarModel, &QAbstractItemModel::rowsInserted);
    lidarModel->addFromFiles({ QUrl::fromLocalFile(lidarFile) });
    root->futureManagerModel()->waitForFinished();
    if (rowsInsertedSpy.isEmpty()) {
        rowsInsertedSpy.wait(1000);
    }

    REQUIRE(lidarModel->rowCount() == 1);
    const QModelIndex firstIndex = lidarModel->index(0, 0);
    QObject* noteObject = lidarModel->data(firstIndex, cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
    auto* note = qobject_cast<cwNoteLiDAR*>(noteObject);
    REQUIRE(note != nullptr);

    // Add stations to trigger triangulation
    const struct { const char* name; QVector3D pos; } stations[] = {
        {"6", QVector3D(0.19147f, -0.720703f, -2.15723f)},
        {"7", QVector3D(3.51028f, -0.0917969f, 5.39945f)},
        {"5", QVector3D(-3.48475f, -1.92188f, -3.38263f)}
    };

    auto* manager = root->noteLiDARManager();
    REQUIRE(manager != nullptr);

    for (const auto& s : stations) {
        cwNoteLiDARStation station;
        station.setName(QString::fromUtf8(s.name));
        station.setPositionOnNote(s.pos);
        note->addStation(station);
    }

    // Dispatch queued startRun so triangulation begins on the thread pool
    QCoreApplication::processEvents();

    // Simulate opening a new file while triangulation is in flight.
    // newProject() clears caves and the undo stack, which queues
    // deleteLater on caves/trips/notes.
    root->project()->newProject();

    // Flush deferred deletes so the old notes are destroyed while
    // the triangulation future is still in flight. Without the fix,
    // the callback accesses the freed cwNoteLiDAR* pointers (ASan
    // heap-buffer-overflow).
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    manager->waitForFinish();
    root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    CHECK(region->caveCount() == 0);
}

TEST_CASE("cwNoteLiDARManager applies declination for manual north", "[cwNoteLiDARManager]")
{
    cwTrip trip;
    cwNoteLiDAR note;
    note.setParentTrip(&trip);

    auto* transform = note.noteTransformation();
    REQUIRE(transform != nullptr);
    transform->setNorthUp(10.0);
    trip.calibrations()->setDeclinationManual(5.0);

    auto compareMatrix = [](const QMatrix4x4& actual, const QMatrix4x4& expected) {
        const float* actualData = actual.constData();
        const float* expectedData = expected.constData();
        for(int i = 0; i < 16; ++i) {
            CHECK(actualData[i] == Catch::Approx(expectedData[i]).epsilon(1e-6));
        }
    };

    note.setAutoCalculateNorth(true);
    cwNoteLiDARTransformationData adjustedData = transform->data();
    adjustedData.north = cwNoteTranformation::northAdjustedForDeclination(adjustedData.north, trip.calibrations()->declination());
    cwNoteLiDARTransformation adjustedTransform;
    adjustedTransform.setData(adjustedData);

    const cwTriangulateLiDARInData autoData = cwNoteLiDARManager::mapNoteToInData(&note, nullptr);
    compareMatrix(autoData.modelMatrix(), adjustedTransform.matrix());

    note.setAutoCalculateNorth(false);
    const cwTriangulateLiDARInData manualData = cwNoteLiDARManager::mapNoteToInData(&note, nullptr);
    compareMatrix(manualData.modelMatrix(), adjustedTransform.matrix());
}

TEST_CASE("cwNoteLiDARManager folds grid convergence into the note north (issue #628)", "[cwNoteLiDARManager]")
{
    // Georeference the cave so grid convergence is non-zero, then verify
    // mapNoteToInData removes it (in addition to declination) from the stored
    // note north to recover the geometric north used for triangulation. This
    // is the read-side mirror of cwNoteLiDAR::updateNoteTransformion (which
    // subtracts the same convergence at store time) and matches the scrap-side test in
    // test_cwScrap.cpp. Convergence is a property of the grid at the cave, so it
    // applies to a manual declination just like an auto one.
    //
    // The grid is the project's frame, which is centered on whatever anchors it
    // — so the anchor goes on a first cave and this one is offset east of it,
    // where the angle is real.
    cwCavingRegion region;
    cwGeoreferenceFixture::fixAtAnchorPoint(cwGeoreferenceFixture::addAnchorCave(&region));

    region.addCave();
    cwCave* cave = region.cave(region.caveCount() - 1);
    REQUIRE(cave != nullptr);

    auto* trip = new cwTrip(cave);
    cave->addTrip(trip);

    const double convergence =
            cwGeoreferenceFixture::fixEastOfAnchor(cave, QStringLiteral("a1"));
    REQUIRE(convergence > 0.1);

    cwNoteLiDAR note;
    note.setParentTrip(trip);

    auto* transform = note.noteTransformation();
    REQUIRE(transform != nullptr);
    transform->setNorthUp(10.0);

    trip->calibrations()->setAutoDeclination(false);
    trip->calibrations()->setDeclinationManual(5.0);
    const double declination = trip->calibrations()->declination();

    auto compareMatrix = [](const QMatrix4x4& actual, const QMatrix4x4& expected) {
        const float* actualData = actual.constData();
        const float* expectedData = expected.constData();
        for(int i = 0; i < 16; ++i) {
            CHECK(actualData[i] == Catch::Approx(expectedData[i]).epsilon(1e-6));
        }
    };

    auto matrixForNorth = [&](double north) {
        cwNoteLiDARTransformationData data = transform->data();
        data.north = north;
        cwNoteLiDARTransformation t;
        t.setData(data);
        return t.matrix();
    };

    // Read side recovers the geometric north: stored - declination + convergence.
    const double declinationOnly =
        cwNoteTranformation::northAdjustedForDeclination(transform->northUp(), declination);
    const double expectedNorth =
        cwNoteTranformation::northAdjustedForDeclination(declinationOnly, -convergence);

    const cwTriangulateLiDARInData data = cwNoteLiDARManager::mapNoteToInData(&note, nullptr);
    compareMatrix(data.modelMatrix(), matrixForNorth(expectedNorth));

    // Regression guard: a declination-only adjustment (the pre-#628 behavior)
    // differs from the emitted matrix by exactly the grid convergence.
    CHECK(cwWrapDegrees360(expectedNorth - declinationOnly)
          == Catch::Approx(convergence).margin(1e-6));
    const QMatrix4x4 emittedMatrix = data.modelMatrix();
    const QMatrix4x4 declOnlyMatrix = matrixForNorth(declinationOnly);
    const float* emitted = emittedMatrix.constData();
    const float* declOnly = declOnlyMatrix.constData();
    bool differs = false;
    for(int i = 0; i < 16; ++i) {
        if(qAbs(emitted[i] - declOnly[i]) > 1e-4f) {
            differs = true;
        }
    }
    CHECK(differs);
}

TEST_CASE("cwNoteLiDARManager triangulates LiDAR notes and keeps geometry accessible", "[cwNoteLiDARManager]")
{
    cwJobSettings::initialize();
    auto* jobSettings = cwJobSettings::instance();
    REQUIRE(jobSettings->automaticUpdate());

    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    TestHelper helper;
    helper.loadProjectFromZip(root->project(), testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
    root->project()->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();
    root->linePlotManager()->waitToFinish();

    auto* region = root->region();
    REQUIRE(region != nullptr);
    REQUIRE(region->caveCount() == 1);

    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    REQUIRE(cave->tripCount() == 1);

    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);

    auto* lidarModel = trip->notesLiDAR();
    REQUIRE(lidarModel != nullptr);

    REQUIRE(cave->stationPositionLookup().positions().size() == 10);

    const QString lidarFile = helper.copyToTempDir(testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
    REQUIRE_FALSE(lidarFile.isEmpty());

    QSignalSpy rowsInsertedSpy(lidarModel, &QAbstractItemModel::rowsInserted);
    lidarModel->addFromFiles({ QUrl::fromLocalFile(lidarFile) });
    root->futureManagerModel()->waitForFinished();
    if (rowsInsertedSpy.isEmpty()) {
        rowsInsertedSpy.wait(1000);
    }

    REQUIRE(lidarModel->rowCount() == 1);
    const QModelIndex firstIndex = lidarModel->index(0, 0);
    REQUIRE(firstIndex.isValid());

    QObject* noteObject = lidarModel->data(firstIndex, cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
    auto* note = qobject_cast<cwNoteLiDAR*>(noteObject);
    REQUIRE(note != nullptr);

    // No stations yet -> nothing to run
    CHECK(note->stations().isEmpty());
    CHECK(note->rowCount() == 0);

    auto* manager = root->noteLiDARManager();
    REQUIRE(manager != nullptr);

    manager->setKeepRenderGeometry(true);

    QSignalSpy triangulatedSpy(manager, &cwNoteLiDARManager::liDARNotesUpdated);
    manager->waitForFinish();
    root->futureManagerModel()->waitForFinished();
    CHECK(triangulatedSpy.isEmpty());

    auto* transform = note->noteTransformation();
    REQUIRE(transform != nullptr);
    CHECK(transform->scale() == Catch::Approx(1.0));
    CHECK(transform->upMode() == cwNoteLiDARTransformation::UpMode::YisUp);
    CHECK(transform->northUp() == Catch::Approx(0.0));

    struct StationInput {
        const char* name;
        QVector3D notePosition;
        int triangleFirstIndex;
    };

    const StationInput inputs[] = {
        {"6", QVector3D(0.19147f, -0.720703f, -2.15723f), 27879},
        {"7", QVector3D(3.51028f, -0.0917969f, 5.39945f), 280854},
        {"5", QVector3D(-3.48475f, -1.92188f, -3.38263f), 120834}
    };

    QHash<QString, int> triangleFirstIndexByStation;
    for (const StationInput& input : inputs) {
        cwNoteLiDARStation station;
        const QString stationName = QString::fromUtf8(input.name);
        station.setName(stationName);
        station.setPositionOnNote(input.notePosition);
        note->addStation(station);
        triangleFirstIndexByStation.insert(stationName, input.triangleFirstIndex);
    }

    manager->waitForFinish();
    root->futureManagerModel()->waitForFinished();

    // Allow queued signals to flush if any were emitted during waitForFinish
    QCoreApplication::processEvents();
    CHECK(triangulatedSpy.count() > 0);

    REQUIRE(note->stations().size() == 3);
    CHECK(transform->northUp() == Catch::Approx(272.7).margin(0.2));

    const auto renderIds = manager->renderItemIds(note);
    REQUIRE_FALSE(renderIds.isEmpty());

    auto* renderItems = root->regionSceneManager()->items();
    REQUIRE(renderItems != nullptr);

    const cwStationPositionLookup lookup = cave->stationPositionLookup();

    const QList<cwNoteLiDARStation> stations = note->stations();
    for (const StationInput& input : inputs) {
        const QString stationName = QString::fromUtf8(input.name);
        auto it = std::find_if(stations.cbegin(), stations.cend(), [&](const cwNoteLiDARStation& station) {
            return station.name() == stationName;
        });
        REQUIRE(it != stations.cend());
        const cwNoteLiDARStation& station = *it;
        REQUIRE(lookup.hasPosition(station.name()));

        const int first = triangleFirstIndexByStation.value(stationName, -1);
        REQUIRE(first >= 0);
        bool foundTriangle = false;
        QVector3D centroid;

        for (uint32_t id : renderIds) {
            if (!renderItems->hasItem(id)) {
                continue;
            }

            const auto renderItem = renderItems->item(id);
            const auto& indices = renderItem.geometry.indices();
            if (first + 2 >= indices.size()) {
                continue;
            }

            const auto* positionAttribute = renderItem.geometry.attribute(cwGeometry::Semantic::Position);
            if (positionAttribute == nullptr) {
                continue;
            }

            const uint32_t i0 = indices.at(first);
            const uint32_t i1 = indices.at(first + 1);
            const uint32_t i2 = indices.at(first + 2);

            const int vertexCount = renderItem.geometry.vertexCount();
            if (i0 >= static_cast<uint32_t>(vertexCount) ||
                i1 >= static_cast<uint32_t>(vertexCount) ||
                i2 >= static_cast<uint32_t>(vertexCount)) {
                continue;
            }

            const QVector3D v0 = renderItem.geometry.value<QVector3D>(positionAttribute, static_cast<int>(i0));
            const QVector3D v1 = renderItem.geometry.value<QVector3D>(positionAttribute, static_cast<int>(i1));
            const QVector3D v2 = renderItem.geometry.value<QVector3D>(positionAttribute, static_cast<int>(i2));

            centroid = (v0 + v1 + v2) / 3.0f;
            foundTriangle = true;
            break;
        }

        REQUIRE(foundTriangle);

        const QVector3D stationWorld = lookup.position(station.name());
        const float distance = (centroid - stationWorld).length();
        CHECK(distance < 0.03f); //Only 3cm off
    }
}

TEST_CASE("cwNoteLiDARManager reuses render ids when re-triangulating a note", "[cwNoteLiDARManager]")
{
    cwJobSettings::initialize();

    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    TestHelper helper;
    helper.loadProjectFromZip(root->project(), testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
    root->project()->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();
    root->linePlotManager()->waitToFinish();

    auto* cave = root->region()->cave(0);
    REQUIRE(cave != nullptr);
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    auto* lidarModel = trip->notesLiDAR();
    REQUIRE(lidarModel != nullptr);
    REQUIRE(cave->stationPositionLookup().positions().size() == 10);

    const QString lidarFile = helper.copyToTempDir(testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
    REQUIRE_FALSE(lidarFile.isEmpty());

    QSignalSpy rowsInsertedSpy(lidarModel, &QAbstractItemModel::rowsInserted);
    lidarModel->addFromFiles({ QUrl::fromLocalFile(lidarFile) });
    root->futureManagerModel()->waitForFinished();
    if (rowsInsertedSpy.isEmpty()) {
        rowsInsertedSpy.wait(1000);
    }
    REQUIRE(lidarModel->rowCount() == 1);

    auto* note = qobject_cast<cwNoteLiDAR*>(
        lidarModel->data(lidarModel->index(0, 0), cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>());
    REQUIRE(note != nullptr);

    const struct { const char* name; QVector3D pos; } stations[] = {
        {"6", QVector3D(0.19147f, -0.720703f, -2.15723f)},
        {"7", QVector3D(3.51028f, -0.0917969f, 5.39945f)},
        {"5", QVector3D(-3.48475f, -1.92188f, -3.38263f)}
    };
    for (const auto& s : stations) {
        cwNoteLiDARStation station;
        station.setName(QString::fromUtf8(s.name));
        station.setPositionOnNote(s.pos);
        note->addStation(station);
    }

    auto* manager = root->noteLiDARManager();
    REQUIRE(manager != nullptr);

    auto settle = [&]() {
        root->linePlotManager()->waitToFinish();
        manager->waitForFinish();
        root->futureManagerModel()->waitForFinished();
        QCoreApplication::processEvents();
    };

    settle();

    const QVector<uint32_t> idsBefore = manager->renderItemIds(note);
    REQUIRE_FALSE(idsBefore.isEmpty());

    // A declination change re-runs the line plot and re-triangulates the note
    // into the same number of items. The manager must update those items in
    // place, reusing their render ids rather than removing and re-adding them
    // (which would mint fresh monotonic ids and churn the picker/visibility
    // bindings). Reusing ids is also what lets the render queue coalesce
    // repeated edits and keeps memory bounded (issue #629).
    QSignalSpy updatedSpy(manager, &cwNoteLiDARManager::liDARNotesUpdated);
    trip->calibrations()->setDeclinationManual(5.0);
    settle();

    CHECK(updatedSpy.count() > 0); // the note actually re-triangulated

    const QVector<uint32_t> idsAfter = manager->renderItemIds(note);
    REQUIRE(idsAfter == idsBefore);

    auto* renderItems = root->regionSceneManager()->items();
    REQUIRE(renderItems != nullptr);
    for (uint32_t id : idsAfter) {
        CHECK(renderItems->hasItem(id));
    }
}

TEST_CASE("Each LiDAR note reaches the render items as it finishes", "[cwNoteLiDARManager]")
{
    // Notes are handed to the renderer one at a time, as each note's own
    // triangulation finishes, so the 3d view fills in note by note. Delivering
    // the whole batch at the end left the view empty for the length of the run
    // and then synchronized everything in one frame.
    cwJobSettings::initialize();

    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    TestHelper helper;
    helper.loadProjectFromZip(root->project(), testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
    root->project()->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();
    root->linePlotManager()->waitToFinish();

    auto* cave = root->region()->cave(0);
    REQUIRE(cave != nullptr);
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    auto* lidarModel = trip->notesLiDAR();
    REQUIRE(lidarModel != nullptr);

    // Two notes off the same scan: one note can't show the difference between
    // per-note and end-of-batch delivery.
    constexpr int kNoteCount = 2;
    QList<QUrl> lidarFiles;
    for (int i = 0; i < kNoteCount; i++) {
        const QString lidarFile = helper.copyToTempDir(testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
        REQUIRE_FALSE(lidarFile.isEmpty());
        lidarFiles.append(QUrl::fromLocalFile(lidarFile));
    }

    QSignalSpy rowsInsertedSpy(lidarModel, &QAbstractItemModel::rowsInserted);
    lidarModel->addFromFiles(lidarFiles);
    root->futureManagerModel()->waitForFinished();
    if (rowsInsertedSpy.isEmpty()) {
        rowsInsertedSpy.wait(1000);
    }
    REQUIRE(lidarModel->rowCount() == kNoteCount);

    auto* manager = root->noteLiDARManager();
    REQUIRE(manager != nullptr);

    QList<cwNoteLiDAR*> notes;
    for (int row = 0; row < lidarModel->rowCount(); row++) {
        auto* note = qobject_cast<cwNoteLiDAR*>(
            lidarModel->data(lidarModel->index(row, 0), cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>());
        REQUIRE(note != nullptr);
        CHECK(manager->renderItemIds(note).isEmpty());
        notes.append(note);
    }

    const struct { const char* name; QVector3D pos; } stationInputs[] = {
        {"6", QVector3D(0.19147f, -0.720703f, -2.15723f)},
        {"7", QVector3D(3.51028f, -0.0917969f, 5.39945f)},
        {"5", QVector3D(-3.48475f, -1.92188f, -3.38263f)}
    };

    // Station every note before the event loop runs again, so the restarter's
    // queued start covers them all in one batch.
    for (cwNoteLiDAR* note : std::as_const(notes)) {
        for (const auto& stationInput : stationInputs) {
            cwNoteLiDARStation station;
            station.setName(QString::fromUtf8(stationInput.name));
            station.setPositionOnNote(stationInput.pos);
            note->addStation(station);
        }
    }

    const auto deliveredNoteCount = [&]() {
        int count = 0;
        for (cwNoteLiDAR* note : std::as_const(notes)) {
            if (!manager->renderItemIds(note).isEmpty()) {
                count++;
            }
        }
        return count;
    };

    // liDARNotesUpdated fires once the batch's completion handler has run, so
    // everything observed before it is mid-batch.
    QSignalSpy batchFinishedSpy(manager, &cwNoteLiDARManager::liDARNotesUpdated);

    // A delivered note stays hidden until the intersecter publishes a BVH that
    // contains it, so "delivered mid-batch" only fills the 3d view in if the
    // gate opens mid-batch too (issue #671).
    auto* scene = root->regionSceneManager()->scene();
    REQUIRE(scene != nullptr);
    auto* renderItems = root->regionSceneManager()->items();
    REQUIRE(renderItems != nullptr);
    const auto renderObjectId = renderItems->renderObjectId();

    constexpr int kBatchTimeoutMs = 120000;
    constexpr int kPollWaitMs = 2;
    bool sawPartialDelivery = false;
    bool sawVisibleMidBatch = false;
    QElapsedTimer batchTimer;
    batchTimer.start();
    while (batchFinishedSpy.isEmpty() && batchTimer.elapsed() < kBatchTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents,
                                        kPollWaitMs);

        const int delivered = deliveredNoteCount();
        if (delivered > 0 && delivered < notes.size()) {
            sawPartialDelivery = true;
        }

        // Only count a gate that opened while notes were still undelivered in
        // this same pass, so a gate opening alongside the batch's completion
        // handler stays out of the mid-batch tally.
        if (delivered < notes.size()) {
            const auto snapshot = scene->visibility()->snapshot();
            for (cwNoteLiDAR* note : std::as_const(notes)) {
                const QVector<uint32_t> ids = manager->renderItemIds(note);
                for (uint32_t id : ids) {
                    if (snapshot.subVisible(renderObjectId, id)) {
                        sawVisibleMidBatch = true;
                    }
                }
            }
        }
    }

    root->linePlotManager()->waitToFinish();
    manager->waitForFinish();
    root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // Every note ended up delivered, exactly one set of render items each.
    CHECK(deliveredNoteCount() == notes.size());
    CHECK(sawPartialDelivery);

    // The gate opened for at least one note while the batch was still running.
    CHECK(sawVisibleMidBatch);
}

TEST_CASE("A LiDAR note deleted mid-run never reaches the render items", "[cwNoteLiDARManager]")
{
    // Per-note delivery hands each result to the renderer while the batch is
    // still running, so a note removed after the batch started can have its
    // result arrive after the note is gone. deliverNote drops it.
    cwJobSettings::initialize();

    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    TestHelper helper;
    helper.loadProjectFromZip(root->project(), testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
    root->project()->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();
    root->linePlotManager()->waitToFinish();

    auto* cave = root->region()->cave(0);
    REQUIRE(cave != nullptr);
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    auto* lidarModel = trip->notesLiDAR();
    REQUIRE(lidarModel != nullptr);

    constexpr int kNoteCount = 2;
    QList<QUrl> lidarFiles;
    for (int i = 0; i < kNoteCount; i++) {
        const QString lidarFile = helper.copyToTempDir(testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
        REQUIRE_FALSE(lidarFile.isEmpty());
        lidarFiles.append(QUrl::fromLocalFile(lidarFile));
    }

    QSignalSpy rowsInsertedSpy(lidarModel, &QAbstractItemModel::rowsInserted);
    lidarModel->addFromFiles(lidarFiles);
    root->futureManagerModel()->waitForFinished();
    if (rowsInsertedSpy.isEmpty()) {
        rowsInsertedSpy.wait(1000);
    }
    REQUIRE(lidarModel->rowCount() == kNoteCount);

    auto* manager = root->noteLiDARManager();
    REQUIRE(manager != nullptr);

    QList<cwNoteLiDAR*> notes;
    for (int row = 0; row < lidarModel->rowCount(); row++) {
        auto* note = qobject_cast<cwNoteLiDAR*>(
            lidarModel->data(lidarModel->index(row, 0), cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>());
        REQUIRE(note != nullptr);
        notes.append(note);
    }

    const struct { const char* name; QVector3D pos; } stationInputs[] = {
        {"6", QVector3D(0.19147f, -0.720703f, -2.15723f)},
        {"7", QVector3D(3.51028f, -0.0917969f, 5.39945f)},
        {"5", QVector3D(-3.48475f, -1.92188f, -3.38263f)}
    };

    for (cwNoteLiDAR* note : std::as_const(notes)) {
        for (const auto& stationInput : stationInputs) {
            cwNoteLiDARStation station;
            station.setName(QString::fromUtf8(stationInput.name));
            station.setPositionOnNote(stationInput.pos);
            note->addStation(station);
        }
    }

    // Let the queued start dispatch, so the removal below lands while the
    // triangulation of both notes is in flight.
    constexpr int kRunStartTimeoutMs = 30000;
    constexpr int kPollWaitMs = 2;
    QElapsedTimer startTimer;
    startTimer.start();
    while (manager->updateState() != cwUpdatable::State::Working
           && startTimer.elapsed() < kRunStartTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents,
                                        kPollWaitMs);
    }
    REQUIRE(manager->updateState() == cwUpdatable::State::Working);

    // The pointer outlives the row: removeNote() defers the delete, and the
    // manager only ever uses it as a hash key.
    cwNoteLiDAR* removedNote = notes.at(1);
    lidarModel->removeNote(1);

    root->linePlotManager()->waitToFinish();
    manager->waitForFinish();
    root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    CHECK(lidarModel->rowCount() == kNoteCount - 1);
    CHECK_FALSE(manager->renderItemIds(notes.at(0)).isEmpty());
    // The removal cleared the note's render ids; a delivery after it would put
    // them back — and leave items in the scene for a note that no longer exists.
    CHECK(manager->renderItemIds(removedNote).isEmpty());

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

TEST_CASE("A restarted LiDAR batch delivers only the new run's results", "[cwNoteLiDARManager]")
{
    // Restarting swaps in a fresh watcher, so the abandoned run's per-note
    // deliveries never fire. If a stale result landed after the new run's, the
    // note would be left showing geometry built from the superseded transform.
    cwJobSettings::initialize();

    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    TestHelper helper;
    helper.loadProjectFromZip(root->project(), testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
    root->project()->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();
    root->linePlotManager()->waitToFinish();

    auto* cave = root->region()->cave(0);
    REQUIRE(cave != nullptr);
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    auto* lidarModel = trip->notesLiDAR();
    REQUIRE(lidarModel != nullptr);

    const QString lidarFile = helper.copyToTempDir(testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
    REQUIRE_FALSE(lidarFile.isEmpty());

    QSignalSpy rowsInsertedSpy(lidarModel, &QAbstractItemModel::rowsInserted);
    lidarModel->addFromFiles({ QUrl::fromLocalFile(lidarFile) });
    root->futureManagerModel()->waitForFinished();
    if (rowsInsertedSpy.isEmpty()) {
        rowsInsertedSpy.wait(1000);
    }
    REQUIRE(lidarModel->rowCount() == 1);

    auto* note = qobject_cast<cwNoteLiDAR*>(
        lidarModel->data(lidarModel->index(0, 0), cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>());
    REQUIRE(note != nullptr);

    auto* manager = root->noteLiDARManager();
    REQUIRE(manager != nullptr);
    manager->setKeepRenderGeometry(true);

    const struct { const char* name; QVector3D pos; } stationInputs[] = {
        {"6", QVector3D(0.19147f, -0.720703f, -2.15723f)},
        {"7", QVector3D(3.51028f, -0.0917969f, 5.39945f)},
        {"5", QVector3D(-3.48475f, -1.92188f, -3.38263f)}
    };
    for (const auto& stationInput : stationInputs) {
        cwNoteLiDARStation station;
        station.setName(QString::fromUtf8(stationInput.name));
        station.setPositionOnNote(stationInput.pos);
        note->addStation(station);
    }

    auto settle = [&]() {
        root->linePlotManager()->waitToFinish();
        manager->waitForFinish();
        root->futureManagerModel()->waitForFinished();
        QCoreApplication::processEvents();
    };

    settle();

    auto* renderItems = root->regionSceneManager()->items();
    REQUIRE(renderItems != nullptr);

    // The first vertex of every item is enough to tell one run's morphed
    // geometry from another's.
    const auto geometrySignature = [&]() {
        QList<QVector3D> signature;
        for (uint32_t id : manager->renderItemIds(note)) {
            REQUIRE(renderItems->hasItem(id));
            const auto item = renderItems->item(id);
            const auto* positionAttribute = item.geometry.attribute(cwGeometry::Semantic::Position);
            REQUIRE(positionAttribute != nullptr);
            REQUIRE(item.geometry.vertexCount() > 0);
            signature.append(item.geometry.value<QVector3D>(positionAttribute, 0));
        }
        return signature;
    };

    const QList<QVector3D> zeroDeclinationSignature = geometrySignature();
    REQUIRE_FALSE(zeroDeclinationSignature.isEmpty());

    // Start a run at one declination and restart it at another while the first
    // is still triangulating.
    constexpr double kSupersededDeclination = 30.0;
    trip->calibrations()->setDeclinationManual(kSupersededDeclination);

    constexpr int kRunStartTimeoutMs = 30000;
    constexpr int kPollWaitMs = 2;
    QElapsedTimer startTimer;
    startTimer.start();
    while (manager->updateState() != cwUpdatable::State::Working
           && startTimer.elapsed() < kRunStartTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents,
                                        kPollWaitMs);
    }
    REQUIRE(manager->updateState() == cwUpdatable::State::Working);

    trip->calibrations()->setDeclinationManual(0.0);
    settle();

    // Back where it started: the abandoned 30° run contributed nothing.
    CHECK(geometrySignature() == zeroDeclinationSignature);

    // And the check has teeth — that declination really does move the geometry.
    trip->calibrations()->setDeclinationManual(kSupersededDeclination);
    settle();
    CHECK(geometrySignature() != zeroDeclinationSignature);
}

TEST_CASE("Deleting the last dirty LiDAR note announces the pipeline is clean",
          "[cwNoteLiDARManager]")
{
    // Mirrors cwScrapManager: removing a note drops it from the dirty set, which
    // can take the pipeline Dirty -> Clean. The coordinator only re-reads its
    // staleness aggregate when a transition is announced, so without that the
    // footer keeps offering Run for a note that no longer exists.
    cwJobSettings::initialize();

    auto root = std::make_unique<cwRootData>();

    TestHelper helper;
    helper.loadProjectFromZip(root->project(),
                              testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
    root->project()->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();
    root->linePlotManager()->waitToFinish();

    REQUIRE(root->region()->caveCount() == 1);
    auto* cave = root->region()->cave(0);
    REQUIRE(cave->tripCount() == 1);

    auto* lidarModel = cave->trip(0)->notesLiDAR();
    REQUIRE(lidarModel != nullptr);

    const QString lidarFile = helper.copyToTempDir(testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
    REQUIRE_FALSE(lidarFile.isEmpty());

    QSignalSpy rowsInsertedSpy(lidarModel, &QAbstractItemModel::rowsInserted);
    lidarModel->addFromFiles({ QUrl::fromLocalFile(lidarFile) });
    root->futureManagerModel()->waitForFinished();
    if (rowsInsertedSpy.isEmpty()) {
        rowsInsertedSpy.wait(1000);
    }
    REQUIRE(lidarModel->rowCount() == 1);

    QObject* noteObject = lidarModel->data(lidarModel->index(0, 0),
                                           cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
    auto* note = qobject_cast<cwNoteLiDAR*>(noteObject);
    REQUIRE(note != nullptr);

    // One station is all it takes for the note to be worth running.
    cwNoteLiDARStation station;
    station.setName(QStringLiteral("6"));
    station.setPositionOnNote(QVector3D(0.19147f, -0.720703f, -2.15723f));
    note->addStation(station);

    auto* manager = root->noteLiDARManager();
    auto* coordinator = root->updateCoordinator();
    REQUIRE(manager != nullptr);

    // Settle to Clean so the dirty set holds exactly what this test puts in it.
    manager->waitForFinish();
    root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    REQUIRE(manager->updateState() == cwUpdatable::State::Clean);

    coordinator->setAutomaticUpdate(false);

    // Dirty the note without running it: with automatic update off the coordinator
    // owns the run decision, which is the state the footer's Run offer is built on.
    cwNoteLiDARStation second;
    second.setName(QStringLiteral("7"));
    second.setPositionOnNote(QVector3D(3.51028f, -0.0917969f, 5.39945f));
    note->addStation(second);

    REQUIRE(manager->updateState() == cwUpdatable::State::Dirty);
    REQUIRE(coordinator->needsUpdate());

    QSignalSpy pipelineSpy(manager, &cwNoteLiDARManager::updateStateChanged);
    QSignalSpy aggregateSpy(coordinator, &cwUpdateCoordinator::needsUpdateChanged);

    // The announcement is synchronous, from liDARRowsAboutToBeRemoved: that
    // handler drops the note from the dirty set and disconnects it, so the
    // note's own destroyed() is never what reports this. removeNote uses
    // deleteLater, and plain processEvents() doesn't flush deferred deletes, so
    // ask for them explicitly to get the note actually destroyed inside the test.
    lidarModel->removeNote(0);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    CHECK(manager->updateState() == cwUpdatable::State::Clean);
    CHECK(pipelineSpy.count() == 1);
    CHECK_FALSE(coordinator->needsUpdate());
    CHECK(aggregateSpy.count() == 1);
}

namespace {
    //What one "Triangulating LiDAR notes" row said while it was alive, read
    //through the roles a person sees in the task list.
    struct LiDARRunObservation {
        QList<int> progress;
        QList<int> steps;
        QSet<QString> detailNames;

        int distinctProgressCount() const { return QSet<int>(progress.begin(), progress.end()).size(); }
    };

    //Records every LiDAR run the model announces, one entry per row. Nothing
    //test-only lives on the manager: this is the same data the task list draws.
    class LiDARRunRecorder
    {
    public:
        LiDARRunRecorder(cwFutureManagerModel* model) :
            m_model(model)
        {
            QObject::connect(model, &QAbstractItemModel::rowsInserted, &m_context,
                             [this](const QModelIndex&, int first, int last)
            {
                for(int row = first; row <= last; row++) {
                    if(isLiDARRow(row)) {
                        m_open.append({QPersistentModelIndex(m_model->index(row)),
                                       LiDARRunObservation()});
                    }
                }
            });

            QObject::connect(model, &QAbstractItemModel::dataChanged, &m_context,
                             [this](const QModelIndex& topLeft,
                                    const QModelIndex& bottomRight,
                                    const QList<int>&)
            {
                for(int row = topLeft.row(); row <= bottomRight.row(); row++) {
                    sample(row);
                }
            });

            QObject::connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, &m_context,
                             [this](const QModelIndex&, int first, int last)
            {
                for(int row = first; row <= last; row++) {
                    //The last thing the row says before it goes
                    sample(row);
                    close(m_model->index(row));
                }
            });
        }

        const QList<LiDARRunObservation>& runs() const { return m_runs; }

        //The run that had the most to say — the cold one, when a test does a
        //cold run and a warm one.
        LiDARRunObservation richestRun() const
        {
            LiDARRunObservation richest;
            for(const auto& run : m_runs) {
                if(run.progress.size() > richest.progress.size()) {
                    richest = run;
                }
            }
            return richest;
        }

    private:
        bool isLiDARRow(int row) const
        {
            return m_model->data(m_model->index(row), cwFutureManagerModel::NameRole).toString()
                   == QStringLiteral("Triangulating LiDAR notes");
        }

        LiDARRunObservation* openRun(const QModelIndex& modelIndex)
        {
            for(auto& entry : m_open) {
                if(entry.first == modelIndex) {
                    return &entry.second;
                }
            }
            return nullptr;
        }

        void sample(int row)
        {
            const QModelIndex modelIndex = m_model->index(row);
            LiDARRunObservation* run = openRun(modelIndex);
            if(run == nullptr) {
                return;
            }

            run->progress.append(m_model->data(modelIndex, cwFutureManagerModel::ProgressRole).toInt());
            run->steps.append(m_model->data(modelIndex, cwFutureManagerModel::NumberOfStepRole).toInt());

            const QString detail = m_model->data(modelIndex, cwFutureManagerModel::DetailNameRole).toString();
            if(!detail.isEmpty()) {
                run->detailNames.insert(detail);
            }
        }

        void close(const QModelIndex& modelIndex)
        {
            for(int i = 0; i < m_open.size(); i++) {
                if(m_open.at(i).first == modelIndex) {
                    m_runs.append(m_open.at(i).second);
                    m_open.removeAt(i);
                    return;
                }
            }
        }

        cwFutureManagerModel* m_model;
        QList<QPair<QPersistentModelIndex, LiDARRunObservation>> m_open;
        QList<LiDARRunObservation> m_runs;

        //Owns the model connections: they go when the recorder does
        QObject m_context;
    };

    //Pumps the event loop until the pipeline is done and its row has gone. The
    //detail line is polled on a timer, so a run has to be watched, never waited
    //out in a nested loop.
    void pumpUntilLiDARSettles(cwRootData* rootData, cwNoteLiDARManager* manager)
    {
        constexpr int kRunTimeoutMs = 120000;
        constexpr int kPollWaitMs = 2;

        QElapsedTimer timer;
        timer.start();
        while(timer.elapsed() < kRunTimeoutMs
              && (manager->updateState() != cwUpdatable::State::Clean
                  || rootData->futureManagerModel()->rowCount() > 0)) {
            QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents,
                                            kPollWaitMs);
        }
    }

    //A loaded project with one freshly copied scan on its only LiDAR note: what
    //both runs below start from. The copy keeps the texture cache cold, so the
    //first run of a test is the one that compresses.
    class LiDARProject
    {
    public:
        LiDARProject()
        {
            cwJobSettings::initialize();
            REQUIRE(cwJobSettings::instance()->automaticUpdate());

            m_rootData = std::make_unique<cwRootData>();

            m_helper.loadProjectFromZip(m_rootData->project(),
                                        testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
            m_rootData->project()->waitLoadToFinish();
            m_rootData->futureManagerModel()->waitForFinished();
            m_rootData->linePlotManager()->waitToFinish();

            m_cave = m_rootData->region()->cave(0);
            REQUIRE(m_cave != nullptr);
            auto* trip = m_cave->trip(0);
            REQUIRE(trip != nullptr);
            auto* lidarModel = trip->notesLiDAR();
            REQUIRE(lidarModel != nullptr);

            const QString lidarFile =
                m_helper.copyToTempDir(testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
            REQUIRE_FALSE(lidarFile.isEmpty());

            lidarModel->addFromFiles({ QUrl::fromLocalFile(lidarFile) });
            m_rootData->futureManagerModel()->waitForFinished();
            REQUIRE(lidarModel->rowCount() == 1);

            m_note = qobject_cast<cwNoteLiDAR*>(
                lidarModel->data(lidarModel->index(0, 0), cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>());
            REQUIRE(m_note != nullptr);
            REQUIRE(manager() != nullptr);
        }

        cwCave* cave() const { return m_cave; }
        cwNoteLiDAR* note() const { return m_note; }
        cwNoteLiDARManager* manager() const { return m_rootData->noteLiDARManager(); }
        cwFutureManagerModel* futureManagerModel() const { return m_rootData->futureManagerModel(); }

        void pumpUntilSettled() const { pumpUntilLiDARSettles(m_rootData.get(), manager()); }

    private:
        //The project's temp folder outlives the data that reads from it
        TestHelper m_helper;
        std::unique_ptr<cwRootData> m_rootData;
        cwCave* m_cave = nullptr;
        cwNoteLiDAR* m_note = nullptr;
    };
}

TEST_CASE("A LiDAR run's progress moves in steps finer than one per note",
          "[cwNoteLiDARManager][Issue671]")
{
    // The row for a LiDAR run used to hold still until the run was over, which
    // is worst for the single-note case the bar spends seconds on. The run now
    // grows a progress tree as it works, so the bar moves through the load, the
    // checksum, the morph and the textures of each note.
    LiDARProject project;
    LiDARRunRecorder recorder(project.futureManagerModel());

    const struct { const char* name; QVector3D notePosition; } inputs[] = {
        {"6", QVector3D(0.19147f, -0.720703f, -2.15723f)},
        {"7", QVector3D(3.51028f, -0.0917969f, 5.39945f)},
        {"5", QVector3D(-3.48475f, -1.92188f, -3.38263f)}
    };

    for (const auto& input : inputs) {
        cwNoteLiDARStation station;
        station.setName(QString::fromUtf8(input.name));
        station.setPositionOnNote(input.notePosition);
        project.note()->addStation(station);
    }

    project.pumpUntilSettled();

    REQUIRE_FALSE(recorder.runs().isEmpty());

    for(const auto& run : recorder.runs()) {
        INFO("Progress: " << run.progress.size() << " observations");
        REQUIRE_FALSE(run.steps.isEmpty());

        // The range is fixed for the life of the row: the bar's denominator
        // never moves under it.
        const int steps = run.steps.first();
        CHECK(steps > 0);
        CHECK(std::count(run.steps.begin(), run.steps.end(), steps) == run.steps.size());

        // Monotone: the bar stalls when an unhinted parent grows, it never
        // steps backward.
        CHECK(std::is_sorted(run.progress.begin(), run.progress.end()));
    }

    const LiDARRunObservation coldRun = recorder.richestRun();

    // Reaches full: the last thing the row said before it went was its maximum.
    CHECK(coldRun.progress.last() == coldRun.steps.last());

    // One note, and the bar still moved several times through it.
    CHECK(coldRun.distinctProgressCount() >= 3);

    // ...and the row named the step it was on. The texture encode is the only
    // step of this scan that outlives the age filter, which is the filter doing
    // its job: the load, the checksum and the morph are each over in tens of
    // milliseconds and never reach the screen.
    CHECK(coldRun.detailNames.contains(QStringLiteral("Compressing texture")));
}

TEST_CASE("A LiDAR rerun off a warm texture cache still reaches full",
          "[cwNoteLiDARManager][Issue671]")
{
    // The encode is the longest step of a cold run and the one the cache skips.
    // A rerun that finds the scan's texture already compressed grows no
    // "Compressing texture" node at all - no caller declared that step, so
    // nothing has to be kept in sync with the cache.
    LiDARProject project;

    cwNoteLiDARStation station;
    station.setName(QStringLiteral("6"));
    station.setPositionOnNote(QVector3D(0.19147f, -0.720703f, -2.15723f));
    project.note()->addStation(station);

    project.pumpUntilSettled();

    LiDARRunRecorder recorder(project.futureManagerModel());
    project.manager()->updateLiDARForCave(project.cave());
    project.pumpUntilSettled();

    REQUIRE_FALSE(recorder.runs().isEmpty());

    const LiDARRunObservation warmRun = recorder.richestRun();
    CHECK(warmRun.progress.last() == warmRun.steps.last());
    CHECK_FALSE(warmRun.detailNames.contains(QStringLiteral("Compressing texture")));
}
