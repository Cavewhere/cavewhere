//Our includes
#include "PainterPathModel.h"
#include "PenLineModel.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//QVector
#include <QVector>

using namespace cwSketch;

TEST_CASE("PainterPathModel add new points", "[PainterPathModel]") {

    // Create a PenLineModel instance.
    PenLineModel penLineModel;
    penLineModel.setCurrentStrokeWidth(-1.0);

    PainterPathModel painterModel;
    painterModel.setPenLineModel(&penLineModel);

    CHECK(painterModel.rowCount() == 1);

    //Get the active painter path and make sure it's empty
    auto painterPath = painterModel.index(0).data(PainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(painterPath.isEmpty());

    // Create a new line and get its index.
    painterModel.setActiveLineIndex(penLineModel.rowCount());
    int lineIndex = penLineModel.addNewLine();
    CHECK(lineIndex == painterModel.activeLineIndex());

    // Add points to the newly created line.
    penLineModel.addPoint(lineIndex, penLineModel.penPoint(QPointF(0.0, 0.0), 1.0));
    penLineModel.addPoint(lineIndex, penLineModel.penPoint(QPointF(10.0, 10.0), 0.5));
    penLineModel.addPoint(lineIndex, penLineModel.penPoint(QPointF(20.0, 20.0), 0.2));

    // Verify that there is one line in the model.
    REQUIRE(penLineModel.rowCount() == 1);
    CHECK(penLineModel.rowCount(penLineModel.index(0)) == 3);

    //The active path should be the first
    REQUIRE(painterModel.rowCount() == 1);

    auto activePainterPath = painterModel.index(0).data(PainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(!activePainterPath.isEmpty());

    //Add another line
    painterModel.setActiveLineIndex(penLineModel.rowCount());
    lineIndex = penLineModel.addNewLine();
    CHECK(lineIndex == painterModel.activeLineIndex());

    // Add points to the newly created line.
    penLineModel.addPoint(lineIndex, penLineModel.penPoint(QPointF(1.0, 0.0), 1.0));
    penLineModel.addPoint(lineIndex, penLineModel.penPoint(QPointF(1.0, 12.0), .51));
    penLineModel.addPoint(lineIndex, penLineModel.penPoint(QPointF(2.0, 23.0), .21));

    REQUIRE(painterModel.rowCount() == 2);

    auto finishedPainterPath = painterModel.index(1).data(PainterPathModel::PainterPathRole).value<QPainterPath>();

    //Make sure the old activePainterPath equals the finishedPainterPath
    REQUIRE(activePainterPath.elementCount() == activePainterPath.elementCount());
    for(int i = 0; i < activePainterPath.elementCount(); ++i) {
        auto oldActiveElement = activePainterPath.elementAt(i);
        auto finishedElement = finishedPainterPath.elementAt(i);
        INFO("i:" << i);
        CHECK(oldActiveElement.x == finishedElement.x);
        CHECK(oldActiveElement.y == finishedElement.y);
        CHECK(oldActiveElement.type == finishedElement.type);
    }
    CHECK(activePainterPath == finishedPainterPath);

}
