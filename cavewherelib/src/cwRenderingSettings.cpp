//Our includes
#include "cwRenderingSettings.h"
#include "cwPointOctree.h"

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
QString freezeCutKey() { return QStringLiteral("rendering/pointCloud/freezeCut"); }
QString maxLevelKey() { return QStringLiteral("rendering/pointCloud/maxLevel"); }
QString inflationGovernorKey() { return QStringLiteral("rendering/pointCloud/inflationGovernor"); }
QString governorSlewKey() { return QStringLiteral("rendering/pointCloud/governorSlew"); }
QString evictionHoldKey() { return QStringLiteral("rendering/pointCloud/evictionHold"); }
QString thinningModeKey() { return QStringLiteral("rendering/pointCloud/thinningMode"); }
QString sizeModeKey() { return QStringLiteral("rendering/pointCloud/sizeMode"); }
QString floorModeKey() { return QStringLiteral("rendering/pointCloud/floorMode"); }
QString spacingSourceKey() { return QStringLiteral("rendering/pointCloud/spacingSource"); }
QString fixedPointSizePxKey() { return QStringLiteral("rendering/pointCloud/fixedPointSizePx"); }
QString maxPointSizePxKey() { return QStringLiteral("rendering/pointCloud/maxPointSizePx"); }
QString minLevelKey() { return QStringLiteral("rendering/pointCloud/minLevel"); }
QString debugViewKey() { return QStringLiteral("rendering/pointCloud/debugView"); }
QString showNodeBoxesKey() { return QStringLiteral("rendering/pointCloud/showNodeBoxes"); }
QString revealFramesKey() { return QStringLiteral("rendering/pointCloud/revealFrames"); }
QString revealModeKey() { return QStringLiteral("rendering/pointCloud/revealMode"); }

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

double clampGovernorSlew(double slew)
{
    return std::clamp(slew, cw::budgets::kMinGovernorSlew, cw::budgets::kMaxGovernorSlew);
}

int clampMaxLevel(int level)
{
    return std::clamp(level, cw::budgets::kUnlimitedLevel, cw::octree::kMaxLevel);
}

int clampMinLevel(int level)
{
    return std::clamp(level, cw::budgets::kRootLevel, cw::octree::kMaxLevel);
}

// An inverted pair caps the cut below the draw floor, so gather() skips every
// node and the cloud vanishes.
bool isInvertedLevelPair(int minLevel, int maxLevel)
{
    return maxLevel != cw::budgets::kUnlimitedLevel && minLevel > maxLevel;
}

template <typename Mode>
Mode clampMode(int mode, int modeCount)
{
    return Mode(std::clamp(mode, 0, modeCount - 1));
}

int clampRevealFrames(int frames)
{
    return std::clamp(frames, cw::pointcloud::kMinRevealFrames, cw::pointcloud::kMaxRevealFrames);
}

float clampPointSizePx(double pixels)
{
    return float(std::clamp(pixels, cw::pointcloud::kMinPointSizePx,
                            cw::pointcloud::kMaxPointSizePx));
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
    if constexpr (std::is_enum_v<T>) {
        settings.setValue(key, int(value));
    } else {
        settings.setValue(key, value);
    }
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

    const cwPointCloudShading defaultShading;
    m_freezeCut = settings.value(freezeCutKey(), cw::budgets::kDefaultFreezeCut).toBool();
    m_maxLevel = clampMaxLevel(settings.value(maxLevelKey(), cw::budgets::kDefaultMaxLevel).toInt());
    m_minLevel = clampMinLevel(settings.value(minLevelKey(), cw::budgets::kDefaultMinLevel).toInt());
    if (isInvertedLevelPair(m_minLevel, m_maxLevel)) {
        m_minLevel = m_maxLevel;
    }
    m_inflationGovernor = settings.value(inflationGovernorKey(),
                                         cw::budgets::kDefaultInflationGovernor).toBool();
    m_governorSlew = clampGovernorSlew(
                settings.value(governorSlewKey(), cw::budgets::kDefaultGovernorSlew).toDouble());
    m_evictionHold = settings.value(evictionHoldKey(), cw::budgets::kDefaultEvictionHold).toBool();
    m_shading.thinningMode = clampMode<cw::pointcloud::ThinningMode>(
                settings.value(thinningModeKey(), int(defaultShading.thinningMode)).toInt(),
                cw::pointcloud::kThinningModeCount);
    m_shading.sizeMode = clampMode<cw::pointcloud::SizeMode>(
                settings.value(sizeModeKey(), int(defaultShading.sizeMode)).toInt(),
                cw::pointcloud::kSizeModeCount);
    m_shading.floorMode = clampMode<cw::pointcloud::FloorMode>(
                settings.value(floorModeKey(), int(defaultShading.floorMode)).toInt(),
                cw::pointcloud::kFloorModeCount);
    m_shading.spacingSource = clampMode<cw::pointcloud::SpacingSource>(
                settings.value(spacingSourceKey(), int(defaultShading.spacingSource)).toInt(),
                cw::pointcloud::kSpacingSourceCount);
    m_shading.fixedPointSizePx = clampPointSizePx(
                settings.value(fixedPointSizePxKey(), defaultShading.fixedPointSizePx).toDouble());
    m_shading.maxPointSizePx = clampPointSizePx(
                settings.value(maxPointSizePxKey(), defaultShading.maxPointSizePx).toDouble());
    m_shading.debugView = clampMode<cw::pointcloud::DebugView>(
                settings.value(debugViewKey(), int(defaultShading.debugView)).toInt(),
                cw::pointcloud::kDebugViewCount);
    m_shading.showNodeBoxes = settings.value(showNodeBoxesKey(),
                                             defaultShading.showNodeBoxes).toBool();
    m_shading.revealFrames = clampRevealFrames(
                settings.value(revealFramesKey(), defaultShading.revealFrames).toInt());
    m_shading.revealMode = clampMode<cw::pointcloud::RevealMode>(
                settings.value(revealModeKey(), int(defaultShading.revealMode)).toInt(),
                cw::pointcloud::kRevealModeCount);
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
    resetPointCloudKnobsToDefaults();
}

