#ifndef CWPASSAGETUBESETTINGS_H
#define CWPASSAGETUBESETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

// Sampling knobs for the passage tube: how far apart rings sit along a shot
// and how many points go around each ring.
class CAVEWHERE_LIB_EXPORT cwPassageTubeSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PassageTubeSettings)
    QML_UNCREATABLE("PassageTubeSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(double sampleSpacingMeters READ sampleSpacingMeters WRITE setSampleSpacingMeters NOTIFY sampleSpacingMetersChanged)
    Q_PROPERTY(int ringResolution READ ringResolution WRITE setRingResolution NOTIFY ringResolutionChanged)
    Q_PROPERTY(bool isAtDefaults READ isAtDefaults NOTIFY isAtDefaultsChanged)

public:
    // Every instance reads and writes the same QSettings keys; the app shares instance().
    explicit cwPassageTubeSettings(QObject* parent = nullptr);

    double sampleSpacingMeters() const { return m_sampleSpacingMeters; }
    void setSampleSpacingMeters(double meters);

    int ringResolution() const { return m_ringResolution; }
    void setRingResolution(int points);

    bool isAtDefaults() const;
    Q_INVOKABLE void resetToDefaults();

    static cwPassageTubeSettings* instance();
    static void initialize();

signals:
    void sampleSpacingMetersChanged();
    void ringResolutionChanged();
    void isAtDefaultsChanged();

private:
    static cwPassageTubeSettings* Settings;

    double m_sampleSpacingMeters;
    int m_ringResolution;
};

#endif // CWPASSAGETUBESETTINGS_H
