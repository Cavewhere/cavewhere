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

#include <algorithm>

// Project includes
#include "cwRootData.h"
#include "LoadProjectHelper.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNoteLiDAR.h"
#include "cwNoteLiDARManager.h"
#include "cwNoteLiDARTransformation.h"
#include "cwRegionSceneManager.h"
#include "cwRenderTexturedItems.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStationPositionLookup.h"

TEST_CASE("cwNoteLiDARManager triangulates LiDAR notes and keeps geometry accessible", "[cwNoteLiDARManager]")
{
    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    TestHelper helper;
    helper.loadProjectFromZip(root->project(), QStringLiteral(":/datasets/lidarProjects/jaws of the beast.zip"));
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

    const QString lidarFile = helper.copyToTempDir(QStringLiteral(":/datasets/lidarProjects/9_15_2025 3.glb"));
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
