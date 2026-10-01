// Real-QRhi reproduction of the multi-tile export crash: a streamed texture swap
// that lands while an offscreen target rebuild has nulled the item's pipeline
// record leaves its SRB pointing at the deleted texture, and the next live frame
// draws with it. Needs a GPU-backed window; skips under the offscreen QPA.

#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCamera.h"
#include "cwDiskCacher.h"
#include "cwGeometry.h"
#include "cwKtx2Codec.h"
#include "cwOffscreenRenderParameters.h"
#include "cwProjection.h"
#include "cwRenderTexturedItems.h"
#include "cwRhiViewer.h"
#include "cwScene.h"
#include "cwStreamedTexture.h"
#include "cwTextureStreamingStats.h"

//Qt includes
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFuture>
#include <QGuiApplication>
#include <QImage>
#include <QList>
#include <QMatrix4x4>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QThread>
#include <QVector2D>
#include <QVector3D>

//Std includes
#include <atomic>

namespace {

constexpr int kWindowSize = 800;
constexpr int kTextureDimension = 1024;
constexpr float kQuadHalfExtent = 1.0f;

// Item B sits at the origin where the live camera looks; item A sits far off to
// the side where only the offscreen tiles look, so no tile's frustum holds B.
const QVector3D kItemBCenter(0.0f, 0.0f, 0.0f);
const QVector3D kItemACenter(100.0f, 0.0f, 0.0f);

constexpr float kFieldOfView = 45.0f;
constexpr float kNearPlane = 0.05f;
constexpr float kFarPlane = 1000.0f;

// Far enough that B only wants its pinned base, close enough that it wants level 0.
constexpr float kLiveFarDistance = 200.0f;
constexpr float kLiveNearDistance = 0.3f;
constexpr float kTileDistance = 5.0f;

// Two tile sizes, alternated like a full tile and a cropped edge tile, so every
// offscreen job rebuilds the scratch target and evicts its pipelines.
const QSize kFullTileSize(96, 96);
const QSize kCroppedTileSize(64, 64);
constexpr int kOutstandingJobs = 64;

constexpr int kWaitTimeoutMs = 60000;
constexpr int kPollIntervalMs = 2;
constexpr int kSettleFrames = 60;

template <typename Predicate>
bool waitFor(Predicate predicate, int timeoutMs = kWaitTimeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        if (predicate()) {
            return true;
        }
        QThread::msleep(kPollIntervalMs);
    }
    return predicate();
}

cwGeometry quadAt(const QVector3D& center)
{
    cwGeometry geometry(cwRenderTexturedItems::geometryLayout());
    geometry.resizeVertices(4);
    const auto* position = geometry.attribute(cwGeometry::Semantic::Position);
    const auto* texCoord = geometry.attribute(cwGeometry::Semantic::TexCoord0);

    const QVector<QVector3D> corners = {
        QVector3D(-kQuadHalfExtent, -kQuadHalfExtent, 0.0f),
        QVector3D( kQuadHalfExtent, -kQuadHalfExtent, 0.0f),
        QVector3D( kQuadHalfExtent,  kQuadHalfExtent, 0.0f),
        QVector3D(-kQuadHalfExtent,  kQuadHalfExtent, 0.0f),
    };
    const QVector<QVector2D> uvs = {
        QVector2D(0.0f, 0.0f), QVector2D(1.0f, 0.0f),
        QVector2D(1.0f, 1.0f), QVector2D(0.0f, 1.0f),
    };
    for (int i = 0; i < corners.size(); ++i) {
        geometry.set(position, i, center + corners.at(i));
        geometry.set(texCoord, i, uvs.at(i));
    }
    geometry.setIndices({0u, 1u, 2u, 0u, 2u, 3u});
    geometry.setType(cwGeometry::Type::Triangles);
    return geometry;
}

QImage patternImage(int seed)
{
    QImage image(QSize(kTextureDimension, kTextureDimension), QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(x, y, QColor((x + seed) % 256, (y * 3) % 256, (x ^ y) % 256));
        }
    }
    return image;
}

cwStreamedTexture encodeStreamedTexture(const QDir& dataRoot, const QString& id, int seed)
{
    cwDiskCacher cacher(dataRoot);
    const cwDiskCacher::Key key {id, QDir(QStringLiteral("textures")), QStringLiteral("checksum-") + id};
    const auto encoded = cw::ktx2::cachedCompressedTexture(cacher, key, patternImage(seed),
                                                           QRhiTexture::RGBA8);
    REQUIRE_FALSE(encoded.hasError());
    return cwStreamedTexture {dataRoot.absolutePath(), key, QSize(kTextureDimension, kTextureDimension)};
}

QMatrix4x4 perspective(double aspect)
{
    QMatrix4x4 projection;
    projection.perspective(kFieldOfView, float(aspect), kNearPlane, kFarPlane);
    return projection;
}

QMatrix4x4 lookingAt(const QVector3D& target, float distance)
{
    QMatrix4x4 view;
    view.lookAt(target + QVector3D(0.0f, 0.0f, distance), target, QVector3D(0.0f, 1.0f, 0.0f));
    return view;
}

