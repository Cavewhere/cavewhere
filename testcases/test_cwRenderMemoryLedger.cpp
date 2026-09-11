// test_cwRenderMemoryLedger.cpp
// Catch2 unit tests for the process-wide render byte ledger and its RAII reporter.

#include <catch2/catch_test_macros.hpp>

#include "cwRenderMemoryLedger.h"

#include <thread>
#include <vector>

using Category = cwRenderMemoryLedger::Category;
using Residency = cwRenderMemoryLedger::Residency;

namespace {

constexpr qint64 kSmallBytes = 1024;
constexpr qint64 kMediumBytes = 4096;
constexpr qint64 kLargeBytes = 1 << 20;
constexpr int kThreadCount = 4;
constexpr int kIterationsPerThread = 10000;

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

    qint64 startBytes() const { return m_startBytes; }

    qint64 delta() const
    {
        return cwRenderMemoryLedger::instance()->bytes(m_category, m_residency) - m_startBytes;
    }

private:
    Category m_category;
    Residency m_residency;
    qint64 m_startBytes;
};

} // namespace

TEST_CASE("cwRenderMemoryLedger: adjust and bytes round-trip per category and residency",
          "[RenderMemoryLedger]") {
    auto* ledger = cwRenderMemoryLedger::instance();

    LedgerScope pointCloudGpu(Category::PointCloudGeometry, Residency::Gpu);
    LedgerScope pointCloudCpu(Category::PointCloudGeometry, Residency::Cpu);
    LedgerScope textureGpu(Category::TexturedItemTexture, Residency::Gpu);

    ledger->adjust(Category::PointCloudGeometry, Residency::Gpu, kSmallBytes);
    ledger->adjust(Category::PointCloudGeometry, Residency::Cpu, kMediumBytes);
    ledger->adjust(Category::TexturedItemTexture, Residency::Gpu, kLargeBytes);

    CHECK(pointCloudGpu.delta() == kSmallBytes);
    CHECK(pointCloudCpu.delta() == kMediumBytes);
    CHECK(textureGpu.delta() == kLargeBytes);

    ledger->adjust(Category::PointCloudGeometry, Residency::Gpu, -kSmallBytes);
    CHECK(pointCloudGpu.delta() == 0);
    CHECK(pointCloudCpu.delta() == kMediumBytes);
}

TEST_CASE("cwRenderMemoryLedger: totalBytes sums one residency across categories",
          "[RenderMemoryLedger]") {
    auto* ledger = cwRenderMemoryLedger::instance();

    LedgerScope lineGpu(Category::LinePlotGeometry, Residency::Gpu);
    LedgerScope otherGpu(Category::Other, Residency::Gpu);
    LedgerScope otherCpu(Category::Other, Residency::Cpu);

    const qint64 startGpuTotal = ledger->totalBytes(Residency::Gpu);
    const qint64 startCpuTotal = ledger->totalBytes(Residency::Cpu);

    ledger->adjust(Category::LinePlotGeometry, Residency::Gpu, kSmallBytes);
    ledger->adjust(Category::Other, Residency::Gpu, kMediumBytes);
    ledger->adjust(Category::Other, Residency::Cpu, kLargeBytes);

    CHECK(ledger->totalBytes(Residency::Gpu) == startGpuTotal + kSmallBytes + kMediumBytes);
    CHECK(ledger->totalBytes(Residency::Cpu) == startCpuTotal + kLargeBytes);
}

TEST_CASE("cwRenderMemoryLedger: underflow clamps to zero and leaves other categories alone",
          "[RenderMemoryLedger]") {
    auto* ledger = cwRenderMemoryLedger::instance();

    LedgerScope otherGpu(Category::Other, Residency::Gpu);
    LedgerScope geometryGpu(Category::TexturedItemGeometry, Residency::Gpu);

    ledger->adjust(Category::TexturedItemGeometry, Residency::Gpu, kMediumBytes);
    ledger->adjust(Category::Other, Residency::Gpu, kSmallBytes);

    ledger->adjust(Category::Other, Residency::Gpu,
                   -(otherGpu.startBytes() + kSmallBytes + kLargeBytes));

    CHECK(ledger->bytes(Category::Other, Residency::Gpu) == 0);
    CHECK(geometryGpu.delta() == kMediumBytes);
}

