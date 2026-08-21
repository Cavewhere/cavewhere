//Our includes
#include "cwRenderingSettings.h"

//Qt includes
#include <QCoreApplication>
#include <QQuickRhiItem>
#include <QSettings>

//Std includes
#include <algorithm>

namespace {
QString sampleCountKey() { return QStringLiteral("rendering/sampleCount"); }
QString showRenderMemoryHudKey() { return QStringLiteral("rendering/showRenderMemoryHud"); }
QString gpuMemoryBudgetMbKey() { return QStringLiteral("rendering/gpuMemoryBudgetMb"); }
QString cpuCacheBudgetMbKey() { return QStringLiteral("rendering/cpuCacheBudgetMb"); }
QString uploadBudgetMbPerFrameKey() { return QStringLiteral("rendering/uploadBudgetMbPerFrame"); }
QString screenSpaceErrorPxKey() { return QStringLiteral("rendering/screenSpaceErrorPx"); }

// 4x MSAA is the historical default and a good quality/cost balance. Snapped to
// the device's supported set if 4 happens to be unavailable.
constexpr int kDefaultSampleCount = 4;

// The render-memory HUD is a debugging aid, so it stays off until asked for.
constexpr bool kDefaultShowRenderMemoryHud = false;

// Budget knobs, advisory in Phase 1 (see the header). The minimums keep a
// hand-edited QSettings file from asking for a budget too small to hold a
// single frame's working set.
constexpr int kDefaultGpuMemoryBudgetMb = 1536;
constexpr int kMinGpuMemoryBudgetMb = 256;

constexpr int kDefaultCpuCacheBudgetMb = 512;
constexpr int kMinCpuCacheBudgetMb = 64;

constexpr int kDefaultUploadBudgetMbPerFrame = 8;
constexpr int kMinUploadBudgetMbPerFrame = 1;

constexpr double kDefaultScreenSpaceErrorPx = 1.5;
constexpr double kMinScreenSpaceErrorPx = 0.5;
constexpr double kMaxScreenSpaceErrorPx = 8.0;
}

cwRenderingSettings* cwRenderingSettings::Settings = nullptr;

cwRenderingSettings::cwRenderingSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    m_sampleCount = clampToSupported(settings.value(sampleCountKey(), kDefaultSampleCount).toInt());
    m_showRenderMemoryHud = settings.value(showRenderMemoryHudKey(), kDefaultShowRenderMemoryHud).toBool();
    m_gpuMemoryBudgetMb = std::max(kMinGpuMemoryBudgetMb,
                                   settings.value(gpuMemoryBudgetMbKey(), kDefaultGpuMemoryBudgetMb).toInt());
    m_cpuCacheBudgetMb = std::max(kMinCpuCacheBudgetMb,
                                  settings.value(cpuCacheBudgetMbKey(), kDefaultCpuCacheBudgetMb).toInt());
    m_uploadBudgetMbPerFrame = std::max(kMinUploadBudgetMbPerFrame,
                                        settings.value(uploadBudgetMbPerFrameKey(), kDefaultUploadBudgetMbPerFrame).toInt());
    m_screenSpaceErrorPx = std::clamp(settings.value(screenSpaceErrorPxKey(), kDefaultScreenSpaceErrorPx).toDouble(),
                                      kMinScreenSpaceErrorPx,
                                      kMaxScreenSpaceErrorPx);
}

int cwRenderingSettings::clampToSupported(int samples) const
{
    // m_supportedSampleCounts is always non-empty, sorted ascending, and starts
    // at 1, so this floors to the largest supported level <= samples.
    int snapped = m_supportedSampleCounts.first();
    for (int supported : m_supportedSampleCounts) {
        if (samples >= supported) {
            snapped = supported;
        }
    }
    return snapped;
}

void cwRenderingSettings::resetToDefaults()
{
    setSampleCount(kDefaultSampleCount);
    setShowRenderMemoryHud(kDefaultShowRenderMemoryHud);
    setGpuMemoryBudgetMb(kDefaultGpuMemoryBudgetMb);
    setCpuCacheBudgetMb(kDefaultCpuCacheBudgetMb);
    setUploadBudgetMbPerFrame(kDefaultUploadBudgetMbPerFrame);
    setScreenSpaceErrorPx(kDefaultScreenSpaceErrorPx);
}

