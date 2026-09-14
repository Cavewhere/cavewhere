// test_cwRenderPointCloud.cpp
// Catch2 unit tests for cwRenderPointCloud — front-end only (no RHI).

#include <catch2/catch_test_macros.hpp>

#include <QVector3D>

#include <memory>

#include "cwPointOctreeManifest.h"
#include "cwPointOctreeSource.h"
#include "cwRHIPointCloud.h"
#include "cwRenderPointCloud.h"

// Friend accessor (declared friend in cwRenderPointCloud.h) so the
// re-publish regression test can observe the private change trackers
// without putting test scaffolding on the production API.
struct CwRenderPointCloudTestAccess {
    static bool sourceChanged(const cwRenderPointCloud& c) {
        return c.m_source.isChanged();
    }
    static bool renderStateChanged(const cwRenderPointCloud& c) {
        return c.m_renderState.isChanged();
    }
    // Mimics the RHI back-end consuming a source publish — synchronize resets
    // the tracker after rebuilding its node table.
    static void clearSourceChanged(cwRenderPointCloud& c) {
        c.m_source.resetChanged();
    }
    // Mimics the RHI back-end consuming a render-state publish —
    // updateResources resets the tracker after re-writing the uniform.
    static void clearRenderStateChanged(cwRenderPointCloud& c) {
        c.m_renderState.resetChanged();
    }
};

namespace {

    constexpr double kRootSize = 128.0;

    cwPointOctreeSource makeSource(const QString& fingerprint)
    {
        auto manifest = std::make_shared<cwPointOctreeManifest>();
        manifest->rootMin = QVector3D(-1.0f, -2.0f, -3.0f);
        manifest->rootSize = kRootSize;
        manifest->pointCount = 3;
        manifest->bboxMin = QVector3D(-1.0f, -2.0f, -3.0f);
        manifest->bboxMax = QVector3D(1.0f, 2.0f, 3.0f);
        manifest->meanSpacingXY = 0.5f;
        manifest->fingerprint = fingerprint;
        manifest->nodes.append(cwPointOctreeNode());

        return cwPointOctreeSource(QStringLiteral("/tmp"),
                                   QStringLiteral("cloud.laz"),
                                   fingerprint,
                                   manifest);
    }

} // namespace

TEST_CASE("cwRenderPointCloud: starts empty", "[cwRenderPointCloud]") {
    cwRenderPointCloud cloud;
    REQUIRE(cloud.pointCount() == 0);
    REQUIRE(cloud.octree().isNull());
    REQUIRE(cloud.bboxMin() == QVector3D());
    REQUIRE(cloud.bboxMax() == QVector3D());
}

TEST_CASE("cwRenderPointCloud: setOctree publishes the manifest's numbers",
          "[cwRenderPointCloud]") {
    cwRenderPointCloud cloud;
    cloud.setOctree(makeSource(QStringLiteral("fingerprint")));

    REQUIRE(cloud.pointCount() == 3);
    REQUIRE(cloud.bboxMin() == QVector3D(-1.0f, -2.0f, -3.0f));
    REQUIRE(cloud.bboxMax() == QVector3D(1.0f, 2.0f, 3.0f));
    REQUIRE(cloud.meanSpacingXY() == 0.5f);
    REQUIRE_FALSE(cloud.octree().isNull());
}

TEST_CASE("cwRenderPointCloud: clear drops the octree but keeps the sprite knobs",
          "[cwRenderPointCloud]") {
    cwRenderPointCloud cloud;
    cloud.setWorldRadius(5.5f);
    cloud.setSpacingCoverage(1.5f);

    cloud.setOctree(makeSource(QStringLiteral("fingerprint")));
    REQUIRE(cloud.pointCount() == 3);

    cloud.clear();
    REQUIRE(cloud.pointCount() == 0);
    REQUIRE(cloud.octree().isNull());
    REQUIRE(cloud.worldRadius() == 5.5f);
    REQUIRE(cloud.spacingCoverage() == 1.5f);
}

