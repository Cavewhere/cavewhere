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

// 4x MSAA is the historical default and a good quality/cost balance. Snapped to
// the device's supported set if 4 happens to be unavailable.
constexpr int kDefaultSampleCount = 4;

// The render-memory HUD is a debugging aid, so it stays off until asked for.
constexpr bool kDefaultShowRenderMemoryHud = false;
}

cwRenderingSettings* cwRenderingSettings::Settings = nullptr;

cwRenderingSettings::cwRenderingSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    m_sampleCount = clampToSupported(settings.value(sampleCountKey(), kDefaultSampleCount).toInt());
    m_showRenderMemoryHud = settings.value(showRenderMemoryHudKey(), kDefaultShowRenderMemoryHud).toBool();
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
}

bool cwRenderingSettings::isAtDefaults() const
{
    return m_sampleCount == clampToSupported(kDefaultSampleCount)
            && m_showRenderMemoryHud == kDefaultShowRenderMemoryHud;
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