bool cwRenderingSettings::isAtDefaults() const
{
    return m_sampleCount == clampToSupported(kDefaultSampleCount)
            && m_showRenderMemoryHud == kDefaultShowRenderMemoryHud
            && m_gpuMemoryBudgetMb == kDefaultGpuMemoryBudgetMb
            && m_cpuCacheBudgetMb == kDefaultCpuCacheBudgetMb
            && m_uploadBudgetMbPerFrame == kDefaultUploadBudgetMbPerFrame
            && qFuzzyCompare(m_screenSpaceErrorPx, kDefaultScreenSpaceErrorPx);
}

void cwRenderingSettings::setGpuMemoryBudgetMb(int megabytes)
{
    const int clamped = std::max(kMinGpuMemoryBudgetMb, megabytes);
    if (m_gpuMemoryBudgetMb == clamped) {
        return;
    }
    m_gpuMemoryBudgetMb = clamped;
    QSettings settings;
    settings.setValue(gpuMemoryBudgetMbKey(), clamped);
    emit gpuMemoryBudgetMbChanged();
    emit isAtDefaultsChanged();
}

void cwRenderingSettings::setCpuCacheBudgetMb(int megabytes)
{
    const int clamped = std::max(kMinCpuCacheBudgetMb, megabytes);
    if (m_cpuCacheBudgetMb == clamped) {
        return;
    }
    m_cpuCacheBudgetMb = clamped;
    QSettings settings;
    settings.setValue(cpuCacheBudgetMbKey(), clamped);
    emit cpuCacheBudgetMbChanged();
    emit isAtDefaultsChanged();
}

void cwRenderingSettings::setUploadBudgetMbPerFrame(int megabytes)
{
    const int clamped = std::max(kMinUploadBudgetMbPerFrame, megabytes);
    if (m_uploadBudgetMbPerFrame == clamped) {
        return;
    }
    m_uploadBudgetMbPerFrame = clamped;
    QSettings settings;
    settings.setValue(uploadBudgetMbPerFrameKey(), clamped);
    emit uploadBudgetMbPerFrameChanged();
    emit isAtDefaultsChanged();
}

void cwRenderingSettings::setScreenSpaceErrorPx(double pixels)
{
    const double clamped = std::clamp(pixels, kMinScreenSpaceErrorPx, kMaxScreenSpaceErrorPx);
    if (qFuzzyCompare(m_screenSpaceErrorPx, clamped)) {
        return;
    }
    m_screenSpaceErrorPx = clamped;
    QSettings settings;
    settings.setValue(screenSpaceErrorPxKey(), clamped);
    emit screenSpaceErrorPxChanged();
    emit isAtDefaultsChanged();
}

void cwRenderingSettings::setShowRenderMemoryHud(bool show)
{
    if (m_showRenderMemoryHud == show) {
        return;
    }
    m_showRenderMemoryHud = show;
    QSettings settings;
    settings.setValue(showRenderMemoryHudKey(), show);
    emit showRenderMemoryHudChanged();
    emit isAtDefaultsChanged();
}

void cwRenderingSettings::setSampleCount(int samples)
{
    const int clamped = clampToSupported(samples);
    if (m_sampleCount == clamped) {
        return;
    }
    m_sampleCount = clamped;
    QSettings settings;
    settings.setValue(sampleCountKey(), clamped);
    emit sampleCountChanged();
    emit isAtDefaultsChanged();
}

void cwRenderingSettings::setSupportedSampleCounts(const QList<int>& counts)
{
    // Sanitize: keep only valid levels, guarantee 1 is present (no MSAA must
    // always be selectable), de-duplicate, and sort ascending so clampToSupported
    // and the UI can rely on the ordering.
    QList<int> sanitized;
    for (int count : counts) {
        if (count >= 1 && !sanitized.contains(count)) {
            sanitized.append(count);
        }
    }
    if (!sanitized.contains(1)) {
        sanitized.append(1);
    }
    std::sort(sanitized.begin(), sanitized.end());

    if (sanitized == m_supportedSampleCounts) {
        return;
    }
    m_supportedSampleCounts = sanitized;
    emit supportedSampleCountsChanged();

    // Re-clamp the current selection against the new device capabilities; this
    // emits sampleCountChanged() only if it actually changes the value.
    setSampleCount(m_sampleCount);
}

void cwRenderingSettings::driveSampleCount(QQuickRhiItem* item)
{
    if (item == nullptr) {
        return;
    }
    item->setSampleCount(m_sampleCount);
    connect(this, &cwRenderingSettings::sampleCountChanged, item, [this, item]() {
        item->setSampleCount(m_sampleCount);
    });
}

cwRenderingSettings* cwRenderingSettings::instance()
{
    return Settings;
}

void cwRenderingSettings::initialize()
{
    if (Settings == nullptr) {
        Settings = new cwRenderingSettings(QCoreApplication::instance());
    }
}
