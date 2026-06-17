#include "OffscreenRenderTester.h"
#include "LazFixtureHelper.h"
#include "cwRootData.h"
#include "cwRhiViewer.h"
#include "cwScene.h"
#include "cwCamera.h"
#include "cwOffscreenRenderParameters.h"
#include "cwRenderingSettings.h"

// Qt includes
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>

#include <asyncfuture.h>

namespace {
// Alpha (0..255) above which a pixel counts as drawn content rather than the
// transparent clear; ignores the near-transparent anti-aliased fringe.
constexpr int kMinOpaqueAlpha = 16;

// Single pass over an image's alpha, gathering everything the opaque-* inspectors
// need (count, total, and the x/y sums for the centroid) so they don't each
// re-scan. Reads alpha straight from the loaded buffer when it already carries it
// (the export PNG is RGBA); converts only when it doesn't. qAlpha is correct for
// premultiplied formats too — premultiplication leaves the alpha byte untouched.
struct OpaqueScan {
    qsizetype count = 0;   // pixels with alpha > kMinOpaqueAlpha
    qsizetype total = 0;   // all pixels
    double sumX = 0.0;     // summed x of opaque pixels
    double sumY = 0.0;     // summed y of opaque pixels
    int width = 0;
    int height = 0;
};

OpaqueScan scanOpaque(const QImage& source)
{
    OpaqueScan scan;
    if (source.isNull()) {
        return scan;
    }
    const QImage image = (source.format() == QImage::Format_ARGB32
                          || source.format() == QImage::Format_ARGB32_Premultiplied)
                             ? source
                             : source.convertToFormat(QImage::Format_ARGB32);
    scan.width = image.width();
    scan.height = image.height();
    scan.total = qsizetype(image.width()) * image.height();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > kMinOpaqueAlpha) {
                ++scan.count;
                scan.sumX += x;
                scan.sumY += y;
            }
        }
    }
    return scan;
}
} // namespace

OffscreenRenderTester::OffscreenRenderTester(QObject* parent) :
    QObject(parent)
{
}

bool OffscreenRenderTester::addSyntheticPointCloud(QObject* rootData)
{
    auto* root = qobject_cast<cwRootData*>(rootData);
    if (!root) {
        return false;
    }

    QTemporaryDir dir;
    if (!dir.isValid()) {
        return false;
    }
    // addLazAndWait copies the file into the project, so the temp can vanish after.
    const QString lazPath = tempLazPath(dir, QStringLiteral("offscreen_diag"));
    writeMinimalLaz(lazPath);
    addLazAndWait(root, QStringList{lazPath});
    return true;
}

bool OffscreenRenderTester::windowHasRhi(QQuickItem* item) const
{
    if (!item || !item->window()) {
        return false;
    }
    QSGRendererInterface* renderer = item->window()->rendererInterface();
    if (!renderer) {
        return false;
    }
    return renderer->getResource(item->window(), QSGRendererInterface::RhiResource) != nullptr;
}

