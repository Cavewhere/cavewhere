#include "TestHelper.h"
#include <catch2/catch_test_macros.hpp>
#include <QQuickItem>
#include "cwTransformItemUpdater.h"

TEST_CASE("cwTransformItemUpdater basic functionality", "[cwTransformItemUpdater]") {
    cwTransformItemUpdater updater;

    SECTION("Target item is null by default") {
        CHECK(updater.targetItem() == nullptr);
    }

    SECTION("Set and retrieve target item") {
        QQuickItem mockItem;
        updater.setTargetItem(&mockItem);

        CHECK(updater.targetItem() == &mockItem);
    }

    SECTION("Matrix updates with target item transformations") {
        QQuickItem mockItem;
        updater.setTargetItem(&mockItem);

        mockItem.setX(10);
        mockItem.setY(20);
        mockItem.setScale(2);
        mockItem.setRotation(45);

        QMatrix4x4 expectedMatrix;
        expectedMatrix.translate(10, 20);
        expectedMatrix.rotate(45, 0, 0, 1);
        expectedMatrix.scale(2);

        CHECK(updater.matrix() == expectedMatrix);
    }

    SECTION("Add and update child items") {
        QQuickItem targetItem;
        updater.setTargetItem(&targetItem);

        cwBasePositioner childItem;
        childItem.setProperty("position3D", QVector3D(1, 1, 0));
        // CHECK(childItem.position3D() == QVector3D(1, 1, 0));

        updater.addChildItem(&childItem);

        targetItem.setX(5);
        targetItem.setY(5);

        QPointF expectedPosition(6, 6);

        CHECK(childItem.position() == expectedPosition);
    }

    SECTION("Remove child items") {
        QQuickItem targetItem;
        updater.setTargetItem(&targetItem);

        cwBasePositioner childItem;
        childItem.setProperty("position3D", QVector3D(1, 1, 0));

        updater.addChildItem(&childItem);
        updater.removeChildItem(&childItem);

        targetItem.setX(10);
        targetItem.setY(10);

        // Ensure child item is no longer updated
        CHECK(childItem.position() == QPointF(1, 1)); //It's 1, 1 because this position is updated
    }

    SECTION("Enabled property controls updates") {
        QQuickItem targetItem;
        updater.setTargetItem(&targetItem);

        cwBasePositioner childItem;
        childItem.setProperty("position3D", QVector3D(1, 1, 0));
        updater.addChildItem(&childItem);

        updater.setEnabled(false);

        targetItem.setX(10);
        targetItem.setY(10);

        // Ensure child item is not updated when updater is disabled
        CHECK(childItem.position() == QPointF(1, 1));
    }

    SECTION("Child item updates position when position3D changes") {
        QQuickItem targetItem;
        updater.setTargetItem(&targetItem);

        // Set the position and scale of the targetItem
        targetItem.setX(10);
        targetItem.setY(15);
        targetItem.setScale(2);

        cwBasePositioner childItem;
        // childItem.setPosition3D(QVector3D(1, 1, 0));
        updater.addChildItem(&childItem);

        // Change child item's position3D
        // childItem.setPosition3D(QVector3D(2, 2, 0));

        // Expected position based on targetItem's matrix
        QMatrix4x4 matrix;
        matrix.translate(targetItem.x(), targetItem.y());
        matrix.scale(targetItem.scale());
        QPointF expectedPosition = matrix.map(QVector3D(2, 2, 0)).toPointF();

        // Verify child item's position is updated
        CHECK(childItem.position() == expectedPosition);
    }
}
