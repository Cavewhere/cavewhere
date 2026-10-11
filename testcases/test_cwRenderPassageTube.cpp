// Catch2 unit tests for cwRenderPassageTube — the front end and its registry
// handoff to cwRHIPassageTube (no QRhi).

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "CwRhiSceneTestAccess.h"
#include "cwGeometryItersecter.h"
#include "cwRHIPassageTube.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderPassageTube.h"
#include "cwRenderingStatsModel.h"
#include "cwRhiScene.h"
#include "cwScene.h"
#include "cwSceneVisibility.h"

//Qt includes
#include <QSignalSpy>

namespace {

using Category = cwRenderMemoryLedger::Category;
using Residency = cwRenderMemoryLedger::Residency;

constexpr int kIndicesPerTrip = 6;

// Two trips, each a two-triangle quad over the same four corners.
cwPassageTubeMesh twoTripMesh()
{
    cwPassageTubeMesh mesh;
    mesh.positions = {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 1.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 2.0f)
    };
    mesh.indices = {
        0, 1, 2, 0, 2, 3,
        0, 2, 1, 0, 3, 2
    };
    mesh.tripIndexRanges = {
        {0, kIndicesPerTrip},
        {kIndicesPerTrip, kIndicesPerTrip}
    };
    return mesh;
}

qint64 meshBytes(const cwPassageTubeMesh& mesh)
{
    return qint64(mesh.positions.size()) * qint64(sizeof(QVector3D))
           + qint64(mesh.indices.size()) * qint64(sizeof(quint32));
}

qint64 cpuLedgerBytes()
{
    return cwRenderMemoryLedger::instance()->bytes(Category::PassageTubeGeometry, Residency::Cpu);
}

// The returned pointer lives as long as the snapshot.
const QVector<quint8>* publishedMask(const cwVisibilitySnapshot& snapshot,
                                     const cwRenderPassageTube& tube)
{
    return snapshot.mask(tube.renderObjectId(), cwRenderPassageTube::kSubId);
}

} // namespace

TEST_CASE("cwRenderPassageTube starts empty", "[PassageTube][cwRenderPassageTube]")
{
    cwRenderPassageTube tube;
    CHECK(tube.positions().isEmpty());
    CHECK(tube.indices().isEmpty());
    CHECK(tube.tripIndexRanges().isEmpty());
    CHECK(tube.visibility().isEmpty());
    CHECK_FALSE(tube.wireframe());
}

TEST_CASE("cwRenderPassageTube setMesh populates data and the ledger",
          "[PassageTube][cwRenderPassageTube]")
{
    const qint64 startBytes = cpuLedgerBytes();
    const cwPassageTubeMesh mesh = twoTripMesh();

    {
        cwRenderPassageTube tube;
        tube.setMesh(mesh);

        CHECK(tube.positions() == mesh.positions);
        CHECK(tube.indices() == mesh.indices);
        REQUIRE(tube.tripIndexRanges().size() == mesh.tripIndexRanges.size());
        CHECK(tube.tripIndexRanges().at(1).first == kIndicesPerTrip);
        CHECK(tube.visibility() == QVector<quint8>(mesh.indices.size(), cwRenderPassageTube::kVisible));
        CHECK(cpuLedgerBytes() == startBytes + meshBytes(mesh));
    }

    CHECK(cpuLedgerBytes() == startBytes);
}

TEST_CASE("cwRenderPassageTube bytes show in the render stats HUD",
          "[PassageTube][cwRenderPassageTube]")
{
    const auto passageTubeRow = [](const cwRenderingStatsModel& model) {
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row);
            if (model.data(index, cwRenderingStatsModel::NameRole).toString() == QStringLiteral("Passage tubes")) {
                return index;
            }
        }
        return QModelIndex();
    };

    cwRenderingStatsModel model;
    const QModelIndex row = passageTubeRow(model);
    REQUIRE(row.isValid());
    const qint64 startBytes = model.data(row, cwRenderingStatsModel::CpuBytesRole).toLongLong();

    const cwPassageTubeMesh mesh = twoTripMesh();
    cwRenderPassageTube tube;
    tube.setMesh(mesh);
    model.refresh();

    CHECK(model.data(row, cwRenderingStatsModel::CpuBytesRole).toLongLong() == startBytes + meshBytes(mesh));
}

TEST_CASE("cwRenderPassageTube clear resets data and the ledger",
          "[PassageTube][cwRenderPassageTube]")
{
    const qint64 startBytes = cpuLedgerBytes();

    cwRenderPassageTube tube;
    tube.setWireframe(true);
    tube.setMesh(twoTripMesh());
    tube.clear();

    CHECK(tube.positions().isEmpty());
    CHECK(tube.indices().isEmpty());
    CHECK(tube.tripIndexRanges().isEmpty());
    CHECK(tube.visibility().isEmpty());
    CHECK(tube.wireframe());
    CHECK(cpuLedgerBytes() == startBytes);
}

