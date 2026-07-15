#include "OffscreenRenderTester.h"
#include "LazFixtureHelper.h"
#include "cwRootData.h"
#include "cwRhiViewer.h"
#include "cwScene.h"
#include "cwGeometryItersecter.h"
#include "cwCamera.h"
#include "cwOffscreenRenderParameters.h"
#include "cwAppearanceOverride.h"
#include "cwPointCloudAppearance.h"
#include "cwRenderingSettings.h"
#include "cwRegionSceneManager.h"
#include "cwLazLayersSceneNode.h"
#include "cwLazLayer.h"
#include "cwRenderPointCloud.h"

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
#include <QtMath>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QVector3D>

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

// Every visible point cloud owned by @a manager's LAZ scene node, in model order.
QList<cwRenderPointCloud*> visiblePointClouds(cwRegionSceneManager* manager)
{
    QList<cwRenderPointCloud*> clouds;
    if (!manager) {
        return clouds;
    }
    cwLazLayersSceneNode* node = manager->lazLayersSceneNode();
    if (!node) {
        return clouds;
    }
    const QList<cwLazLayer*> layers = node->visibleLayers();
    for (cwLazLayer* layer : layers) {
        if (cwRenderPointCloud* cloud = node->pointCloudForLayer(layer)) {
            clouds.append(cloud);
        }
    }
    return clouds;
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

int OffscreenRenderTester::visiblePointCloudCount(QObject* sceneManager) const
{
    return int(visiblePointClouds(qobject_cast<cwRegionSceneManager*>(sceneManager)).size());
}

bool OffscreenRenderTester::buildFramedCloudParameters(QQuickItem* viewer, QObject* sceneManager,
                                                       QSize size, double worldRadiusOverride,
                                                       cwScene*& sceneOut,
                                                       cwOffscreenRenderParameters& parametersOut)
{
    auto* rhiViewer = qobject_cast<cwRhiViewer*>(viewer);
    auto* manager = qobject_cast<cwRegionSceneManager*>(sceneManager);
    if (!rhiViewer || !manager) {
        qWarning() << "buildFramedCloudParameters: bad viewer or sceneManager";
        return false;
    }
    cwScene* sceneObject = rhiViewer->scene();
    if (!sceneObject || size.isEmpty()) {
        qWarning() << "buildFramedCloudParameters: no scene or empty size";
        return false;
    }
    const QList<cwRenderPointCloud*> clouds = visiblePointClouds(manager);
    if (clouds.isEmpty()) {
        qWarning() << "buildFramedCloudParameters: no visible point cloud";
        return false;
    }
    cwRenderPointCloud* cloud = clouds.first();

    // Frame the cloud's scene-space bounds head-on. The distance must satisfy two
    // things at once: (a) the whole bbox fits the frustum, and (b) the LIVE
    // world-radius renders well below the shader's maxPointSizePx clamp — a frame
    // too tight saturates the live sprites at the clamp, where a larger override
    // radius can't grow them and the difference vanishes (an earlier bounds-only
    // version did exactly this and the test could not tell the radii apart). So take
    // the farther of two distances: bounds-fit, and the distance at which the live
    // radius projects to ~kTargetLivePx (mirrors the size formula in PointCloud.vert,
    // dpr 1, point near the view axis so w ≈ distance).
    const QVector3D bboxMin = cloud->bboxMin();
    const QVector3D bboxMax = cloud->bboxMax();
    const QVector3D center = (bboxMin + bboxMax) * 0.5f;
    const float boundsRadius = qMax(0.5f * (bboxMax - bboxMin).length(), 1.0f);

    constexpr float kFovDeg = 45.0f;
    constexpr float kFrameMargin = 1.5f;
    constexpr float kTargetLivePx = 4.0f;
    const float halfFov = qDegreesToRadians(kFovDeg * 0.5f);
    const float liveRadius = cloud->worldRadius();
    const float distanceForBounds = boundsRadius / std::sin(halfFov) * kFrameMargin;
    const float distanceForSprite = (liveRadius > 0.0f)
        ? liveRadius * (1.0f / std::tan(halfFov)) * (size.height() * 0.5f) / kTargetLivePx
        : 0.0f;
    const float distance = qMax(distanceForBounds, distanceForSprite);
    const QVector3D eye = center + QVector3D(0.0f, 0.0f, distance);

    QMatrix4x4 viewMatrix;
    viewMatrix.lookAt(eye, center, QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projectionMatrix;
    projectionMatrix.perspective(kFovDeg, 1.0f,
                                 qMax(0.01f, distance - boundsRadius * 2.0f),
                                 distance + boundsRadius * 2.0f);

    parametersOut = cwOffscreenRenderParameters{};
    parametersOut.viewMatrix = viewMatrix;
    parametersOut.projectionMatrix = projectionMatrix;
    parametersOut.outputSize = size;
    parametersOut.devicePixelRatio = 1.0f;
    parametersOut.sampleCount = 1;
    parametersOut.backgroundColor = QColor::fromRgbF(0.0, 0.0, 0.0, 0.0); // transparent clear
    parametersOut.hiddenObjectIds = manager->captureHiddenObjectIds();    // isolate the cloud

    // Attach the per-job appearance override on the job itself (the on-job payload
    // path), keyed by the cloud's stable render-object id.
    if (worldRadiusOverride > 0.0) {
        cwPointCloudAppearance appearance;
        appearance.worldRadius = float(worldRadiusOverride);
        parametersOut.appearanceOverrides.insert(cloud->renderObjectId(),
                                                 cwAppearanceOverride(appearance));
    }

    sceneOut = sceneObject;
    return true;
}

void OffscreenRenderTester::renderPointCloudFramed(QQuickItem* viewer, QObject* sceneManager,
                                                   const QString& filePath, QSize size,
                                                   double worldRadiusOverride)
{
    if (filePath.isEmpty() || !QFileInfo(filePath).absoluteDir().exists()) {
        qWarning() << "renderPointCloudFramed: empty path or missing output directory" << filePath;
        return;
    }
    cwScene* sceneObject = nullptr;
    cwOffscreenRenderParameters parameters;
    if (!buildFramedCloudParameters(viewer, sceneManager, size, worldRadiusOverride,
                                    sceneObject, parameters)) {
        return;
    }
    saveOffscreenRender(sceneObject, parameters, filePath);
}

int OffscreenRenderTester::renderPointCloudFramedPair(QQuickItem* viewer, QObject* sceneManager,
                                                      const QString& filePathA, double worldRadiusA,
                                                      const QString& filePathB, double worldRadiusB,
                                                      QSize size)
{
    cwScene* sceneObject = nullptr;
    cwOffscreenRenderParameters paramsA;
    cwOffscreenRenderParameters paramsB;
    if (!buildFramedCloudParameters(viewer, sceneManager, size, worldRadiusA, sceneObject, paramsA)
        || !buildFramedCloudParameters(viewer, sceneManager, size, worldRadiusB, sceneObject, paramsB)) {
        return 0;
    }

    // Issue BOTH before returning to the event loop so they reach the render queue in
    // one sync and drain through the atlas batch path — both override slots live at once.
    QFuture<QImage> futureA = sceneObject->renderOffscreen(paramsA);
    QFuture<QImage> futureB = sceneObject->renderOffscreen(paramsB);

    auto combine = AsyncFuture::combine(AsyncFuture::AllSettled);
    combine << futureA << futureB;
    AsyncFuture::waitForFinished(combine.future(), 30000); // test-only nested loop

    int written = 0;
    const QPair<QFuture<QImage>, QString> outputs[2] = {
        { futureA, filePathA }, { futureB, filePathB }
    };
    for (const auto& [future, path] : outputs) {
        if (future.isFinished() && future.resultCount() == 1 && !future.result().isNull()
            && future.result().save(path)) {
            ++written;
        } else {
            qWarning() << "renderPointCloudFramedPair: no image written for" << path;
        }
    }
    return written;
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

QBox3D OffscreenRenderTester::sceneBoundingBox(cwScene* scene) const
{
    if (!scene || !scene->geometryItersecter()) {
        return QBox3D();
    }
    return scene->geometryItersecter()->boundingBox();
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

    saveOffscreenRender(sceneObject, parameters, filePath);
}

void OffscreenRenderTester::saveOffscreenRender(cwScene* scene,
                                                const cwOffscreenRenderParameters& parameters,
                                                const QString& filePath)
{
    QFuture<QImage> future = scene->renderOffscreen(parameters);
    AsyncFuture::observe(future).context(this, [filePath](QImage image) {
        if (image.isNull()) {
            qWarning() << "saveOffscreenRender: offscreen render produced no image" << filePath;
            return;
        }
        if (!image.save(filePath)) {
            qWarning() << "saveOffscreenRender: failed to write image to" << filePath
                       << "(check the extension and write permissions)";
        }
    });
}

void OffscreenRenderTester::renderTransparentImage(QQuickItem* viewer, QObject* sceneManager,
                                                   const QString& filePath, QSize size,
                                                   bool hideExportChrome)
{
    auto* rhiViewer = qobject_cast<cwRhiViewer*>(viewer);
    if (!rhiViewer) {
        qWarning() << "renderTransparentImage: item is not a cwRhiViewer";
        return;
    }
    cwScene* sceneObject = rhiViewer->scene();
    cwCamera* cameraObject = rhiViewer->camera();
    if (!sceneObject || !cameraObject || filePath.isEmpty() || size.isEmpty()) {
        qWarning() << "renderTransparentImage: no scene/camera, empty path, or empty size";
        return;
    }
    const QDir outputDir = QFileInfo(filePath).absoluteDir();
    if (!outputDir.exists()) {
        qWarning() << "renderTransparentImage: output directory does not exist"
                   << outputDir.absolutePath();
        return;
    }

    cwOffscreenRenderParameters parameters;
    parameters.viewMatrix = cameraObject->viewMatrix();
    parameters.projectionMatrix = cameraObject->projectionMatrix();
    parameters.outputSize = size;
    parameters.devicePixelRatio = 1.0f;
    parameters.sampleCount = 1;
    parameters.backgroundColor = QColor::fromRgbF(0.0, 0.0, 0.0, 0.0); // transparent clear

    if (hideExportChrome) {
        auto* manager = qobject_cast<cwRegionSceneManager*>(sceneManager);
        if (!manager) {
            qWarning() << "renderTransparentImage: hideExportChrome set but sceneManager is not a"
                          " cwRegionSceneManager";
            return;
        }
        parameters.hiddenObjectIds = manager->captureHiddenObjectIds();
    }

    saveOffscreenRender(sceneObject, parameters, filePath);
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
