// Catch includes
#include <catch2/catch_test_macros.hpp>

// Our includes
#include "cwTwoPointProjector.h"
#include "cwAbstract2PointItem.h"
#include "cwCamera.h"
#include "cwProjection.h"

// Qt includes
#include <QMatrix4x4>
#include <QPointF>
#include <QRect>
#include <QVector3D>

namespace {

class TestTwoPointItem : public cwAbstract2PointItem
{
public:
    TestTwoPointItem() : cwAbstract2PointItem(nullptr) {}

    void p1Updated() override { ++p1UpdateCount; lastP1 = p1(); }
    void p2Updated() override { ++p2UpdateCount; lastP2 = p2(); }

    int p1UpdateCount = 0;
    int p2UpdateCount = 0;
    QPointF lastP1;
    QPointF lastP2;
};

void configureCamera(cwCamera& camera)
{
    cwProjection projection;
    projection.setOrtho(-10.0, 10.0, -10.0, 10.0, -10.0, 10.0);
    camera.setProjection(projection);
    camera.setViewport(QRect(0, 0, 200, 200));
    camera.setViewMatrix(QMatrix4x4()); // Identity
}

} // namespace

TEST_CASE("cwTwoPointProjector projects world points once enabled", "[cwTwoPointProjector]")
{
    TestTwoPointItem target;
    cwCamera camera;
    configureCamera(camera);
    cwTwoPointProjector projector;

    const QVector3D firstPoint(0.0f, 0.0f, 0.0f);
    const QVector3D secondPoint(5.0f, -5.0f, 0.0f);
    QMatrix4x4 modelMatrix;
    modelMatrix.setToIdentity();

    projector.setTarget(&target);
    projector.setCamera(&camera);
    projector.setModelMatrix(modelMatrix);
    projector.setP1World(firstPoint);
    projector.setP2World(secondPoint);

    CHECK(target.p1UpdateCount == 0);
    CHECK(target.p2UpdateCount == 0);

    projector.setEnabled(true);

    const QPointF expectedP1 = camera.project(firstPoint, modelMatrix);
    const QPointF expectedP2 = camera.project(secondPoint, modelMatrix);

    CHECK(target.p1UpdateCount == 1);
    CHECK(target.p2UpdateCount == 1);
    CHECK(target.lastP1 == expectedP1);
    CHECK(target.lastP2 == expectedP2);
}

TEST_CASE("cwTwoPointProjector reacts to camera and model changes", "[cwTwoPointProjector]")
{
    TestTwoPointItem target;
    cwCamera camera;
    configureCamera(camera);
    cwTwoPointProjector projector;

    projector.setTarget(&target);
    projector.setCamera(&camera);
    projector.setEnabled(true);
    projector.setP1World(QVector3D(0.0f, 0.0f, 0.0f));
    projector.setP2World(QVector3D(0.0f, 1.0f, 0.0f));

    const int initialP1Updates = target.p1UpdateCount;
    const int initialP2Updates = target.p2UpdateCount;

    // Changing the model matrix should trigger a reprojection.
    QMatrix4x4 translatedMatrix;
    translatedMatrix.translate(2.0f, 0.0f, 0.0f);
    projector.setModelMatrix(translatedMatrix);

    CHECK(target.p1UpdateCount == initialP1Updates + 1);
    CHECK(target.p2UpdateCount == initialP2Updates + 1);

    const QPointF expectedShifted = camera.project(QVector3D(0.0f, 0.0f, 0.0f), translatedMatrix);
    CHECK(target.lastP1 == expectedShifted);

    // Changing the camera viewport should also trigger projection updates.
    camera.setViewport(QRect(0, 0, 400, 400));

    CHECK(target.p1UpdateCount == initialP1Updates + 2);
    CHECK(target.p2UpdateCount == initialP2Updates + 2);
}
