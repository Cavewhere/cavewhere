//Our includes
#include "cwRenderingSettings.h"

//Qt includes
#include <QCoreApplication>
#include <QQuickRhiItem>
#include <QSettings>

//Std includes
#include <algorithm>
#include <type_traits>

namespace {
QString sampleCountKey() { return QStringLiteral("rendering/sampleCount"); }
QString showRenderStatsHudKey() { return QStringLiteral("rendering/showRenderStatsHud"); }
QString gpuMemoryBudgetMbKey() { return QStringLiteral("rendering/gpuMemoryBudgetMb"); }
QString cpuCacheBudgetMbKey() { return QStringLiteral("rendering/cpuCacheBudgetMb"); }
QString uploadBudgetMbPerFrameKey() { return QStringLiteral("rendering/uploadBudgetMbPerFrame"); }
QString screenSpaceErrorPxKey() { return QStringLiteral("rendering/screenSpaceErrorPx"); }
QString pointBudgetMillionsKey() { return QStringLiteral("rendering/pointBudgetMillions"); }

// 4x MSAA is the historical default and a good quality/cost balance. Snapped to
// the device's supported set if 4 happens to be unavailable.
constexpr int kDefaultSampleCount = 4;

// The render-stats HUD is a debugging aid, so it stays off until asked for.
constexpr bool kDefaultShowRenderStatsHud = false;

// The budget limits live in cw::budgets so the render thread, the settings UI,
// and these clamps all count from the same numbers.
int clampGpuMemoryBudgetMb(int megabytes)
{
    return std::clamp(megabytes, cw::budgets::kMinGpuBudgetMb, cw::budgets::kMaxGpuBudgetMb);
}

int clampCpuCacheBudgetMb(int megabytes)
{
    return std::clamp(megabytes, cw::budgets::kMinCpuBudgetMb, cw::budgets::kMaxCpuBudgetMb);
}

int clampUploadBudgetMbPerFrame(int megabytes)
{
    return std::clamp(megabytes,
                      cw::budgets::kMinUploadBudgetMbPerFrame,
                      cw::budgets::kMaxUploadBudgetMbPerFrame);
}

int clampPointBudgetMillions(int millions)
{
    return std::clamp(millions,
                      cw::budgets::kMinPointBudgetMillions,
                      cw::budgets::kMaxPointBudgetMillions);
}

double clampScreenSpaceErrorPx(double pixels)
{
    return std::clamp(pixels,
                      cw::budgets::kMinScreenSpaceErrorPx,
                      cw::budgets::kMaxScreenSpaceErrorPx);
}

// The persisted setters differ only in their member, key, and notify signal, so
// the store-and-persist half funnels through here. Returns true when the member
// changed and the caller should emit.
template <typename T>
bool setPersisted(T& member, const T& value, const QString& key)
{
    if constexpr (std::is_floating_point_v<T>) {
        if (qFuzzyCompare(member, value)) {
            return false;
        }
    } else {
        if (member == value) {
            return false;
        }
    }
    member = value;
    QSettings settings;
    settings.setValue(key, value);
    return true;
}
}

cwRenderingSettings* cwRenderingSettings::Settings = nullptr;

cwRenderingSettings::cwRenderingSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    m_sampleCount = clampToSupported(settings.value(sampleCountKey(), kDefaultSampleCount).toInt());
    m_showRenderStatsHud = settings.value(showRenderStatsHudKey(), kDefaultShowRenderStatsHud).toBool();
    m_gpuMemoryBudgetMb = clampGpuMemoryBudgetMb(
                settings.value(gpuMemoryBudgetMbKey(), cw::budgets::kDefaultGpuBudgetMb).toInt());
    m_cpuCacheBudgetMb = clampCpuCacheBudgetMb(
                settings.value(cpuCacheBudgetMbKey(), cw::budgets::kDefaultCpuBudgetMb).toInt());
    m_uploadBudgetMbPerFrame = clampUploadBudgetMbPerFrame(
                settings.value(uploadBudgetMbPerFrameKey(), cw::budgets::kDefaultUploadBudgetMbPerFrame).toInt());
    m_screenSpaceErrorPx = clampScreenSpaceErrorPx(
                settings.value(screenSpaceErrorPxKey(), cw::budgets::kDefaultScreenSpaceErrorPx).toDouble());
    m_pointBudgetMillions = clampPointBudgetMillions(
                settings.value(pointBudgetMillionsKey(), cw::budgets::kDefaultPointBudgetMillions).toInt());
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
    setShowRenderStatsHud(kDefaultShowRenderStatsHud);
    setGpuMemoryBudgetMb(cw::budgets::kDefaultGpuBudgetMb);
    setCpuCacheBudgetMb(cw::budgets::kDefaultCpuBudgetMb);
    setUploadBudgetMbPerFrame(cw::budgets::kDefaultUploadBudgetMbPerFrame);
    setScreenSpaceErrorPx(cw::budgets::kDefaultScreenSpaceErrorPx);
    setPointBudgetMillions(cw::budgets::kDefaultPointBudgetMillions);
}

