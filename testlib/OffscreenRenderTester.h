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

    // PID-tagged path under the system temp dir so parallel test processes
    // never collide.
    Q_INVOKABLE QString tempPngPath(const QString& tag) const;
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
};

#endif // OFFSCREENRENDERTESTER_H