void setLiveCamera(cwCamera* camera, float distance)
{
    cwProjection projection;
    projection.setPerspective(kFieldOfView, 1.0, kNearPlane, kFarPlane);
    camera->setProjection(projection);
    camera->setViewMatrix(lookingAt(kItemBCenter, distance));
}

cwTextureStreamingStats::Counts streamingCounts()
{
    return cwTextureStreamingStats::instance()->counts();
}

} // namespace

TEST_CASE("a streamed swap after an offscreen target rebuild keeps the live frame's bindings valid",
          "[TexturedItemsStaleBindings]")
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        SKIP("needs a GPU-backed window; the offscreen QPA has no QRhi");
    }

    QTemporaryDir dataRoot;
    REQUIRE(dataRoot.isValid());
    const QDir dataRootDir(dataRoot.path());

    const cwStreamedTexture textureA = encodeStreamedTexture(dataRootDir, QStringLiteral("item-a"), 0);
    const cwStreamedTexture textureB = encodeStreamedTexture(dataRootDir, QStringLiteral("item-b"), 64);

    cwScene scene;

    QQuickWindow window;
    window.resize(kWindowSize, kWindowSize);

    auto* viewer = new cwRhiViewer();
    viewer->setParentItem(window.contentItem());
    viewer->setSize(QSizeF(kWindowSize, kWindowSize));
    viewer->setScene(&scene);

    std::atomic<int> framesSwapped {0};
    QObject::connect(&window, &QQuickWindow::frameSwapped, &window,
                     [&framesSwapped]() { framesSwapped++; }, Qt::DirectConnection);

    window.show();
    REQUIRE(waitFor([&]() { return window.isExposed() && framesSwapped > 0; }));

    if (window.rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
        SKIP("needs a QRhi-backed scene graph");
    }

    setLiveCamera(viewer->camera(), kLiveFarDistance);

    auto* render = new cwRenderTexturedItems();
    render->setScene(&scene);

    cwRenderMaterialState material;
    material.cullMode = cwRenderMaterialState::CullMode::None;

    cwRenderTexturedItems::Item itemA;
    itemA.geometry = quadAt(kItemACenter);
    itemA.streamedTexture = textureA;
    itemA.material = material;
    render->addItem(itemA);

    cwRenderTexturedItems::Item itemB;
    itemB.geometry = quadAt(kItemBCenter);
    itemB.streamedTexture = textureB;
    itemB.material = material;
    render->addItem(itemB);

    const auto framesFrom = [&](int count) {
        const int target = framesSwapped + count;
        return waitFor([&]() {
            viewer->update();
            return framesSwapped >= target;
        });
    };

    // B's pinned base lands while nothing evicts, so its first swap is harmless:
    // the SRB it replaces only ever sampled the shared loading texture.
    REQUIRE(waitFor([&]() {
        viewer->update();
        const auto counts = streamingCounts();
        return counts.streamedItems == 2 && counts.loadsInFlight == 0;
    }));
    REQUIRE(framesFrom(kSettleFrames));

    // Every job looks only at A, alternating tile sizes, so each frame's offscreen
    // render rebuilds the scratch target and nulls B's pipeline record.
    cwOffscreenRenderParameters tileParameters;
    tileParameters.viewMatrix = lookingAt(kItemACenter, kTileDistance);
    tileParameters.projectionMatrix = perspective(1.0);
    tileParameters.backgroundColor = Qt::transparent;

    QList<QFuture<QImage>> jobs;
    int jobsQueued = 0;
    const auto keepTilesFlowing = [&]() {
        jobs.removeIf([](const QFuture<QImage>& job) { return job.isFinished(); });
        while (jobs.size() < kOutstandingJobs) {
            tileParameters.outputSize = (jobsQueued % 2 == 0) ? kFullTileSize : kCroppedTileSize;
            jobs.append(scene.renderOffscreen(tileParameters));
            jobsQueued++;
        }
    };

    // Let A reach its export level so tiles dispatch one per frame.
    REQUIRE(waitFor([&]() {
        keepTilesFlowing();
        return jobsQueued > 4 * kOutstandingJobs;
    }));

    // Now the live camera moves in on B. Its refinement loads off-thread and lands
    // in some live frame's streamResources, which always follows an evicting tile.
    setLiveCamera(viewer->camera(), kLiveNearDistance);

    const bool refined = waitFor([&]() {
        keepTilesFlowing();
        viewer->update();
        const auto counts = streamingCounts();
        return counts.loadsInFlight == 0 && counts.itemsBelowDesired == 0;
    });

    // The live frames that draw B after its swap are where a stale SRB binds a
    // freed QRhiTexture.
    REQUIRE(framesFrom(kSettleFrames));

    REQUIRE(waitFor([&]() {
        jobs.removeIf([](const QFuture<QImage>& job) { return job.isFinished(); });
        viewer->update();
        return jobs.isEmpty();
    }));

    CHECK(refined);
    window.hide();
}
