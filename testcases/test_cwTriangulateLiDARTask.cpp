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
#include "TestHelper.h"
#include "cwThreadCountRestorer.h"
#include "LimitedRunTestHelper.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QThread>
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

    QObject context;
    const auto futures = cwTriangulateLiDARTask::triangulate(&context, inputs).items;
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

    QObject context;
    const auto futures = cwTriangulateLiDARTask::triangulate(&context, {liDARInData(gltfPath, tempDir.path())}, root).items;
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

    QObject context;
    const auto futures = cwTriangulateLiDARTask::triangulate(&context, {liDARInData(gltfPath, tempDir.path())}).items;
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

    QObject context;
    const auto futures = cwTriangulateLiDARTask::triangulate(&context, {data}, root).items;
    REQUIRE(futures.size() == 1);
    auto future = futures.first();
    REQUIRE(AsyncFuture::waitForFinished(future));
    REQUIRE(future.resultCount() == 1);
    CHECK(future.result().hasError());

    CHECK(root->activeChildren().isEmpty());
    CHECK(root->fraction() == Catch::Approx(1.0));
}

namespace {

constexpr int kTwoNotes = 2;

//How long the event loop stays held once the first note is done: a note that
//skips admission gets this long to start on the free pool thread
constexpr int kHoldPastFirstNoteMs = 1000;
constexpr int kHoldPollMs = 1;

using LiDARFuture = QFuture<cwTriangulateLiDARTask::Result>;

//Two scans with names of their own, so each note's progress node is told apart,
//on a one-thread pool, so the limit is one
struct TwoNoteRun {
    cwThreadCountRestorer threadCount{kSingleThread};
    QTemporaryDir tempDir;
    QObject context;
    cwProgressNodePtr root = cwProgressNode::createRoot(QStringLiteral("Triangulating LiDAR notes"));
    QList<cwTriangulateLiDARInData> inputs;
    const QString firstNote = QStringLiteral("scan-1.glb");
    const QString secondNote = QStringLiteral("scan-2.glb");

    TwoNoteRun()
    {
        REQUIRE(tempDir.isValid());
        for(const QString& name : {firstNote, secondNote}) {
            const QString gltfPath = QDir(tempDir.path()).filePath(name);
            REQUIRE(QFile::copy(testcasesDatasetPath("test_cwGltfLoader/test.glb"), gltfPath));
            inputs.append(liDARInData(gltfPath, tempDir.path()));
        }
        root->expectChildren(kTwoNotes);
    }

    bool firstNoteDone() const
    {
        return root->fraction() > 0.0 && !hasActiveChild(root, firstNote);
    }

    bool secondNoteLaunched() const
    {
        return hasActiveChild(root, secondNote) || secondNoteDone();
    }

    bool secondNoteDone() const
    {
        return root->fraction() >= 1.0 && !hasActiveChild(root, secondNote);
    }

    //Blocks the event loop until the first note's worker is done, then up to
    //kHoldPastFirstNoteMs longer, returning early once stopWhen() holds.
    //Admission runs on the context's thread, so while it is held the second
    //note waits its turn. Returns stopWhen()'s last answer.
    template <typename StopWhen>
    bool holdEventLoopPastFirstNote(StopWhen stopWhen) const
    {
        QElapsedTimer timer;
        timer.start();
        while(!firstNoteDone() && timer.elapsed() < kLimitedRunTimeoutMs) {
            QThread::msleep(kHoldPollMs);
        }
        REQUIRE(firstNoteDone());

        QElapsedTimer hold;
        hold.start();
        while(!stopWhen() && hold.elapsed() < kHoldPastFirstNoteMs) {
            QThread::msleep(kHoldPollMs);
        }
        return stopWhen();
    }
};

}

TEST_CASE("A limit of one delivers both LiDAR notes, in order, one at a time",
          "[cwTriangulateLiDARTask]")
{
    // The pool's thread count is the window's width, so on one thread a note is
    // admitted only once the one before it has settled on the context's thread.
    TwoNoteRun run;

    const auto limited = cwTriangulateLiDARTask::triangulate(&run.context, run.inputs, run.root);
    const QList<LiDARFuture> futures = limited.items;
    REQUIRE(futures.size() == kTwoNotes);

    QList<int> settledOrder;
    bool secondSettledAtFirstDelivery = false;
    bool allSettledAtFirstDelivery = false;
    AsyncFuture::observe(futures.at(0)).context(&run.context, [&]() {
        settledOrder.append(0);
        secondSettledAtFirstDelivery = isSettled(futures.at(1));
        allSettledAtFirstDelivery = limited.all.isFinished();
    });
    AsyncFuture::observe(futures.at(1)).context(&run.context, [&]() {
        settledOrder.append(1);
    });

    // A second note launched without admission finishes on the free pool
    // thread while the first note's delivery is still held back.
    run.holdEventLoopPastFirstNote([&run]() { return run.secondNoteDone(); });

    REQUIRE(waitUntil([&]() { return allSettled(futures) && limited.all.isFinished(); },
                      kLimitedRunTimeoutMs));
    REQUIRE(waitUntil([&]() { return settledOrder.size() == kTwoNotes; }));

    CHECK(settledOrder == QList<int>({0, 1}));
    CHECK_FALSE(secondSettledAtFirstDelivery);
    CHECK_FALSE(allSettledAtFirstDelivery);
    for(const LiDARFuture& future : futures) {
        REQUIRE(future.isFinished());
        REQUIRE(future.resultCount() == 1);
        REQUIRE_FALSE(future.result().hasError());
        CHECK_FALSE(future.result().value().isEmpty());
    }
    CHECK(run.root->activeChildren().isEmpty());
}

TEST_CASE("A LiDAR note is launched only when it is admitted", "[cwTriangulateLiDARTask]")
{
    // Every note's future exists before its turn, but its decode is submitted to
    // the pool only on admission. On one thread the second note's launch waits
    // until the first note's settlement reaches the context's thread, however
    // long the pool thread sits free before that.
    TwoNoteRun run;

    const QList<LiDARFuture> futures
        = cwTriangulateLiDARTask::triangulate(&run.context, run.inputs, run.root).items;
    REQUIRE(futures.size() == kTwoNotes);

    // Both futures are observable before the second is admitted.
    CHECK_FALSE(isSettled(futures.at(0)));
    CHECK_FALSE(isSettled(futures.at(1)));

    CHECK_FALSE(run.holdEventLoopPastFirstNote([&run]() { return run.secondNoteLaunched(); }));
    CHECK_FALSE(isSettled(futures.at(1)));

    REQUIRE(spinUntilSettled(futures));
    CHECK(futures.at(1).isFinished());
    REQUIRE(futures.at(1).resultCount() == 1);
    CHECK_FALSE(futures.at(1).result().hasError());
}