void cwRenderingSettings::resetPointCloudKnobsToDefaults()
{
    const cwPointCloudShading defaultShading;
    setFreezeCut(cw::budgets::kDefaultFreezeCut);
    setMaxLevel(cw::budgets::kDefaultMaxLevel);
    setMinLevel(cw::budgets::kDefaultMinLevel);
    setInflationGovernor(cw::budgets::kDefaultInflationGovernor);
    setGovernorSlew(cw::budgets::kDefaultGovernorSlew);
    setEvictionHold(cw::budgets::kDefaultEvictionHold);
    setThinningMode(int(defaultShading.thinningMode));
    setSizeMode(int(defaultShading.sizeMode));
    setFloorMode(int(defaultShading.floorMode));
    setSpacingSource(int(defaultShading.spacingSource));
    setFixedPointSizePx(defaultShading.fixedPointSizePx);
    setMaxPointSizePx(defaultShading.maxPointSizePx);
    setDebugView(int(defaultShading.debugView));
    setShowNodeBoxes(defaultShading.showNodeBoxes);
    setRevealFrames(defaultShading.revealFrames);
    setRevealMode(int(defaultShading.revealMode));
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
            && m_pointBudgetMillions == cw::budgets::kDefaultPointBudgetMillions
            && m_freezeCut == cw::budgets::kDefaultFreezeCut
            && m_maxLevel == cw::budgets::kDefaultMaxLevel
            && m_minLevel == cw::budgets::kDefaultMinLevel
            && m_inflationGovernor == cw::budgets::kDefaultInflationGovernor
            && qFuzzyCompare(m_governorSlew, cw::budgets::kDefaultGovernorSlew)
            && m_evictionHold == cw::budgets::kDefaultEvictionHold
            && m_shading == cwPointCloudShading();
}

int cwRenderingSettings::maximumLevel()
{
    return cw::octree::kMaxLevel;
}

cwRenderBudgets cwRenderingSettings::budgets() const
{
    cwRenderBudgets budgets;
    budgets.gpuBudgetBytes = gpuBudgetBytes();
    budgets.cpuBudgetBytes = qint64(m_cpuCacheBudgetMb) * cw::budgets::kBytesPerMegabyte;
    budgets.uploadBudgetBytesPerFrame = qint64(m_uploadBudgetMbPerFrame) * cw::budgets::kBytesPerMegabyte;
    budgets.screenSpaceErrorPx = m_screenSpaceErrorPx;
    budgets.pointBudget = qint64(m_pointBudgetMillions) * cw::budgets::kPointsPerMillion;
    budgets.freezeCut = m_freezeCut;
    budgets.maxLevel = m_maxLevel;
    budgets.minLevel = m_minLevel;
    budgets.inflationGovernor = m_inflationGovernor;
    budgets.governorSlew = m_governorSlew;
    budgets.evictionHold = m_evictionHold;
    return budgets;
}

