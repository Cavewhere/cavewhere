//Our includes
#include "cwPassageTubeSettings.h"

//Qt includes
#include <QCoreApplication>
#include <QSettings>

//Std includes
#include <algorithm>

namespace {
QString sampleSpacingMetersKey() { return QStringLiteral("passageTube/sampleSpacingMeters"); }
QString ringResolutionKey() { return QStringLiteral("passageTube/ringResolution"); }

constexpr double kDefaultSampleSpacingMeters = 1.0;
constexpr double kMinSampleSpacingMeters = 0.1;
constexpr double kMaxSampleSpacingMeters = 50.0;

constexpr int kDefaultRingResolution = 16;
constexpr int kMinRingResolution = 4;
constexpr int kMaxRingResolution = 64;

double clampSampleSpacing(double meters)
{
    return std::clamp(meters, kMinSampleSpacingMeters, kMaxSampleSpacingMeters);
}

int clampRingResolution(int points)
{
    return std::clamp(points, kMinRingResolution, kMaxRingResolution);
}
}

cwPassageTubeSettings* cwPassageTubeSettings::Settings = nullptr;

cwPassageTubeSettings::cwPassageTubeSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    m_sampleSpacingMeters = clampSampleSpacing(
        settings.value(sampleSpacingMetersKey(), kDefaultSampleSpacingMeters).toDouble());
    m_ringResolution = clampRingResolution(
        settings.value(ringResolutionKey(), kDefaultRingResolution).toInt());
}

void cwPassageTubeSettings::setSampleSpacingMeters(double meters)
{
    const double clamped = clampSampleSpacing(meters);
    if (qFuzzyCompare(m_sampleSpacingMeters, clamped)) {
        return;
    }
    m_sampleSpacingMeters = clamped;
    QSettings settings;
    settings.setValue(sampleSpacingMetersKey(), clamped);
    emit sampleSpacingMetersChanged();
    emit isAtDefaultsChanged();
}

void cwPassageTubeSettings::setRingResolution(int points)
{
    const int clamped = clampRingResolution(points);
    if (m_ringResolution == clamped) {
        return;
    }
    m_ringResolution = clamped;
    QSettings settings;
    settings.setValue(ringResolutionKey(), clamped);
    emit ringResolutionChanged();
    emit isAtDefaultsChanged();
}

bool cwPassageTubeSettings::isAtDefaults() const
{
    return qFuzzyCompare(m_sampleSpacingMeters, kDefaultSampleSpacingMeters)
            && m_ringResolution == kDefaultRingResolution;
}

void cwPassageTubeSettings::resetToDefaults()
{
    setSampleSpacingMeters(kDefaultSampleSpacingMeters);
    setRingResolution(kDefaultRingResolution);
}

cwPassageTubeSettings* cwPassageTubeSettings::instance()
{
    return Settings;
}

void cwPassageTubeSettings::initialize()
{
    if (Settings == nullptr) {
        Settings = new cwPassageTubeSettings(QCoreApplication::instance());
    }
}
