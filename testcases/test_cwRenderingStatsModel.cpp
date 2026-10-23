// test_cwRenderingStatsModel.cpp
// Catch2 unit tests for the QML-facing view of the render byte ledger.

#include <catch2/catch_test_macros.hpp>

#include "cwRenderFrameStats.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderingStatsModel.h"

//Qt includes
#include <QSignalSpy>
#include <QTest>

using Category = cwRenderMemoryLedger::Category;
using Residency = cwRenderMemoryLedger::Residency;

namespace {

constexpr qint64 kKilobyte = 1024;
constexpr qint64 kMegabyte = kKilobyte * kKilobyte;
constexpr qint64 kGigabyte = kMegabyte * kKilobyte;
constexpr qint64 kOneAndAHalfMegabytes = kMegabyte + kMegabyte / 2;
constexpr qint64 kTwoGigabytes = 2 * kGigabyte;
constexpr qint64 kThreeMegabytes = 3 * kMegabyte;
constexpr int kCategoryCount = 5;
constexpr int kPointCloudRow = 0;
constexpr int kLinePlotRow = 3;
constexpr int kLongerThanPollInterval = 1200;
constexpr cwRenderFrameStats::Culling kCounts {
    .objectsTotal = 9,
    .objectsCulled = 4,
    .itemsTotal = 25,
    .itemsCulled = 20
};
constexpr cwRenderFrameStats::Streaming kStreamingCounts {
    .streamedItems = 12,
    .loadsInFlight = 3,
    .itemsBelowDesired = 5,
    .readyCpuBytes = kOneAndAHalfMegabytes,
    .demotionsInFlight = 2
};
constexpr qint64 kMillion = 1000000;
constexpr cwRenderFrameStats::PointCloud kPointCloudCounts {
    .residentNodes = 41,
    .selectedNodes = 57,
    .selectedPoints = 17200000,
    .nodeLoadsInFlight = 6,
    .sseInflation = 1.25,
    .pickMirrorBytes = kThreeMegabytes
};

// The ledger is process-wide, so every test works in deltas from what it finds
// and puts the value back when it is done.
class LedgerScope
{
public:
    LedgerScope(Category category, Residency residency) :
        m_category(category),
        m_residency(residency),
        m_startBytes(cwRenderMemoryLedger::instance()->bytes(category, residency))
    {
    }

    ~LedgerScope()
    {
        const qint64 current = cwRenderMemoryLedger::instance()->bytes(m_category, m_residency);
        cwRenderMemoryLedger::instance()->adjust(m_category, m_residency, m_startBytes - current);
    }

private:
    Category m_category;
    Residency m_residency;
    qint64 m_startBytes;
};

qint64 rowGpuBytes(const cwRenderingStatsModel& model, int row)
{
    return model.data(model.index(row), cwRenderingStatsModel::GpuBytesRole).toLongLong();
}

qint64 rowCpuBytes(const cwRenderingStatsModel& model, int row)
{
    return model.data(model.index(row), cwRenderingStatsModel::CpuBytesRole).toLongLong();
}

} // namespace

TEST_CASE("cwRenderingStatsModel: formattedMillions reads a point count in millions",
          "[RenderingStatsModel]") {
    CHECK(cwRenderingStatsModel::formattedMillions(0) == QStringLiteral("0.0 M"));
    CHECK(cwRenderingStatsModel::formattedMillions(kPointCloudCounts.selectedPoints)
          == QStringLiteral("17.2 M"));
    CHECK(cwRenderingStatsModel::formattedMillions(16 * kMillion) == QStringLiteral("16.0 M"));
}

TEST_CASE("cwRenderingStatsModel: formattedBytes covers the unit boundaries",
          "[RenderingStatsModel]") {
    CHECK(cwRenderingStatsModel::formattedBytes(0) == QStringLiteral("0 B"));
    CHECK(cwRenderingStatsModel::formattedBytes(1023) == QStringLiteral("1023 B"));
    CHECK(cwRenderingStatsModel::formattedBytes(kKilobyte) == QStringLiteral("1.0 KB"));
    CHECK(cwRenderingStatsModel::formattedBytes(kOneAndAHalfMegabytes) == QStringLiteral("1.5 MB"));
    CHECK(cwRenderingStatsModel::formattedBytes(kTwoGigabytes) == QStringLiteral("2.0 GB"));
}