TEST_CASE("cwRenderPointCloud: spacing coverage starts at the default and follows its setter",
          "[cwRenderPointCloud]") {
    using Access = CwRenderPointCloudTestAccess;

    cwRenderPointCloud cloud;
    REQUIRE(cloud.spacingCoverage() == cw::pointcloud::kDefaultSpacingCoverage);
    REQUIRE(cloud.worldRadius() == cw::pointcloud::kDefaultWorldRadius);
    REQUIRE_FALSE(Access::renderStateChanged(cloud));

    // Zero is a real setting: it turns the per-node floor off and leaves the
    // tuned world radius as the only sprite size.
    cloud.setSpacingCoverage(0.0f);
    CHECK(cloud.spacingCoverage() == 0.0f);
    CHECK(Access::renderStateChanged(cloud));

    Access::clearRenderStateChanged(cloud);
    cloud.setSpacingCoverage(0.0f);
    CHECK_FALSE(Access::renderStateChanged(cloud));
}

TEST_CASE("cwRenderPointCloud: createRHIObject returns cwRHIPointCloud",
          "[cwRenderPointCloud]") {
    // createRHIObject is protected on cwRenderObject; exercise it through a
    // derived helper that exposes the call.
    struct Exposer : cwRenderPointCloud {
        using cwRenderPointCloud::createRHIObject;
    };

    Exposer cloud;
    cwRHIObject* rhiObject = cloud.createRHIObject();
    REQUIRE(rhiObject != nullptr);
    REQUIRE(dynamic_cast<cwRHIPointCloud*>(rhiObject) != nullptr);
    delete rhiObject;
}

TEST_CASE("cwRenderPointCloud: a uniform-only change does not dirty the source tracker",
          "[cwRenderPointCloud]") {
    // Regression guard for the point-cloud re-stream trap.
    //
    // The RHI back-end (cwRHIPointCloud::synchronize) throws away its whole
    // node table — every resident buffer with it — when the source tracker
    // reports a change. The source and the cheap render knobs (world radius,
    // spacing coverage) are tracked separately for exactly this reason: a
    // uniform-only setter must NOT mark the source dirty, or every P+wheel
    // tick would drop the cloud's residency and re-stream it from the root.
    using Access = CwRenderPointCloudTestAccess;

    cwRenderPointCloud cloud;
    cloud.setOctree(makeSource(QStringLiteral("fingerprint")));

    // setOctree dirties the source tracker → the back-end rebuilds.
    REQUIRE(Access::sourceChanged(cloud));

    // Simulate the back-end consuming that publish.
    Access::clearSourceChanged(cloud);
    REQUIRE_FALSE(Access::sourceChanged(cloud));

    // A uniform-only change must leave the source tracker clean (no re-stream)
    // while marking render state dirty (cheap UBO update).
    cloud.setWorldRadius(cloud.worldRadius() + 1.0f);
    REQUIRE_FALSE(Access::sourceChanged(cloud));
    REQUIRE(Access::renderStateChanged(cloud));

    // Same for spacing coverage.
    cloud.setSpacingCoverage(cloud.spacingCoverage() + 1.0f);
    REQUIRE_FALSE(Access::sourceChanged(cloud));
}

TEST_CASE("cwRenderPointCloud: re-publishing the same octree is a no-op",
          "[cwRenderPointCloud]") {
    using Access = CwRenderPointCloudTestAccess;

    cwRenderPointCloud cloud;
    cloud.setOctree(makeSource(QStringLiteral("fingerprint")));
    Access::clearSourceChanged(cloud);

    // A freshly built manifest of the same octree: the same cache root, path,
    // and fingerprint, so identity holds and residency survives.
    cloud.setOctree(makeSource(QStringLiteral("fingerprint")));
    REQUIRE_FALSE(Access::sourceChanged(cloud));

    // A rebuilt LAZ has a new fingerprint, and that does have to reset.
    cloud.setOctree(makeSource(QStringLiteral("other-fingerprint")));
    REQUIRE(Access::sourceChanged(cloud));
}
