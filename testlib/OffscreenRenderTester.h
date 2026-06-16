#ifndef OFFSCREENRENDERTESTER_H
#define OFFSCREENRENDERTESTER_H

// Our includes
#include "CaveWhereTestLibExport.h"

// Qt includes
#include <QObject>
#include <QSize>
#include <QString>
#include <qqmlintegration.h>
class QQuickItem;

/**
 * Test-only helpers for the offscreen-render QML regression test: a PID-tagged
 * temp path plus inspectors for the PNG that cw3dRegionViewer::renderToImage
 * writes. Lives in cavewhere-testlib so production code carries no test
 * scaffolding.
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
};

#endif // OFFSCREENRENDERTESTER_H
