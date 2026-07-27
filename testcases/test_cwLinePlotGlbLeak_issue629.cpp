// Regression test for issue #629 — "[GLB?] Memory leak when running lineplot".
//
// Repro (the reporter's workflow): a project with a GLB LiDAR note, then step a
// trip's declination repeatedly *while the 3D View page is not visible*. Each
// declination edit re-runs the line plot, which re-triangulates every GLB note
// and pushes the morphed mesh to cwRenderTexturedItems. That render object only
// drains its pending-change queue inside cwRhiTexturedItems::synchronize(), which
// runs only when the View renders a frame. With the View hidden nothing drains
// the queue, so before the fix every edit stacked another full-mesh cwGeometry
// payload and RSS grew without bound (~137 MB per edit for this GLB).
//
// The fix coalesces cwRenderTexturedItems::m_pendingChanges per id (an Add later
// removed before a sync annihilates; updates replace rather than stack), so the
// queue stays bounded by the number of live items regardless of render cadence.
//
// This test never renders a frame — it reproduces exactly the "View hidden"
// state — and asserts that resident memory stays bounded across many edits. It
// checks growth each iteration so a regression fails fast (well under a GB)
// instead of ballooning to many GB in CI.

#include <catch2/catch_test_macros.hpp>

#include "cwRootData.h"
#include "LoadProjectHelper.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNoteLiDAR.h"
#include "cwNoteLiDARManager.h"
#include "cwNoteLiDARStation.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwLinePlotManager.h"
#include "cwFutureManagerModel.h"
#include "cwJobSettings.h"

#include <QtTest/QSignalSpy>
#include <QCoreApplication>
#include <QUrl>
#include <QVector3D>

#include <cstdio>

#if defined(__APPLE__)
#include <mach/mach.h>
#else
#include <unistd.h>
#endif

namespace {

#if defined(__APPLE__)
size_t residentBytes() {
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
        return 0;
    }
    return info.resident_size;
}
#else
size_t residentBytes() {
    FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) {
        return 0;
    }
    long pages = 0;
    long resident = 0;
    const bool ok = std::fscanf(f, "%ld %ld", &pages, &resident) == 2;
    std::fclose(f);
    if (!ok) {
        return 0;
    }
    const long pageSize = sysconf(_SC_PAGESIZE);
    return static_cast<size_t>(resident) * static_cast<size_t>(pageSize > 0 ? pageSize : 4096);
}
#endif

// The GLB triangulates to ~137 MB. A leak adds one full copy per edit; a bounded
// queue adds a fixed one-time steady-state cost the baseline absorbs. This cap
// sits far above post-warmup jitter (single-digit MB observed) yet is crossed
// within a few edits of the linear leak, so a regression fails fast.
constexpr double kMaxGrowthBytes = 400.0 * 1024.0 * 1024.0;

// Warm-up edits fold the one-time triangulation/BVH allocations into the
// baseline so the measured phase reflects per-edit growth only.
constexpr int kWarmupEdits = 4;
constexpr int kMeasuredEdits = 16;

} // namespace

TEST_CASE("issue #629: stepping declination with a GLB note does not grow memory "
          "while the 3D view is hidden",
          "[cwLinePlotManager][cwNoteLiDARManager][issue629]")
{
    cwJobSettings::initialize();

    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    TestHelper helper;
    helper.loadProjectFromZip(root->project(),
                              testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
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

    const QString lidarFile =
        helper.copyToTempDir(testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
    REQUIRE_FALSE(lidarFile.isEmpty());

    QSignalSpy rowsInsertedSpy(lidarModel, &QAbstractItemModel::rowsInserted);
    lidarModel->addFromFiles({ QUrl::fromLocalFile(lidarFile) });
    root->futureManagerModel()->waitForFinished();
    if (rowsInsertedSpy.isEmpty()) {
        rowsInsertedSpy.wait(1000);
    }
    REQUIRE(lidarModel->rowCount() == 1);

    auto* note = qobject_cast<cwNoteLiDAR*>(
        lidarModel->data(lidarModel->index(0, 0), cwSurveyNoteModelBase::NoteObjectRole)
            .value<QObject*>());
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

    // Drain the async pipeline the way the app does between edits, but never
    // render a frame — this is the "View page hidden" state that leaves the
    // render object's queue undrained.
    auto settle = [&]() {
        root->linePlotManager()->waitToFinish();
        root->noteLiDARManager()->waitForFinish();
        root->futureManagerModel()->waitForFinished();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    };

    auto* calibration = trip->calibrations();
    REQUIRE(calibration != nullptr);

    // Each distinct declination re-runs the line plot -> re-triangulates the GLB.
    auto stepDeclination = [&](int step) {
        calibration->setDeclinationManual(1.0 + 0.01 * step);
        settle();
    };

    // The note must actually triangulate into render items; otherwise the
    // pending-change queue never fills and the growth assertion below would pass
    // vacuously — guarding nothing — if the fixture's station names ever stop
    // matching the survey network.
    settle();
    REQUIRE_FALSE(root->noteLiDARManager()->renderItemIds(note).isEmpty());

    for (int i = 0; i < kWarmupEdits; ++i) {
        stepDeclination(i + 1);
    }

    const size_t baseline = residentBytes();
    REQUIRE(baseline > 0);

    for (int i = 0; i < kMeasuredEdits; ++i) {
        stepDeclination(kWarmupEdits + i + 1);

        const double growth = double(residentBytes()) - double(baseline);
        INFO("declination edit " << (i + 1) << " of " << kMeasuredEdits
             << ": RSS grew " << (growth / 1e6) << " MB above baseline");
        REQUIRE(growth < kMaxGrowthBytes);
    }
}
