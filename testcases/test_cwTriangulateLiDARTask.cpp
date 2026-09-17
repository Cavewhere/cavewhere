//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwJobSettings.h"
#include "cwTriangulateLiDARTask.h"
#include "cwTriangulateLiDARInData.h"
#include "cwNoteLiDARStation.h"
#include "cwStationPositionLookup.h"
#include "cwProgressNode.h"
#include "LoadProjectHelper.h"
#include "asyncfuture.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QVector3D>

//Std includes
#include <algorithm>

namespace {

//One scan, with two stations the morph can lean on.
cwTriangulateLiDARInData liDARInData(const QString& gltfPath, const QString& dataRootPath)
{
    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("a1"), QVector3D(0.0f, 0.0f, 0.0f));
    lookup.setPosition(QStringLiteral("a2"), QVector3D(3.0f, 1.0f, 0.0f));

    QList<cwNoteLiDARStation> noteStations;
    for(const auto& entry : {std::pair<const char*, QVector3D>{"a1", QVector3D(0.0f, 0.0f, 0.0f)},
                             std::pair<const char*, QVector3D>{"a2", QVector3D(2.0f, 1.0f, 0.0f)}}) {
        cwNoteLiDARStation station;
        station.setName(QString::fromUtf8(entry.first));
        station.setPositionOnNote(entry.second);
        noteStations.append(station);
    }

    cwTriangulateLiDARInData data;
    data.setGltfFilename(gltfPath);
    data.setDataRootPath(dataRootPath);
    data.setStationLookup(lookup);
    data.setNoteStations(noteStations);

    return data;
}

}

TEST_CASE("cwTriangulateLiDARTask returns one future per input, in input order", "[cwTriangulateLiDARTask]")
{
    cwJobSettings::initialize();

    //The middle input has no station lookup, so it is the one that errors out
    const int errorIndex = 1;

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("a1"), QVector3D(0.0f, 0.0f, 0.0f));

    QList<cwTriangulateLiDARInData> inputs;
    for(int i = 0; i < 3; i++) {
        cwTriangulateLiDARInData data;
        if(i != errorIndex) {
            data.setStationLookup(lookup);
        }
        data.setGltfFilename(QStringLiteral("missing-scan-%1.gltf").arg(i));
        inputs.append(data);
    }

    const auto futures = cwTriangulateLiDARTask::triangulate(inputs);
    REQUIRE(futures.size() == inputs.size());

    for(int i = 0; i < futures.size(); i++) {
        auto future = futures.at(i);
        AsyncFuture::waitForFinished(future); //Test-only
        REQUIRE(future.resultCount() == 1);

        const auto result = future.result();
        if(i == errorIndex) {
            CHECK(result.hasError());
        } else {
            CHECK_FALSE(result.hasError());
        }
    }
}

TEST_CASE("A LiDAR triangulation fills the progress tree it was given",
          "[cwTriangulateLiDARTask][Issue671]")
{
    // A LiDAR run's row shows the tree the task grows: nothing here declares how
    // many steps a note takes, so the bar has to move off what the load, the
    // checksum, the morph and the textures report as they go.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString gltfPath = QDir(tempDir.path()).filePath(QStringLiteral("scan.glb"));
    REQUIRE(QFile::copy(testcasesDatasetPath("test_cwGltfLoader/test.glb"), gltfPath));

    auto root = cwProgressNode::createRoot(QStringLiteral("Triangulating LiDAR notes"));
    root->expectChildren(1);

    QList<int> values;
    QFutureWatcher<void> watcher;
    QObject::connect(&watcher, &QFutureWatcherBase::progressValueChanged,
                     &watcher, [&values](int value) { values.append(value); });
    watcher.setFuture(root->future());

    const auto futures = cwTriangulateLiDARTask::triangulate({liDARInData(gltfPath, tempDir.path())}, root);
    REQUIRE(futures.size() == 1);
    auto future = futures.first();
    REQUIRE(AsyncFuture::waitForFinished(future));
    REQUIRE(future.resultCount() == 1);
    REQUIRE_FALSE(future.result().hasError());
    CHECK_FALSE(future.result().value().isEmpty());

    // Every node the run grew has been handed back, so the root reads full...
    CHECK(root->activeChildren().isEmpty());
    CHECK(root->fraction() == Catch::Approx(1.0));

    // ...and it got there through several steps, in order, for a single note.
    CHECK(values.size() > 1);
    CHECK(std::is_sorted(values.begin(), values.end()));
    CHECK(values.last() == root->future().progressMaximum());
}

TEST_CASE("A LiDAR triangulation runs with no progress root", "[cwTriangulateLiDARTask]")
{
    // Every caller outside a tracked run passes nothing, and a null tree is a
    // tree of no-ops.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString gltfPath = QDir(tempDir.path()).filePath(QStringLiteral("scan.glb"));
    REQUIRE(QFile::copy(testcasesDatasetPath("test_cwGltfLoader/test.glb"), gltfPath));

    const auto futures = cwTriangulateLiDARTask::triangulate({liDARInData(gltfPath, tempDir.path())});
    REQUIRE(futures.size() == 1);
    auto future = futures.first();
    REQUIRE(AsyncFuture::waitForFinished(future));
    REQUIRE(future.resultCount() == 1);
    REQUIRE_FALSE(future.result().hasError());
    CHECK_FALSE(future.result().value().isEmpty());
}

TEST_CASE("A LiDAR note without station positions finishes its own node",
          "[cwTriangulateLiDARTask][Issue671]")
{
    // The early return is the error path the scope has to cover: the row would
    // otherwise wait forever on a note that never started.
    cwTriangulateLiDARInData data;
    data.setGltfFilename(QStringLiteral("no-such-scan.glb"));

    auto root = cwProgressNode::createRoot(QStringLiteral("Triangulating LiDAR notes"));
    root->expectChildren(1);

    const auto futures = cwTriangulateLiDARTask::triangulate({data}, root);
    REQUIRE(futures.size() == 1);
    auto future = futures.first();
    REQUIRE(AsyncFuture::waitForFinished(future));
    REQUIRE(future.resultCount() == 1);
    CHECK(future.result().hasError());

    CHECK(root->activeChildren().isEmpty());
    CHECK(root->fraction() == Catch::Approx(1.0));
}
