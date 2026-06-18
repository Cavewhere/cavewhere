#ifndef OFFSCREENRENDERTESTER_H
#define OFFSCREENRENDERTESTER_H

// Our includes
#include "CaveWhereTestLibExport.h"

// Qt includes
#include <QObject>
#include <QPointF>
#include <QSize>
#include <QString>
#include <qqmlintegration.h>
class QQuickItem;
class cwScene;
struct cwOffscreenRenderParameters;

/**
 * Test-only helpers for the render-to-PNG QML regression tests (the offscreen
 * renderer and the tiled map export): the renderToImage driver itself, plus a
 * PID-tagged temp path and content inspectors for a saved PNG (size, uniformity,
 * non-black / opaque fraction, opaque centroid). Lives in cavewhere-testlib so
 * production code carries no test scaffolding.
 */
class CAVEWHERE_TESTLIB_EXPORT OffscreenRenderTester : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(OffscreenRenderTester)
    QML_SINGLETON

public:
    explicit OffscreenRenderTester(QObject* parent = nullptr);

    // Write a tiny synthetic .laz and add it to rootData's region, so a point
    // cloud is visible and the offscreen render engages the EDL composite path.
    // rootData is the cwRootData singleton (passed as QObject* to avoid pulling
    // the type into this header). Returns true on success.
    Q_INVOKABLE bool addSyntheticPointCloud(QObject* rootData);

    // Set a per-appearance-slot world-radius override on every visible point
    // cloud, so an offscreen render selecting @a slot draws the cloud at @a
    // worldRadius while the live view (slot 0) is unaffected. @a sceneManager is
    // the cwRegionSceneManager (QObject* to keep this header free of the type).
    // Drives the Tier-2 per-object appearance rail. Returns the number of clouds
    // set (0 = none found).
    Q_INVOKABLE int setPointCloudWorldRadiusOverride(QObject* sceneManager, int slot,
                                                     double worldRadius);

    // Render the first visible point cloud, framed to its own bounds with the
    // export chrome hidden over a transparent clear, at appearance slot @a
    // appearanceSlot — so the opaque region is exactly the cloud and its coverage
    // tracks that slot's world-radius. The cloud is framed (not the live camera)
    // so the assertion never depends on where the on-screen view happens to look.
    Q_INVOKABLE void renderPointCloudFramed(QQuickItem* viewer, QObject* sceneManager,
                                            const QString& filePath, QSize size,
                                            int appearanceSlot);

    // True when the item's window has a live QRhi device. False under the
    // headless `offscreen` QPA, which provides none — the readback path then
    // can't run, so the test skips rather than fails.
    Q_INVOKABLE bool windowHasRhi(QQuickItem* item) const;

    // Render the viewer's resident scene through the offscreen path with its
    // current camera and save the result to filePath as a PNG. This is the test
    // driver for a real-RHI offscreen render + texture read-back; it lives here,
    // not on the production viewer, so cw3dRegionViewer carries no debug-only
    // scaffolding. An empty size renders at the camera's viewport and DPR; an
    // explicit size is a display-independent render at DPR 1. sampleCount selects
    // MSAA: -1 (auto) mirrors the live framebuffer for the empty-size path and 1x
    // for an explicit size; a value >= 1 forces that count. viewer must be a
    // cwRhiViewer (passed as QQuickItem* to keep this header free of that type).
    Q_INVOKABLE void renderToImage(QQuickItem* viewer, const QString& filePath,
                                   QSize size = QSize(), int sampleCount = -1);

    // Render the resident scene to a transparent-backed PNG (alpha-0 clear, so drawn
    // geometry is exactly the opaque region — opaqueFraction is meaningful). When
    // @a hideExportChrome is true, the map-export chrome (background gradient, line
    // plot, grid plane) is suppressed for this render only via the per-job
    // hiddenObjectIds override — the exact mechanism the tiled exporter uses, fed from
    // @a sceneManager's captureHiddenObjectIds(). @a sceneManager is the
    // cwRegionSceneManager (passed as QObject* to keep this header free of the type);
    // it may be null only when @a hideExportChrome is false. A display-independent
    // square render at DPR 1, no MSAA.
    Q_INVOKABLE void renderTransparentImage(QQuickItem* viewer, QObject* sceneManager,
                                            const QString& filePath, QSize size,
                                            bool hideExportChrome);

    // Render @a count offscreen images of the resident scene, each panned by a distinct
    // per-index amount so the tiles differ, all issued before the render thread drains so
    // @a count > 1 batches through the atlas path (the way the map exporter and other
    // batched offscreen consumers queue their tiles). Saves them to <dirPath>/<tag>_<i>.png and returns
    // how many were written. @a count == 1 takes the single-tile path (the parity
    // reference); index 0 is the unpanned camera, so the index-0 image is identical across
    // batch sizes. A display-independent square render at @a tileSize px, DPR 1.
    Q_INVOKABLE int renderPannedBatch(QQuickItem* viewer, const QString& dirPath,
                                      const QString& tag, int count, int tileSize);

    // True when the two saved images have identical dimensions and pixels (compared in a
    // common format). The atlas slice of a tile must equal its standalone single-tile
    // render, and distinct-camera tiles must not be equal.
    Q_INVOKABLE bool imagesEqual(const QString& pathA, const QString& pathB) const;

    // PID-tagged path under the system temp dir so parallel test processes
    // never collide.
    Q_INVOKABLE QString tempPngPath(const QString& tag) const;
    // Create and return a PID-tagged directory under the system temp dir (for a batch of
    // tile PNGs); removeDir tears it down.
    Q_INVOKABLE QString makeTempDir(const QString& tag) const;
    Q_INVOKABLE bool removeDir(const QString& path) const;
    Q_INVOKABLE bool fileExists(const QString& path) const;
    Q_INVOKABLE bool removeFile(const QString& path) const;
    Q_INVOKABLE QSize imageSize(const QString& path) const;
    // True when the saved image holds more than one distinct pixel colour — a
    // real rendered scene (the radial-gradient background) rather than a flat
    // clear.
    Q_INVOKABLE bool imageIsNonUniform(const QString& path) const;
    // Fraction (0..1) of pixels that are not near-black. Distinguishes a scene
    // that fills the frame (gradient background ≈ 1.0) from the EDL-composite
    // regression that painted everything black except a tiny cloud (≈ 0).
    Q_INVOKABLE double nonBlackFraction(const QString& path) const;

    // Fraction (0..1) of pixels whose alpha is above a small threshold — i.e. real
    // rendered content over a transparent clear. The map-export PNG fills with
    // Qt::transparent, so its drawn geometry is exactly the opaque region; a value
    // near 0 means a blank export. Returns 1.0 for an image with no alpha channel.
    Q_INVOKABLE double opaqueFraction(const QString& path) const;

    // Center of mass of the opaque pixels, normalized to 0..1 in each axis (x to the
    // right, y downward). A robust placement/orientation probe: unlike a tight
    // bounding box it is not thrown off by a few stray edge pixels (the map export
    // leaves single opaque pixels at the page corners). A vertical flip maps y -> 1-y;
    // a blank or grossly mis-rendered export moves it. Returns (-1,-1) for a null or
    // fully transparent image.
    Q_INVOKABLE QPointF opaqueCentroid(const QString& path) const;

    // Map-export tile-uniqueness probes. The tiled exporter renders each tile
    // through the offscreen renderer; a multi-job batching bug once made tiles
    // rendered in the same frame read back identical (every tile a copy of the
    // last). SVG export writes each tile as its own base64 <image>, so these parse
    // them out: svgContentTileCount is how many tiles carry real (non-transparent)
    // content, and svgDistinctContentTileCount how many of those are byte-distinct.
    // A correct export keeps the two equal; the bug collapses the distinct count.
    Q_INVOKABLE int svgContentTileCount(const QString& svgPath) const;
    Q_INVOKABLE int svgDistinctContentTileCount(const QString& svgPath) const;

private:
    // Shared tail of the single-image render-to-PNG helpers: queue the offscreen
    // render and, when it completes, save the QImage to filePath (warning on a null
    // result or a write failure). The render parameters and destination differ per
    // caller; this queue-and-save plumbing does not.
    void saveOffscreenRender(cwScene* scene, const cwOffscreenRenderParameters& parameters,
                             const QString& filePath);
};

#endif // OFFSCREENRENDERTESTER_H
