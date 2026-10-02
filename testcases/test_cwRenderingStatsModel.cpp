// test_cwRenderingStatsModel.cpp
// Catch2 unit tests for the QML-facing view of the render byte ledger.

#include <catch2/catch_test_macros.hpp>

#include "cwRenderCullingStats.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderingStatsModel.h"
#include "cwTextureStreamingStats.h"

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
constexpr int kCategoryCount = 5;
constexpr int kPointCloudRow = 0;
constexpr int kLinePlotRow = 3;
constexpr int kLongerThanPollInterval = 1200;
constexpr cwRenderCullingStats::Counts kCounts {
    .objectsTotal = 9,
    .objectsCulled = 4,
    .itemsTotal = 25,
    .itemsCulled = 20
};
constexpr cwTextureStreamingStats::Counts kStreamingCounts {
    .streamedItems = 12,
    .loadsInFlight = 3,
    .itemsBelowDesired = 5,
    .readyCpuBytes = kOneAndAHalfMegabytes,
    .demotionsInFlight = 2
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
    cwRenderingStatsModel model;

    QSignalSpy cullingSpy(&model, &cwRenderingStatsModel::cullingChanged);

    cwRenderCullingStats::instance()->publish(kCounts);

    model.refresh();

    CHECK(model.totalObjects() == kCounts.objectsTotal);
    CHECK(model.culledObjects() == kCounts.objectsCulled);
    CHECK(model.totalItems() == kCounts.itemsTotal);
    CHECK(model.culledItems() == kCounts.itemsCulled);
    CHECK(cullingSpy.count() == 1);

    //Without a new published frame the model stays quiet
    model.refresh();
    CHECK(cullingSpy.count() == 1);
}

TEST_CASE("cwRenderingStatsModel: refresh reads the published streaming counts",
          "[RenderingStatsModel]") {
    cwRenderingStatsModel model;

    QSignalSpy streamingSpy(&model, &cwRenderingStatsModel::streamingChanged);

    cwTextureStreamingStats::instance()->publish(kStreamingCounts);

    model.refresh();

    CHECK(model.streamedItems() == kStreamingCounts.streamedItems);
    CHECK(model.loadsInFlight() == kStreamingCounts.loadsInFlight);
    CHECK(model.itemsBelowDesired() == kStreamingCounts.itemsBelowDesired);
    CHECK(model.readyCpuBytes() == kStreamingCounts.readyCpuBytes);
    CHECK(model.readyCpuText()
          == cwRenderingStatsModel::formattedBytes(kStreamingCounts.readyCpuBytes));
    CHECK(model.demotionsInFlight() == kStreamingCounts.demotionsInFlight);
    CHECK(streamingSpy.count() == 1);

    //Without a new published frame the model stays quiet
    model.refresh();
    CHECK(streamingSpy.count() == 1);

    //A frame with nothing streaming reads back as zeros
    cwTextureStreamingStats::instance()->publish({});
    model.refresh();

    CHECK(model.streamedItems() == 0);
    CHECK(model.loadsInFlight() == 0);
    CHECK(model.itemsBelowDesired() == 0);
    CHECK(model.readyCpuBytes() == 0);
    CHECK(model.demotionsInFlight() == 0);
    CHECK(streamingSpy.count() == 2);
}

TEST_CASE("cwRenderingStatsModel: polling picks up a published streaming frame",
          "[RenderingStatsModel]") {
    cwRenderingStatsModel model;
    model.setRunning(true);

    QSignalSpy streamingSpy(&model, &cwRenderingStatsModel::streamingChanged);

    cwTextureStreamingStats::Counts counts = kStreamingCounts;
    counts.loadsInFlight = kStreamingCounts.loadsInFlight + 1;
    cwTextureStreamingStats::instance()->publish(counts);

    QTest::qWait(kLongerThanPollInterval);

    CHECK(model.loadsInFlight() == counts.loadsInFlight);
    CHECK(streamingSpy.count() == 1);
}

TEST_CASE("cwRenderingStatsModel: polling runs only while running is true",
          "[RenderingStatsModel]") {
    LedgerScope linePlotGpu(Category::LinePlotGeometry, Residency::Gpu);
    LedgerScope linePlotCpu(Category::LinePlotGeometry, Residency::Cpu);

    cwRenderingStatsModel model;
    const qint64 startGpuBytes = rowGpuBytes(model, kLinePlotRow);
    const qint64 startCpuBytes = rowCpuBytes(model, kLinePlotRow);

    QSignalSpy runningSpy(&model, &cwRenderingStatsModel::runningChanged);

    CHECK_FALSE(model.running());

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