TEST_CASE("cwRenderingStatsModel: row count and role names match the ledger categories",
          "[RenderingStatsModel]") {
    cwRenderingStatsModel model;

    CHECK(model.rowCount() == kCategoryCount);
    CHECK(model.rowCount(model.index(0)) == 0);

    const QHash<int, QByteArray> roles = model.roleNames();
    CHECK(roles.value(cwRenderingStatsModel::NameRole) == QByteArray("name"));
    CHECK(roles.value(cwRenderingStatsModel::GpuBytesRole) == QByteArray("gpuBytes"));
    CHECK(roles.value(cwRenderingStatsModel::CpuBytesRole) == QByteArray("cpuBytes"));
    CHECK(roles.value(cwRenderingStatsModel::GpuTextRole) == QByteArray("gpuText"));
    CHECK(roles.value(cwRenderingStatsModel::CpuTextRole) == QByteArray("cpuText"));

    for(int i = 0; i < model.rowCount(); i++) {
        CHECK_FALSE(model.data(model.index(i), cwRenderingStatsModel::NameRole).toString().isEmpty());
    }
}

TEST_CASE("cwRenderingStatsModel: refresh re-reads roles and totals from the ledger",
          "[RenderingStatsModel]") {
    LedgerScope pointCloudGpu(Category::PointCloudGeometry, Residency::Gpu);
    LedgerScope textureCpu(Category::TexturedItemTexture, Residency::Cpu);

    cwRenderingStatsModel model;

    const qint64 startTotalGpu = model.totalGpuBytes();
    const qint64 startTotalCpu = model.totalCpuBytes();
    const qint64 startPointCloudGpu = rowGpuBytes(model, kPointCloudRow);

    QSignalSpy dataChangedSpy(&model, &cwRenderingStatsModel::dataChanged);
    QSignalSpy totalsSpy(&model, &cwRenderingStatsModel::totalsChanged);

    cwRenderMemoryLedger::instance()->adjust(Category::PointCloudGeometry,
                                             Residency::Gpu,
                                             kOneAndAHalfMegabytes);
    cwRenderMemoryLedger::instance()->adjust(Category::TexturedItemTexture,
                                             Residency::Cpu,
                                             kKilobyte);

    model.refresh();

    CHECK(rowGpuBytes(model, kPointCloudRow) == startPointCloudGpu + kOneAndAHalfMegabytes);
    CHECK(model.data(model.index(kPointCloudRow), cwRenderingStatsModel::GpuTextRole).toString()
          == cwRenderingStatsModel::formattedBytes(startPointCloudGpu + kOneAndAHalfMegabytes));
    CHECK(model.totalGpuBytes() == startTotalGpu + kOneAndAHalfMegabytes);
    CHECK(model.totalCpuBytes() == startTotalCpu + kKilobyte);
    CHECK(model.totalGpuText() == cwRenderingStatsModel::formattedBytes(model.totalGpuBytes()));
    CHECK(model.totalCpuText() == cwRenderingStatsModel::formattedBytes(model.totalCpuBytes()));
    CHECK(dataChangedSpy.count() == 2);
    CHECK(totalsSpy.count() == 1);

    //An unchanged ledger keeps the model quiet
    model.refresh();
    CHECK(dataChangedSpy.count() == 2);
    CHECK(totalsSpy.count() == 1);
}