TEST_CASE("cwRenderMemoryLedger: revision counts adjustments only", "[RenderMemoryLedger]") {
    auto* ledger = cwRenderMemoryLedger::instance();

    LedgerScope otherCpu(Category::Other, Residency::Cpu);

    const quint64 startRevision = ledger->revision();

    ledger->adjust(Category::Other, Residency::Cpu, kSmallBytes);
    CHECK(ledger->revision() == startRevision + 1);

    ledger->adjust(Category::Other, Residency::Cpu, -kSmallBytes);
    CHECK(ledger->revision() == startRevision + 2);

    const quint64 afterAdjust = ledger->revision();
    ledger->bytes(Category::Other, Residency::Cpu);
    ledger->totalBytes(Residency::Cpu);
    CHECK(ledger->revision() == afterAdjust);
}

TEST_CASE("cwLedgeredBytes: setBytes reports the delta and destruction returns the bytes",
          "[RenderMemoryLedger]") {
    LedgerScope pointCloudGpu(Category::PointCloudGeometry, Residency::Gpu);

    {
        cwLedgeredBytes reporter(Category::PointCloudGeometry, Residency::Gpu);
        CHECK(reporter.bytes() == 0);

        reporter.setBytes(kSmallBytes);
        CHECK(reporter.bytes() == kSmallBytes);
        CHECK(pointCloudGpu.delta() == kSmallBytes);

        reporter.setBytes(kLargeBytes);
        CHECK(reporter.bytes() == kLargeBytes);
        CHECK(pointCloudGpu.delta() == kLargeBytes);
    }

    CHECK(pointCloudGpu.delta() == 0);
}

TEST_CASE("cwLedgeredBytes: moving transfers the reported bytes", "[RenderMemoryLedger]") {
    LedgerScope lineGpu(Category::LinePlotGeometry, Residency::Gpu);

    {
        cwLedgeredBytes source(Category::LinePlotGeometry, Residency::Gpu);
        source.setBytes(kMediumBytes);

        {
            cwLedgeredBytes moved(std::move(source));
            CHECK(moved.bytes() == kMediumBytes);
            CHECK(source.bytes() == 0);
            CHECK(lineGpu.delta() == kMediumBytes);
        }

        CHECK(lineGpu.delta() == 0);
    }

    CHECK(lineGpu.delta() == 0);
}

TEST_CASE("cwLedgeredBytes: move assignment returns the target's own bytes first",
          "[RenderMemoryLedger]") {
    LedgerScope lineGpu(Category::LinePlotGeometry, Residency::Gpu);

    {
        cwLedgeredBytes source(Category::LinePlotGeometry, Residency::Gpu);
        source.setBytes(kMediumBytes);

        cwLedgeredBytes target(Category::LinePlotGeometry, Residency::Gpu);
        target.setBytes(kSmallBytes);
        CHECK(lineGpu.delta() == kMediumBytes + kSmallBytes);

        target = std::move(source);
        CHECK(target.bytes() == kMediumBytes);
        CHECK(source.bytes() == 0);
        CHECK(lineGpu.delta() == kMediumBytes);
    }

    CHECK(lineGpu.delta() == 0);
}

TEST_CASE("cwRenderMemoryLedger: paired adjustments from many threads balance out",
          "[RenderMemoryLedger]") {
    auto* ledger = cwRenderMemoryLedger::instance();

    LedgerScope textureGpu(Category::TexturedItemTexture, Residency::Gpu);

    // Keep the total well above zero so an interleaved -N never trips the clamp.
    ledger->adjust(Category::TexturedItemTexture, Residency::Gpu, kLargeBytes);

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (int thread = 0; thread < kThreadCount; ++thread) {
        threads.emplace_back([ledger]() {
            for (int i = 0; i < kIterationsPerThread; ++i) {
                ledger->adjust(Category::TexturedItemTexture, Residency::Gpu, kSmallBytes);
                ledger->adjust(Category::TexturedItemTexture, Residency::Gpu, -kSmallBytes);
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    CHECK(textureGpu.delta() == kLargeBytes);
}