void OffscreenRenderTester::renderToImage(QQuickItem* viewer, const QString& filePath,
                                          QSize size, int sampleCount)
{
    auto* rhiViewer = qobject_cast<cwRhiViewer*>(viewer);
    if (!rhiViewer) {
        qWarning() << "renderToImage: item is not a cwRhiViewer";
        return;
    }

    cwScene* sceneObject = rhiViewer->scene();
    cwCamera* cameraObject = rhiViewer->camera();
    if (!sceneObject || !cameraObject || filePath.isEmpty()) {
        qWarning() << "renderToImage: no scene/camera or empty path";
        return;
    }

    // Catch a bad destination here, on the GUI thread, so the caller gets an
    // immediate, specific failure instead of a misleading "failed to save" after
    // the async render completes.
    const QDir outputDir = QFileInfo(filePath).absoluteDir();
    if (!outputDir.exists()) {
        qWarning() << "renderToImage: output directory does not exist" << outputDir.absolutePath();
        return;
    }

    // Empty size means "render what's on screen": mirror the live framebuffer by
    // rendering at the physical (DPR-scaled) resolution and feeding the live DPR,
    // so DPR-dependent geometry — point-cloud point size scales by it in the
    // shader — matches the live view exactly. An explicit size is a
    // display-independent render, so it keeps DPR at 1 to stay consistent across
    // machines.
    const double dpr = cameraObject->devicePixelRatio();
    QSize outputSize;
    float requestDevicePixelRatio;
    int requestSampleCount;
    if (size.isEmpty()) {
        const QSize logical = cameraObject->viewport().size();
        outputSize = QSize(qRound(logical.width() * dpr), qRound(logical.height() * dpr));
        requestDevicePixelRatio = float(dpr);
        // Auto: mirror the live framebuffer's MSAA so the "render what's on screen"
        // image is a faithful copy (anti-aliased edges match the live view).
        requestSampleCount = (sampleCount < 0) ? cwRenderingSettings::instance()->sampleCount()
                                               : sampleCount;
    } else {
        outputSize = size;
        requestDevicePixelRatio = 1.0f;
        // Auto: a display-independent export defaults to no AA. A caller can still
        // force MSAA by passing an explicit sampleCount.
        requestSampleCount = (sampleCount < 0) ? 1 : sampleCount;
    }
    if (outputSize.isEmpty()) {
        qWarning() << "renderToImage: empty output size";
        return;
    }

    // The offscreen path applies the RHI clip-space correction itself, so hand it
    // the camera's raw projection (matching how the live frame feeds the scene).
    cwOffscreenRenderParameters parameters;
    parameters.viewMatrix = cameraObject->viewMatrix();
    parameters.projectionMatrix = cameraObject->projectionMatrix();
    parameters.outputSize = outputSize;
    parameters.devicePixelRatio = requestDevicePixelRatio;
    parameters.sampleCount = requestSampleCount;
    parameters.backgroundColor = QColor::fromRgbF(0.0, 0.0, 0.0, 1.0);

    QFuture<QImage> future = sceneObject->renderOffscreen(parameters);

    AsyncFuture::observe(future).context(this, [filePath](QImage image) {
        if (image.isNull()) {
            qWarning() << "renderToImage: offscreen render produced no image" << filePath;
            return;
        }
        if (!image.save(filePath)) {
            qWarning() << "renderToImage: failed to write image to" << filePath
                       << "(check the extension and write permissions)";
        }
    });
}

QString OffscreenRenderTester::tempPngPath(const QString& tag) const
{
    return QDir(QDir::tempPath()).filePath(
        QStringLiteral("cw_%1_%2.png").arg(tag).arg(QCoreApplication::applicationPid()));
}

bool OffscreenRenderTester::fileExists(const QString& path) const
{
    return QFile::exists(path);
}

bool OffscreenRenderTester::removeFile(const QString& path) const
{
    return QFile::exists(path) ? QFile::remove(path) : true;
}

QSize OffscreenRenderTester::imageSize(const QString& path) const
{
    return QImage(path).size();
}

bool OffscreenRenderTester::imageIsNonUniform(const QString& path) const
{
    const QImage image(path);
    if (image.isNull()) {
        return false;
    }

    const QRgb first = image.pixel(0, 0);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y) != first) {
                return true;
            }
        }
    }
    return false;
}

double OffscreenRenderTester::nonBlackFraction(const QString& path) const
{
    const QImage image(path);
    if (image.isNull()) {
        return 0.0;
    }

    constexpr int kNearBlack = 16; // 0..255 per channel
    qsizetype nonBlack = 0;
    const qsizetype total = qsizetype(image.width()) * image.height();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb p = image.pixel(x, y);
            if (qRed(p) > kNearBlack || qGreen(p) > kNearBlack || qBlue(p) > kNearBlack) {
                ++nonBlack;
            }
        }
    }
    return total > 0 ? double(nonBlack) / double(total) : 0.0;
}

double OffscreenRenderTester::opaqueFraction(const QString& path) const
{
    const OpaqueScan scan = scanOpaque(QImage(path));
    return scan.total > 0 ? double(scan.count) / double(scan.total) : 0.0;
}

QPointF OffscreenRenderTester::opaqueCentroid(const QString& path) const
{
    const OpaqueScan scan = scanOpaque(QImage(path));
    if (scan.count == 0) {
        return QPointF(-1.0, -1.0);
    }
    return QPointF((scan.sumX / scan.count) / scan.width,
                   (scan.sumY / scan.count) / scan.height);
}
