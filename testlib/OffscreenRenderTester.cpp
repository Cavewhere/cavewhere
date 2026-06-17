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
#include <QMatrix4x4>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
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

// Opaque fraction above which an embedded SVG tile counts as carrying real drawn
// content (rather than a transparent/empty edge tile).
constexpr double kSvgContentOpaqueThreshold = 0.001;

// Pull the base64 payloads of every embedded raster tile that carries content out
// of an SVG export. QSvgGenerator writes each painted QImage as its own
// <image ... base64,...> element, so one entry per rendered map tile; the
// transparent edge/empty tiles are filtered out by opaque fraction. The repeated-
// tile offscreen bug made same-frame tiles read back identical, so two content
// tiles would share a byte-identical (hence base64-identical) payload here.
QList<QByteArray> svgContentTileBlobs(const QString& svgPath)
{
    QList<QByteArray> blobs;
    QFile file(svgPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return blobs;
    }
    const QByteArray svg = file.readAll();
    const QByteArray marker = "base64,";
    qsizetype pos = 0;
    while ((pos = svg.indexOf(marker, pos)) >= 0) {
        pos += marker.size();
        const qsizetype end = svg.indexOf('"', pos);
        if (end < 0) {
            break;
        }
        const QByteArray base64 = svg.mid(pos, end - pos);
        pos = end;
        const QImage tile = QImage::fromData(QByteArray::fromBase64(base64));
        const OpaqueScan scan = scanOpaque(tile);
        if (scan.total > 0
            && double(scan.count) / double(scan.total) > kSvgContentOpaqueThreshold) {
            blobs.append(base64);
        }
    }
    return blobs;
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

int OffscreenRenderTester::renderPannedBatch(QQuickItem* viewer, const QString& dirPath,
                                             const QString& tag, int count, int tileSize)
{
    auto* rhiViewer = qobject_cast<cwRhiViewer*>(viewer);
    if (!rhiViewer) {
        qWarning() << "renderPannedBatch: item is not a cwRhiViewer";
        return 0;
    }
    cwScene* sceneObject = rhiViewer->scene();
    cwCamera* cameraObject = rhiViewer->camera();
    if (!sceneObject || !cameraObject || count < 1 || tileSize < 1) {
        qWarning() << "renderPannedBatch: bad arguments";
        return 0;
    }
    const QDir outputDir(dirPath);
    if (!outputDir.exists()) {
        qWarning() << "renderPannedBatch: output directory does not exist" << dirPath;
        return 0;
    }

    const QMatrix4x4 baseView = cameraObject->viewMatrix();
    const QMatrix4x4 projection = cameraObject->projectionMatrix();

    // Per-index pan in view space so each tile frames a distinct crop. Laid out as a
    // bounded 2D raster (col across, row down) rather than a single growing axis, so even
    // a several-hundred-tile spillover batch stays on-scene and pairwise distinct instead
    // of drifting off into a blank, uniform frame. Index 0 is (0,0) — unpanned — so its
    // image matches the single-tile reference render bit-for-bit.
    constexpr float kPanStep = 0.15f;
    constexpr int kPanGridStride = 16;

    // Issue the whole batch before returning to the event loop, so all jobs reach the
    // render queue in one sync and a count > 1 drains through the atlas path.
    QList<QFuture<QImage>> futures;
    futures.reserve(count);
    for (int i = 0; i < count; ++i) {
        QMatrix4x4 view = baseView;
        view.translate(kPanStep * float(i % kPanGridStride),
                        kPanStep * float(i / kPanGridStride), 0.0f);

        cwOffscreenRenderParameters parameters;
        parameters.viewMatrix = view;
        parameters.projectionMatrix = projection;
        parameters.outputSize = QSize(tileSize, tileSize);
        parameters.devicePixelRatio = 1.0f;
        parameters.sampleCount = 1;
        parameters.backgroundColor = QColor::fromRgbF(0.0, 0.0, 0.0, 1.0);
        futures.append(sceneObject->renderOffscreen(parameters));
    }

    // waitForFinished is test-only (it spins a nested event loop) — fine here, and it lets
    // the render thread drain the queued batch across however many frames spillover needs.
    auto combine = AsyncFuture::combine(AsyncFuture::AllSettled);
    for (const auto& future : std::as_const(futures)) {
        combine << future;
    }
    AsyncFuture::waitForFinished(combine.future(), 30000);

    int written = 0;
    for (int i = 0; i < count; ++i) {
        const QFuture<QImage>& future = futures.at(i);
        if (!future.isFinished() || future.resultCount() != 1 || future.result().isNull()) {
            qWarning() << "renderPannedBatch: tile" << i << "produced no image";
            continue;
        }
        const QString path = outputDir.filePath(QStringLiteral("%1_%2.png").arg(tag).arg(i));
        if (future.result().save(path)) {
            ++written;
        } else {
            qWarning() << "renderPannedBatch: failed to write" << path;
        }
    }
    return written;
}

bool OffscreenRenderTester::imagesEqual(const QString& pathA, const QString& pathB) const
{
    const QImage a(pathA);
    const QImage b(pathB);
    if (a.isNull() || b.isNull() || a.size() != b.size()) {
        return false;
    }
    return a.convertToFormat(QImage::Format_ARGB32) == b.convertToFormat(QImage::Format_ARGB32);
}

QString OffscreenRenderTester::tempPngPath(const QString& tag) const
{
    return QDir(QDir::tempPath()).filePath(
        QStringLiteral("cw_%1_%2.png").arg(tag).arg(QCoreApplication::applicationPid()));
}

QString OffscreenRenderTester::makeTempDir(const QString& tag) const
{
    const QString path = QDir(QDir::tempPath()).filePath(
        QStringLiteral("cw_%1_%2").arg(tag).arg(QCoreApplication::applicationPid()));
    QDir().mkpath(path);
    return path;
}

bool OffscreenRenderTester::removeDir(const QString& path) const
{
    return QDir(path).removeRecursively();
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

int OffscreenRenderTester::svgContentTileCount(const QString& svgPath) const
{
    return int(svgContentTileBlobs(svgPath).size());
}

int OffscreenRenderTester::svgDistinctContentTileCount(const QString& svgPath) const
{
    const QList<QByteArray> blobs = svgContentTileBlobs(svgPath);
    const QSet<QByteArray> distinct(blobs.constBegin(), blobs.constEnd());
    return int(distinct.size());
}