bool cwRenderingSettings::isAtDefaults() const
{
    return m_sampleCount == clampToSupported(kDefaultSampleCount)
            && m_showRenderStatsHud == kDefaultShowRenderStatsHud
            && m_gpuMemoryBudgetMb == cw::budgets::kDefaultGpuBudgetMb
            && m_cpuCacheBudgetMb == cw::budgets::kDefaultCpuBudgetMb
            && m_uploadBudgetMbPerFrame == cw::budgets::kDefaultUploadBudgetMbPerFrame
            && qFuzzyCompare(m_screenSpaceErrorPx, cw::budgets::kDefaultScreenSpaceErrorPx)
            && m_pointBudgetMillions == cw::budgets::kDefaultPointBudgetMillions;
}

cwRenderBudgets cwRenderingSettings::budgets() const
{
    cwRenderBudgets budgets;
    budgets.gpuBudgetBytes = gpuBudgetBytes();
    budgets.cpuBudgetBytes = qint64(m_cpuCacheBudgetMb) * cw::budgets::kBytesPerMegabyte;
    budgets.uploadBudgetBytesPerFrame = qint64(m_uploadBudgetMbPerFrame) * cw::budgets::kBytesPerMegabyte;
    budgets.screenSpaceErrorPx = m_screenSpaceErrorPx;
    budgets.pointBudget = qint64(m_pointBudgetMillions) * cw::budgets::kPointsPerMillion;
    return budgets;
}

void cwRenderingSettings::setGpuMemoryBudgetMb(int megabytes)
{
    if (setPersisted(m_gpuMemoryBudgetMb, clampGpuMemoryBudgetMb(megabytes), gpuMemoryBudgetMbKey())) {
        emit gpuMemoryBudgetMbChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setCpuCacheBudgetMb(int megabytes)
{
    if (setPersisted(m_cpuCacheBudgetMb, clampCpuCacheBudgetMb(megabytes), cpuCacheBudgetMbKey())) {
        emit cpuCacheBudgetMbChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setUploadBudgetMbPerFrame(int megabytes)
{
    if (setPersisted(m_uploadBudgetMbPerFrame,
                     clampUploadBudgetMbPerFrame(megabytes),
                     uploadBudgetMbPerFrameKey())) {
        emit uploadBudgetMbPerFrameChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setScreenSpaceErrorPx(double pixels)
{
    if (setPersisted(m_screenSpaceErrorPx, clampScreenSpaceErrorPx(pixels), screenSpaceErrorPxKey())) {
        emit screenSpaceErrorPxChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setPointBudgetMillions(int millions)
{
    if (setPersisted(m_pointBudgetMillions,
                     clampPointBudgetMillions(millions),
                     pointBudgetMillionsKey())) {
        emit pointBudgetMillionsChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setShowRenderStatsHud(bool show)
{
    if (setPersisted(m_showRenderStatsHud, show, showRenderStatsHudKey())) {
        emit showRenderStatsHudChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setSampleCount(int samples)
{
    if (setPersisted(m_sampleCount, clampToSupported(samples), sampleCountKey())) {
        emit sampleCountChanged();
        emit isAtDefaultsChanged();
    }
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
