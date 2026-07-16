#ifndef CWRENDERINGSETTINGS_H
#define CWRENDERINGSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QList>

//Our includes
#include "cwGlobals.h"

class QQuickRhiItem;

// App-global render-quality knobs. Kept as a singleton so the defaults live in
// one place and can be tweaked from QML / tests. sampleCount is the single MSAA
// source of truth: the 3D viewer's QQuickRhiItem reads it (cw3dRegionViewer) and
// the EDL composite path inherits it. supportedSampleCounts is reported by the
// active QRhi backend (see cwRhiScene) so the UI offers only valid MSAA levels;
// it is platform dependent (e.g. Metal supports 1/2/4 but not 8).
class CAVEWHERE_LIB_EXPORT cwRenderingSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderingSettings)
    QML_UNCREATABLE("RenderingSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(int sampleCount READ sampleCount WRITE setSampleCount NOTIFY sampleCountChanged)
    Q_PROPERTY(QList<int> supportedSampleCounts READ supportedSampleCounts NOTIFY supportedSampleCountsChanged)
    Q_PROPERTY(bool isAtDefaults READ isAtDefaults NOTIFY isAtDefaultsChanged)

public:
    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int samples);

    QList<int> supportedSampleCounts() const { return m_supportedSampleCounts; }
    void setSupportedSampleCounts(const QList<int>& counts);

    bool isAtDefaults() const;
    Q_INVOKABLE void resetToDefaults();

    // Apply sampleCount to item now and on every change. Connection scoped to item.
    void driveSampleCount(QQuickRhiItem* item);

    static cwRenderingSettings* instance();
    static void initialize();

signals:
    void sampleCountChanged();
    void supportedSampleCountsChanged();
    void isAtDefaultsChanged();

private:
    explicit cwRenderingSettings(QObject* parent = nullptr);

    // Snap an arbitrary request down to the nearest supported MSAA level (never
    // below the smallest supported count, which is always 1).
    int clampToSupported(int samples) const;

    static cwRenderingSettings* Settings;

    int m_sampleCount = 4; // overwritten from QSettings in the constructor; see kDefaultSampleCount

    // Safe baseline until the QRhi backend reports the real set (see cwRhiScene).
    // Always kept sorted ascending and containing 1.
    QList<int> m_supportedSampleCounts = {1, 2, 4, 8};
};

#endif // CWRENDERINGSETTINGS_H
