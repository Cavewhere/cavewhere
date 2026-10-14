#ifndef CWRENDERINGSETTINGS_H
#define CWRENDERINGSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QList>

//Our includes
#include "cwGlobals.h"
#include "cwPointCloudShading.h"
#include "cwRenderBudgets.h"

class QQuickRhiItem;

// App-global render-quality knobs. Kept as a singleton so the defaults live in
// one place and can be tweaked from QML / tests. sampleCount is the single MSAA
// source of truth: the 3D viewer's QQuickRhiItem reads it (cw3dRegionViewer) and
// the EDL composite path inherits it. supportedSampleCounts is reported by the
// active QRhi backend (see cwRhiScene) so the UI offers only valid MSAA levels;
// it is platform dependent (e.g. Metal supports 1/2/4 but not 8).
//
// The budget knobs (gpuMemoryBudgetMb, cpuCacheBudgetMb, uploadBudgetMbPerFrame,
// screenSpaceErrorPx, pointBudgetMillions) drive streamed residency: the render
// thread reads them through cwRenderBudgets each frame to pick mip levels, pace
// uploads, cut the point octree, and demote back under the GPU budget. The
// render-stats HUD reports the GPU total against gpuMemoryBudgetMb.
//
// The point cloud knobs switch one streaming or CLOD rule each so a picture
// can be A/B'd (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html section 7). freezeCut,
// maxLevel, inflationGovernor and evictionHold ride cwRenderBudgets to the
// planner, forest and eviction; the thinning, sizing and floor modes and the
// two pixel sizes ride pointCloudShading() to every cloud's per-cloud uniform.
// The section 8 debug knobs travel the same two ways: minLevel with the
// budgets, debugView and showNodeBoxes with the shading.
// Every default is the renderer's own rule.
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
    Q_PROPERTY(int pointBudgetMillions READ pointBudgetMillions WRITE setPointBudgetMillions NOTIFY pointBudgetMillionsChanged)
    Q_PROPERTY(qint64 gpuBudgetBytes READ gpuBudgetBytes NOTIFY gpuMemoryBudgetMbChanged)
    Q_PROPERTY(bool freezeCut READ freezeCut WRITE setFreezeCut NOTIFY freezeCutChanged)
    Q_PROPERTY(int maxLevel READ maxLevel WRITE setMaxLevel NOTIFY maxLevelChanged)
    Q_PROPERTY(int minLevel READ minLevel WRITE setMinLevel NOTIFY minLevelChanged)
    Q_PROPERTY(bool inflationGovernor READ inflationGovernor WRITE setInflationGovernor NOTIFY inflationGovernorChanged)
    Q_PROPERTY(bool evictionHold READ evictionHold WRITE setEvictionHold NOTIFY evictionHoldChanged)
    Q_PROPERTY(int thinningMode READ thinningMode WRITE setThinningMode NOTIFY thinningModeChanged)
    Q_PROPERTY(int sizeMode READ sizeMode WRITE setSizeMode NOTIFY sizeModeChanged)
    Q_PROPERTY(int floorMode READ floorMode WRITE setFloorMode NOTIFY floorModeChanged)
    Q_PROPERTY(double fixedPointSizePx READ fixedPointSizePx WRITE setFixedPointSizePx NOTIFY fixedPointSizePxChanged)
    Q_PROPERTY(double maxPointSizePx READ maxPointSizePx WRITE setMaxPointSizePx NOTIFY maxPointSizePxChanged)
    Q_PROPERTY(int debugView READ debugView WRITE setDebugView NOTIFY debugViewChanged)
    Q_PROPERTY(bool showNodeBoxes READ showNodeBoxes WRITE setShowNodeBoxes NOTIFY showNodeBoxesChanged)
    Q_PROPERTY(bool isAtDefaults READ isAtDefaults NOTIFY isAtDefaultsChanged)

    // The shared budget limits, exposed so the settings UI spin boxes range over
    // the same numbers the setters clamp to.
    Q_PROPERTY(int minimumGpuMemoryBudgetMb READ minimumGpuMemoryBudgetMb CONSTANT)
    Q_PROPERTY(int maximumGpuMemoryBudgetMb READ maximumGpuMemoryBudgetMb CONSTANT)
    Q_PROPERTY(int minimumCpuCacheBudgetMb READ minimumCpuCacheBudgetMb CONSTANT)
    Q_PROPERTY(int maximumCpuCacheBudgetMb READ maximumCpuCacheBudgetMb CONSTANT)
    Q_PROPERTY(int minimumUploadBudgetMbPerFrame READ minimumUploadBudgetMbPerFrame CONSTANT)
    Q_PROPERTY(int maximumUploadBudgetMbPerFrame READ maximumUploadBudgetMbPerFrame CONSTANT)
    Q_PROPERTY(double minimumScreenSpaceErrorPx READ minimumScreenSpaceErrorPx CONSTANT)
    Q_PROPERTY(double maximumScreenSpaceErrorPx READ maximumScreenSpaceErrorPx CONSTANT)
    Q_PROPERTY(int minimumPointBudgetMillions READ minimumPointBudgetMillions CONSTANT)
    Q_PROPERTY(int maximumPointBudgetMillions READ maximumPointBudgetMillions CONSTANT)
    Q_PROPERTY(int unlimitedLevel READ unlimitedLevel CONSTANT)
    Q_PROPERTY(int maximumLevel READ maximumLevel CONSTANT)
    Q_PROPERTY(int rootLevel READ rootLevel CONSTANT)
    Q_PROPERTY(int thinningModeCount READ thinningModeCount CONSTANT)
    Q_PROPERTY(int sizeModeCount READ sizeModeCount CONSTANT)
    Q_PROPERTY(int floorModeCount READ floorModeCount CONSTANT)
    Q_PROPERTY(int debugViewCount READ debugViewCount CONSTANT)
    Q_PROPERTY(double minimumPointSizePx READ minimumPointSizePx CONSTANT)
    Q_PROPERTY(double maximumPointSizePx READ maximumPointSizePx CONSTANT)

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

    int pointBudgetMillions() const { return m_pointBudgetMillions; }
    void setPointBudgetMillions(int millions);

    qint64 gpuBudgetBytes() const { return qint64(m_gpuMemoryBudgetMb) * cw::budgets::kBytesPerMegabyte; }

    bool freezeCut() const { return m_freezeCut; }
    void setFreezeCut(bool freeze);

    //! The deepest level the point cloud cut refines to, unlimitedLevel for no cap
    int maxLevel() const { return m_maxLevel; }
    void setMaxLevel(int level);

    //! The shallowest level the point clouds draw; the cut itself is unchanged
    int minLevel() const { return m_minLevel; }
    void setMinLevel(int level);

    bool inflationGovernor() const { return m_inflationGovernor; }
    void setInflationGovernor(bool governed);

    bool evictionHold() const { return m_evictionHold; }
    void setEvictionHold(bool hold);

    //! A cw::pointcloud::ThinningMode
    int thinningMode() const { return int(m_shading.thinningMode); }
    void setThinningMode(int mode);

    //! A cw::pointcloud::SizeMode
    int sizeMode() const { return int(m_shading.sizeMode); }
    void setSizeMode(int mode);

    //! A cw::pointcloud::FloorMode
    int floorMode() const { return int(m_shading.floorMode); }
    void setFloorMode(int mode);

    double fixedPointSizePx() const { return m_shading.fixedPointSizePx; }
    void setFixedPointSizePx(double pixels);

    double maxPointSizePx() const { return m_shading.maxPointSizePx; }
    void setMaxPointSizePx(double pixels);

    //! A cw::pointcloud::DebugView
    int debugView() const { return int(m_shading.debugView); }
    void setDebugView(int view);

    bool showNodeBoxes() const { return m_shading.showNodeBoxes; }
    void setShowNodeBoxes(bool show);

    //! The budget knobs in bytes, the form the render thread consumes
    cwRenderBudgets budgets() const;

    //! The shader-side point cloud knobs, the form every cloud's uniform takes
    const cwPointCloudShading& pointCloudShading() const { return m_shading; }

    static int minimumGpuMemoryBudgetMb() { return cw::budgets::kMinGpuBudgetMb; }
    static int maximumGpuMemoryBudgetMb() { return cw::budgets::kMaxGpuBudgetMb; }
    static int minimumCpuCacheBudgetMb() { return cw::budgets::kMinCpuBudgetMb; }
    static int maximumCpuCacheBudgetMb() { return cw::budgets::kMaxCpuBudgetMb; }
    static int minimumUploadBudgetMbPerFrame() { return cw::budgets::kMinUploadBudgetMbPerFrame; }
    static int maximumUploadBudgetMbPerFrame() { return cw::budgets::kMaxUploadBudgetMbPerFrame; }
    static double minimumScreenSpaceErrorPx() { return cw::budgets::kMinScreenSpaceErrorPx; }
    static double maximumScreenSpaceErrorPx() { return cw::budgets::kMaxScreenSpaceErrorPx; }
    static int minimumPointBudgetMillions() { return cw::budgets::kMinPointBudgetMillions; }
    static int maximumPointBudgetMillions() { return cw::budgets::kMaxPointBudgetMillions; }
    static int unlimitedLevel() { return cw::budgets::kUnlimitedLevel; }
    static int maximumLevel();
    static int rootLevel() { return cw::budgets::kRootLevel; }
    static int thinningModeCount() { return cw::pointcloud::kThinningModeCount; }
    static int sizeModeCount() { return cw::pointcloud::kSizeModeCount; }
    static int floorModeCount() { return cw::pointcloud::kFloorModeCount; }
    static int debugViewCount() { return cw::pointcloud::kDebugViewCount; }
    static double minimumPointSizePx() { return cw::pointcloud::kMinPointSizePx; }
    static double maximumPointSizePx() { return cw::pointcloud::kMaxPointSizePx; }

    bool isAtDefaults() const;
    Q_INVOKABLE void resetToDefaults();

    //! The budget, streaming and CLOD knobs alone, which is what the point cloud
    //! debug panel resets: MSAA and the HUD it is opened from stay as they are
    Q_INVOKABLE void resetPointCloudKnobsToDefaults();

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
    void pointBudgetMillionsChanged();
    void freezeCutChanged();
    void maxLevelChanged();
    void minLevelChanged();
    void inflationGovernorChanged();
    void evictionHoldChanged();
    void thinningModeChanged();
    void sizeModeChanged();
    void floorModeChanged();
    void fixedPointSizePxChanged();
    void maxPointSizePxChanged();
    void debugViewChanged();
    void showNodeBoxesChanged();

    //! Any one of the budgets() knobs moved
    void budgetsChanged();

    //! Any one of the pointCloudShading() knobs moved
    void pointCloudShadingChanged();
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

    // All five are overwritten from QSettings in the constructor.
    int m_gpuMemoryBudgetMb = cw::budgets::kDefaultGpuBudgetMb;
    int m_cpuCacheBudgetMb = cw::budgets::kDefaultCpuBudgetMb;
    int m_uploadBudgetMbPerFrame = cw::budgets::kDefaultUploadBudgetMbPerFrame;
    double m_screenSpaceErrorPx = cw::budgets::kDefaultScreenSpaceErrorPx;
    int m_pointBudgetMillions = cw::budgets::kDefaultPointBudgetMillions;

    // Overwritten from QSettings in the constructor, like the budgets.
    bool m_freezeCut = cw::budgets::kDefaultFreezeCut;
    int m_maxLevel = cw::budgets::kDefaultMaxLevel;
    int m_minLevel = cw::budgets::kDefaultMinLevel;
    bool m_inflationGovernor = cw::budgets::kDefaultInflationGovernor;
    bool m_evictionHold = cw::budgets::kDefaultEvictionHold;
    cwPointCloudShading m_shading;

    // Safe baseline until the QRhi backend reports the real set (see cwRhiScene).
    // Always kept sorted ascending and containing 1.
    QList<int> m_supportedSampleCounts = {1, 2, 4, 8};
};

#endif // CWRENDERINGSETTINGS_H