TEST_CASE("cwRenderingStatsModel: refresh reads the published culling counts",
          "[RenderingStatsModel]") {
    //A known baseline so the model's construction-time snapshot differs from kCounts
    cwRenderFrameStats::instance()->publishCulling({});

    cwRenderingStatsModel model;

    QSignalSpy cullingSpy(&model, &cwRenderingStatsModel::cullingChanged);
    QSignalSpy streamingSpy(&model, &cwRenderingStatsModel::streamingChanged);

    cwRenderFrameStats::instance()->publishCulling(kCounts);

    model.refresh();

    CHECK(model.totalObjects() == kCounts.objectsTotal);
    CHECK(model.culledObjects() == kCounts.objectsCulled);
    CHECK(model.totalItems() == kCounts.itemsTotal);
    CHECK(model.culledItems() == kCounts.itemsCulled);
    CHECK(cullingSpy.count() == 1);

    //Publishing culling alone leaves the streaming half quiet
    CHECK(streamingSpy.count() == 0);

    //Without a new published frame the model stays quiet
    model.refresh();
    CHECK(cullingSpy.count() == 1);
    CHECK(streamingSpy.count() == 0);
}

TEST_CASE("cwRenderingStatsModel: refresh reads the published streaming counts",
          "[RenderingStatsModel]") {
    //A known baseline so the model's construction-time snapshot differs from kStreamingCounts
    cwRenderFrameStats::instance()->publishStreaming({});

    cwRenderingStatsModel model;

    QSignalSpy streamingSpy(&model, &cwRenderingStatsModel::streamingChanged);
    QSignalSpy cullingSpy(&model, &cwRenderingStatsModel::cullingChanged);

    cwRenderFrameStats::instance()->publishStreaming(kStreamingCounts);

    model.refresh();

    CHECK(model.streamedItems() == kStreamingCounts.streamedItems);
    CHECK(model.loadsInFlight() == kStreamingCounts.loadsInFlight);
    CHECK(model.itemsBelowDesired() == kStreamingCounts.itemsBelowDesired);
    CHECK(model.readyCpuBytes() == kStreamingCounts.readyCpuBytes);
    CHECK(model.readyCpuText()
          == cwRenderingStatsModel::formattedBytes(kStreamingCounts.readyCpuBytes));
    CHECK(model.demotionsInFlight() == kStreamingCounts.demotionsInFlight);
    CHECK(streamingSpy.count() == 1);

    //Publishing streaming alone leaves the culling half quiet
    CHECK(cullingSpy.count() == 0);

    //Without a new published frame the model stays quiet
    model.refresh();
    CHECK(streamingSpy.count() == 1);
    CHECK(cullingSpy.count() == 0);

    //A frame with nothing streaming reads back as zeros
    cwRenderFrameStats::instance()->publishStreaming({});
    model.refresh();

    CHECK(model.streamedItems() == 0);
    CHECK(model.loadsInFlight() == 0);
    CHECK(model.itemsBelowDesired() == 0);
    CHECK(model.readyCpuBytes() == 0);
    CHECK(model.demotionsInFlight() == 0);
    CHECK(streamingSpy.count() == 2);
    CHECK(cullingSpy.count() == 0);
}