TEST_CASE("cwRenderPassageTube setRangeVisible hides one trip range",
          "[PassageTube][cwRenderPassageTube]")
{
    cwScene scene;
    cwRenderPassageTube tube;
    tube.setScene(&scene);
    tube.setMesh(twoTripMesh());

    CHECK(publishedMask(scene.visibility()->snapshot(), tube) == nullptr);

    tube.setRangeVisible(kIndicesPerTrip, kIndicesPerTrip, false);

    QVector<quint8> expected(2 * kIndicesPerTrip, cwRenderPassageTube::kVisible);
    for (int i = kIndicesPerTrip; i < 2 * kIndicesPerTrip; ++i) {
        expected[i] = cwRenderPassageTube::kHidden;
    }
    CHECK(tube.visibility() == expected);

    const cwVisibilitySnapshot hiddenSnapshot = scene.visibility()->snapshot();
    const QVector<quint8>* mask = publishedMask(hiddenSnapshot, tube);
    REQUIRE(mask != nullptr);
    CHECK(*mask == expected);

    SECTION("out-of-range ranges leave the mask untouched")
    {
        tube.setRangeVisible(-1, 2, true);
        tube.setRangeVisible(0, 0, true);
        tube.setRangeVisible(kIndicesPerTrip + 1, kIndicesPerTrip, true);
        CHECK(tube.visibility() == expected);
    }

    SECTION("showing the range again returns to the all-visible store entry")
    {
        tube.setRangeVisible(kIndicesPerTrip, kIndicesPerTrip, true);
        CHECK(tube.visibility() == QVector<quint8>(2 * kIndicesPerTrip, cwRenderPassageTube::kVisible));
        CHECK(publishedMask(scene.visibility()->snapshot(), tube) == nullptr);
    }

    SECTION("new geometry resets visibility to all visible")
    {
        tube.setMesh(twoTripMesh());
        CHECK(tube.visibility() == QVector<quint8>(2 * kIndicesPerTrip, cwRenderPassageTube::kVisible));
        CHECK(publishedMask(scene.visibility()->snapshot(), tube) == nullptr);
    }
}

TEST_CASE("cwRenderPassageTube wireframe property notifies on change",
          "[PassageTube][cwRenderPassageTube]")
{
    cwRenderPassageTube tube;
    QSignalSpy spy(&tube, &cwRenderPassageTube::wireframeChanged);

    tube.setWireframe(true);
    CHECK(tube.wireframe());
    CHECK(spy.count() == 1);

    tube.setWireframe(true);
    CHECK(spy.count() == 1);

    tube.setWireframe(false);
    CHECK_FALSE(tube.wireframe());
    CHECK(spy.count() == 2);
}

TEST_CASE("cwRenderPassageTube opacity and clip plane notify on change",
          "[PassageTube][cwRenderPassageTube]")
{
    cwRenderPassageTube tube;
    CHECK(tube.opacity() == 1.0);
    CHECK_FALSE(tube.clipEnabled());

    QSignalSpy opacitySpy(&tube, &cwRenderPassageTube::opacityChanged);
    QSignalSpy planeSpy(&tube, &cwRenderPassageTube::clipPlaneChanged);
    QSignalSpy enabledSpy(&tube, &cwRenderPassageTube::clipEnabledChanged);

    const QVector4D plane(1.0f, 0.0f, 0.0f, -2.0f);
    tube.setOpacity(0.5);
    tube.setClipPlane(plane);
    tube.setClipEnabled(true);
    tube.setOpacity(0.5);
    tube.setClipPlane(plane);
    tube.setClipEnabled(true);

    CHECK(tube.opacity() == 0.5);
    CHECK(tube.clipPlane() == plane);
    CHECK(tube.clipEnabled());
    CHECK(opacitySpy.count() == 1);
    CHECK(planeSpy.count() == 1);
    CHECK(enabledSpy.count() == 1);
}

TEST_CASE("cwRenderPassageTube stays out of picking",
          "[PassageTube][cwRenderPassageTube]")
{
    cwScene scene;
    cwRenderPassageTube tube;
    tube.setScene(&scene);
    tube.setMesh(twoTripMesh());
    scene.geometryItersecter()->waitForFinish();

    CHECK(scene.geometryItersecter()->boundingBox().isNull());
}

TEST_CASE("cwRenderPassageTube syncs into a cwRHIPassageTube",
          "[PassageTube][cwRenderPassageTube][cwRHIPassageTube]")
{
    cwScene scene;
    cwRhiScene rhiScene;

    auto* tube = new cwRenderPassageTube();
    tube->setScene(&scene);
    tube->setParent(nullptr);
    tube->setMesh(twoTripMesh());
    const cwRenderObjectId id = tube->renderObjectId();

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

    cwRHIObject* rhiObject = CwRhiSceneTestAccess::renderObjectForId(rhiScene, id);
    REQUIRE(rhiObject != nullptr);
    auto* rhiTube = dynamic_cast<cwRHIPassageTube*>(rhiObject);
    REQUIRE(rhiTube != nullptr);

    const std::optional<QBox3D> bounds = rhiTube->worldBounds();
    REQUIRE(bounds.has_value());
    CHECK(bounds->minimum() == QVector3D(0.0f, 0.0f, 0.0f));
    CHECK(bounds->maximum() == QVector3D(1.0f, 1.0f, 2.0f));

    scene.removeItem(tube);
    delete tube;
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    CHECK(CwRhiSceneTestAccess::renderObjectForId(rhiScene, id) == nullptr);
}