void cwRenderingSettings::setFreezeCut(bool freeze)
{
    if (setPersisted(m_freezeCut, freeze, freezeCutKey())) {
        emit freezeCutChanged();
        emit budgetsChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setMaxLevel(int level)
{
    if (setPersisted(m_maxLevel, clampMaxLevel(level), maxLevelKey())) {
        emit maxLevelChanged();
        emit budgetsChanged();
        emit isAtDefaultsChanged();
    }
    if (isInvertedLevelPair(m_minLevel, m_maxLevel)) {
        setMinLevel(m_maxLevel);
    }
}

void cwRenderingSettings::setMinLevel(int level)
{
    if (setPersisted(m_minLevel, clampMinLevel(level), minLevelKey())) {
        emit minLevelChanged();
        emit budgetsChanged();
        emit isAtDefaultsChanged();
    }
    if (isInvertedLevelPair(m_minLevel, m_maxLevel)) {
        setMaxLevel(m_minLevel);
    }
}

void cwRenderingSettings::setInflationGovernor(bool governed)
{
    if (setPersisted(m_inflationGovernor, governed, inflationGovernorKey())) {
        emit inflationGovernorChanged();
        emit budgetsChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setGovernorSlew(double slew)
{
    if (setPersisted(m_governorSlew, clampGovernorSlew(slew), governorSlewKey())) {
        emit governorSlewChanged();
        emit budgetsChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setEvictionHold(bool hold)
{
    if (setPersisted(m_evictionHold, hold, evictionHoldKey())) {
        emit evictionHoldChanged();
        emit budgetsChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setThinningMode(int mode)
{
    const auto clamped = clampMode<cw::pointcloud::ThinningMode>(
        mode, cw::pointcloud::kThinningModeCount);
    if (setPersisted(m_shading.thinningMode, clamped, thinningModeKey())) {
        emit thinningModeChanged();
        emit pointCloudShadingChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setSizeMode(int mode)
{
    const auto clamped = clampMode<cw::pointcloud::SizeMode>(mode, cw::pointcloud::kSizeModeCount);
    if (setPersisted(m_shading.sizeMode, clamped, sizeModeKey())) {
        emit sizeModeChanged();
        emit pointCloudShadingChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setFloorMode(int mode)
{
    const auto clamped = clampMode<cw::pointcloud::FloorMode>(mode, cw::pointcloud::kFloorModeCount);
    if (setPersisted(m_shading.floorMode, clamped, floorModeKey())) {
        emit floorModeChanged();
        emit pointCloudShadingChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setSpacingSource(int source)
{
    const auto clamped = clampMode<cw::pointcloud::SpacingSource>(
        source, cw::pointcloud::kSpacingSourceCount);
    if (setPersisted(m_shading.spacingSource, clamped, spacingSourceKey())) {
        emit spacingSourceChanged();
        emit pointCloudShadingChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setFixedPointSizePx(double pixels)
{
    if (setPersisted(m_shading.fixedPointSizePx, clampPointSizePx(pixels), fixedPointSizePxKey())) {
        emit fixedPointSizePxChanged();
        emit pointCloudShadingChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setMaxPointSizePx(double pixels)
{
    if (setPersisted(m_shading.maxPointSizePx, clampPointSizePx(pixels), maxPointSizePxKey())) {
        emit maxPointSizePxChanged();
        emit pointCloudShadingChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setDebugView(int view)
{
    const auto clamped = clampMode<cw::pointcloud::DebugView>(view, cw::pointcloud::kDebugViewCount);
    if (setPersisted(m_shading.debugView, clamped, debugViewKey())) {
        emit debugViewChanged();
        emit pointCloudShadingChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setShowNodeBoxes(bool show)
{
    if (setPersisted(m_shading.showNodeBoxes, show, showNodeBoxesKey())) {
        emit showNodeBoxesChanged();
        emit pointCloudShadingChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setRevealFrames(int frames)
{
    if (setPersisted(m_shading.revealFrames, clampRevealFrames(frames), revealFramesKey())) {
        emit revealFramesChanged();
        emit pointCloudShadingChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setRevealMode(int mode)
{
    const auto clamped = clampMode<cw::pointcloud::RevealMode>(mode, cw::pointcloud::kRevealModeCount);
    if (setPersisted(m_shading.revealMode, clamped, revealModeKey())) {
        emit revealModeChanged();
        emit pointCloudShadingChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setGpuMemoryBudgetMb(int megabytes)
{
    if (setPersisted(m_gpuMemoryBudgetMb, clampGpuMemoryBudgetMb(megabytes), gpuMemoryBudgetMbKey())) {
        emit gpuMemoryBudgetMbChanged();
        emit budgetsChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setCpuCacheBudgetMb(int megabytes)
{
    if (setPersisted(m_cpuCacheBudgetMb, clampCpuCacheBudgetMb(megabytes), cpuCacheBudgetMbKey())) {
        emit cpuCacheBudgetMbChanged();
        emit budgetsChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setUploadBudgetMbPerFrame(int megabytes)
{
    if (setPersisted(m_uploadBudgetMbPerFrame,
                     clampUploadBudgetMbPerFrame(megabytes),
                     uploadBudgetMbPerFrameKey())) {
        emit uploadBudgetMbPerFrameChanged();
        emit budgetsChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setScreenSpaceErrorPx(double pixels)
{
    if (setPersisted(m_screenSpaceErrorPx, clampScreenSpaceErrorPx(pixels), screenSpaceErrorPxKey())) {
        emit screenSpaceErrorPxChanged();
        emit budgetsChanged();
        emit isAtDefaultsChanged();
    }
}

void cwRenderingSettings::setPointBudgetMillions(int millions)
{
    if (setPersisted(m_pointBudgetMillions,
                     clampPointBudgetMillions(millions),
                     pointBudgetMillionsKey())) {
        emit pointBudgetMillionsChanged();
        emit budgetsChanged();
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