TEST_CASE("cwRenderingStatsModel: refresh reads the published point cloud counts",
          "[RenderingStatsModel]") {
    //A known baseline so the model's construction-time snapshot differs from kPointCloudCounts
    cwRenderFrameStats::instance()->publishPointCloud({});

    cwRenderingStatsModel model;

    QSignalSpy pointCloudSpy(&model, &cwRenderingStatsModel::pointCloudChanged);
    QSignalSpy streamingSpy(&model, &cwRenderingStatsModel::streamingChanged);
    QSignalSpy cullingSpy(&model, &cwRenderingStatsModel::cullingChanged);

    cwRenderFrameStats::instance()->publishPointCloud(kPointCloudCounts);

    model.refresh();

    CHECK(model.residentNodes() == kPointCloudCounts.residentNodes);
    CHECK(model.selectedNodes() == kPointCloudCounts.selectedNodes);
    CHECK(model.nodeLoadsInFlight() == kPointCloudCounts.nodeLoadsInFlight);
    CHECK(model.selectedPoints() == kPointCloudCounts.selectedPoints);
    CHECK(model.selectedPointsText()
          == cwRenderingStatsModel::formattedMillions(kPointCloudCounts.selectedPoints));
    CHECK(model.sseInflation() == kPointCloudCounts.sseInflation);
    CHECK(model.pickMirrorBytes() == kPointCloudCounts.pickMirrorBytes);
    CHECK(model.pickMirrorText()
          == cwRenderingStatsModel::formattedBytes(kPointCloudCounts.pickMirrorBytes));
    CHECK(pointCloudSpy.count() == 1);

    //Publishing the point cloud alone leaves the other parts quiet
    CHECK(streamingSpy.count() == 0);
    CHECK(cullingSpy.count() == 0);

    //Without a new published frame the model stays quiet
    model.refresh();
    CHECK(pointCloudSpy.count() == 1);

    //A frame with no octree reads back as the resting values
    cwRenderFrameStats::instance()->publishPointCloud({});
    model.refresh();

    CHECK(model.residentNodes() == 0);
    CHECK(model.selectedNodes() == 0);
    CHECK(model.nodeLoadsInFlight() == 0);
    CHECK(model.selectedPoints() == 0);
    CHECK(model.sseInflation() == 1.0);
    CHECK(model.pickMirrorBytes() == 0);
    CHECK(pointCloudSpy.count() == 2);
}

TEST_CASE("cwRenderingStatsModel: polling picks up a published streaming frame",
          "[RenderingStatsModel]") {
    cwRenderingStatsModel model;
    model.setRunning(true);

    QSignalSpy streamingSpy(&model, &cwRenderingStatsModel::streamingChanged);

    cwRenderFrameStats::Streaming counts = kStreamingCounts;
    counts.loadsInFlight = kStreamingCounts.loadsInFlight + 1;
    cwRenderFrameStats::instance()->publishStreaming(counts);

    QTest::qWait(kLongerThanPollInterval);

    CHECK(model.loadsInFlight() == counts.loadsInFlight);
    CHECK(streamingSpy.count() == 1);
}

TEST_CASE("cwRenderingStatsModel: polling runs only while running is true",
          "[RenderingStatsModel]") {
    LedgerScope linePlotGpu(Category::LinePlotGeometry, Residency::Gpu);
    LedgerScope linePlotCpu(Category::LinePlotGeometry, Residency::Cpu);

    cwRenderingStatsModel model;
    CHECK_FALSE(model.running());

    const qint64 startGpuBytes = rowGpuBytes(model, kLinePlotRow);
    const qint64 startCpuBytes = rowCpuBytes(model, kLinePlotRow);

    QSignalSpy runningSpy(&model, &cwRenderingStatsModel::runningChanged);

    cwRenderMemoryLedger::instance()->adjust(Category::LinePlotGeometry,
                                             Residency::Gpu,
                                             kMegabyte);
    QTest::qWait(kLongerThanPollInterval);
    CHECK(rowGpuBytes(model, kLinePlotRow) == startGpuBytes);

    model.setRunning(true);
    CHECK(model.running());
    CHECK(runningSpy.count() == 1);
    CHECK(rowGpuBytes(model, kLinePlotRow) == startGpuBytes + kMegabyte);

    cwRenderMemoryLedger::instance()->adjust(Category::LinePlotGeometry,
                                             Residency::Cpu,
                                             kKilobyte);
    QTest::qWait(kLongerThanPollInterval);
    CHECK(rowCpuBytes(model, kLinePlotRow) == startCpuBytes + kKilobyte);

    model.setRunning(false);
    CHECK_FALSE(model.running());
    CHECK(runningSpy.count() == 2);

    cwRenderMemoryLedger::instance()->adjust(Category::LinePlotGeometry,
                                             Residency::Gpu,
                                             kMegabyte);
    QTest::qWait(kLongerThanPollInterval);
    CHECK(rowGpuBytes(model, kLinePlotRow) == startGpuBytes + kMegabyte);
}
