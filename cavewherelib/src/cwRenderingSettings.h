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
//
// The budget knobs (gpuMemoryBudgetMb, cpuCacheBudgetMb, uploadBudgetMbPerFrame,
// screenSpaceErrorPx) drive streamed-texture residency: the render thread reads
// them through cwRenderBudgets each frame to pick mip levels, pace uploads, and
// demote back under the GPU budget. The render-stats HUD reports the GPU total
// against gpuMemoryBudgetMb.
class CAVEWHERE_LIB_EXPORT cwRenderingSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderingSettings)
    QML_UNCREATABLE("RenderingSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(int sampleCount READ sampleCount WRITE setSampleCount NOTIFY sampleCountChanged)
    Q_PROPERTY(QList<int> supportedSampleCounts READ supportedSampleCounts NOTIFY supportedSampleCountsChanged)
    Q_PROPERTY(bool showRenderStatsHud READ showRenderStatsHud WRITE setShowRenderStatsHud NOTIFY showRenderStatsHudChanged)
    Q_PROPERTY(int gpuMemoryBudgetMb READ gpuMemoryBudgetMb WRITE setGpuMemoryBudgetMb NOTIFY gpuMemoryBudgetMbChanged)
    Q_PROPERTY(int cpuCacheBudgetMb READ cpuCacheBudgetMb WRITE setCpuCacheBudgetMb NOTIFY cpuCacheBudgetMbChanged)
    Q_PROPERTY(int uploadBudgetMbPerFrame READ uploadBudgetMbPerFrame WRITE setUploadBudgetMbPerFrame NOTIFY uploadBudgetMbPerFrameChanged)
    Q_PROPERTY(double screenSpaceErrorPx READ screenSpaceErrorPx WRITE setScreenSpaceErrorPx NOTIFY screenSpaceErrorPxChanged)
    Q_PROPERTY(bool isAtDefaults READ isAtDefaults NOTIFY isAtDefaultsChanged)

public:
    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int samples);

    QList<int> supportedSampleCounts() const { return m_supportedSampleCounts; }
    void setSupportedSampleCounts(const QList<int>& counts);

    bool showRenderStatsHud() const { return m_showRenderStatsHud; }
    void setShowRenderStatsHud(bool show);

    int gpuMemoryBudgetMb() const { return m_gpuMemoryBudgetMb; }
    void setGpuMemoryBudgetMb(int megabytes);

    int cpuCacheBudgetMb() const { return m_cpuCacheBudgetMb; }
    void setCpuCacheBudgetMb(int megabytes);

    int uploadBudgetMbPerFrame() const { return m_uploadBudgetMbPerFrame; }
    void setUploadBudgetMbPerFrame(int megabytes);

    double screenSpaceErrorPx() const { return m_screenSpaceErrorPx; }
    void setScreenSpaceErrorPx(double pixels);

    bool isAtDefaults() const;
    Q_INVOKABLE void resetToDefaults();

    // Apply sampleCount to item now and on every change. Connection scoped to item.
    void driveSampleCount(QQuickRhiItem* item);

    static cwRenderingSettings* instance();
    static void initialize();

signals:
    void sampleCountChanged();
    void showRenderStatsHudChanged();
    void gpuMemoryBudgetMbChanged();
    void cpuCacheBudgetMbChanged();
    void uploadBudgetMbPerFrameChanged();
    void screenSpaceErrorPxChanged();
    void supportedSampleCountsChanged();
    void isAtDefaultsChanged();

private:
    explicit cwRenderingSettings(QObject* parent = nullptr);

    // Snap an arbitrary request down to the nearest supported MSAA level (never
    // below the smallest supported count, which is always 1).
    int clampToSupported(int samples) const;

    static cwRenderingSettings* Settings;

    int m_sampleCount = 4; // overwritten from QSettings in the constructor; see kDefaultSampleCount
    bool m_showRenderStatsHud = false; // see kDefaultShowRenderStatsHud

    // All four are overwritten from QSettings in the constructor; see the
    // kDefault constants in the .cpp.
    int m_gpuMemoryBudgetMb = 1536;
    int m_cpuCacheBudgetMb = 512;
    int m_uploadBudgetMbPerFrame = 8;
    double m_screenSpaceErrorPx = 1.5;

    // Safe baseline until the QRhi backend reports the real set (see cwRhiScene).
    // Always kept sorted ascending and containing 1.
    QList<int> m_supportedSampleCounts = {1, 2, 4, 8};
};

#endif // CWRENDERINGSETTINGS_H
