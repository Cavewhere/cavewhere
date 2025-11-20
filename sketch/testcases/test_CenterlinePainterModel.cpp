//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QSignalSpy>
#include <QFuture>

//Our includes
#include "CenterlinePainterModel.h"

//CaveWhere includes
#include "cavewhere/cavewherelib/src/cwSurvey2DGeometryArtifact.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "cwSurvey2DGeometry.h"
#include "cwAsyncFuture.h"

//Async includes
#include "asyncfuture.h"  // for AsyncFuture::Result

//Monad
#include "Monad/Monad.h"

using namespace Monad;
using namespace cwSketch;
using namespace Catch;

TEST_CASE("CenterlinePainterModel builds exactly three paths with correct strokes", "[CenterlinePainterModel]") {
    // 1) Instantiate model and geometry artifact
    cwSketch::CenterlinePainterModel centerlinePainterModel;
    cwSurvey2DGeometryArtifact geometryArtifact;
    centerlinePainterModel.setSurvey2DGeometry(&geometryArtifact);

    // 2) Prepare a minimal geometry: one shot line, two stations
    cwSurvey2DGeometry geometry;
    geometry.shotLines = { QLineF(0.0, 0.0, 1.0, 1.0) };

    Station2DGeometry stationA;
    stationA.name     = QStringLiteral("StationA");
    stationA.position = QPointF(0.0, 0.0);

    Station2DGeometry stationB;
    stationB.name     = QStringLiteral("StationB");
    stationB.position = QPointF(1.0, 1.0);

    geometry.stations = { stationA, stationB };

    auto future = AsyncFuture::observe(&centerlinePainterModel, &CenterlinePainterModel::modelReset).future();

    // Inject geometry result synchronously
    geometryArtifact.setGeometryResult(QtFuture::makeReadyValueFuture(Result(geometry)));

    REQUIRE(cwAsyncFuture::waitForFinished(future)); //, 2000));


    // 6) Verify that the model has exactly three rows
    REQUIRE(centerlinePainterModel.rowCount() == 3);

    // Helpers to retrieve data
    auto getPainterPath = [&](int rowIndex) {
        return centerlinePainterModel
            .data(centerlinePainterModel.index(rowIndex, 0),
                  cwSketch::AbstractPainterPathModel::PainterPathRole)
            .value<QPainterPath>();
    };
    auto getStrokeWidth = [&](int rowIndex) {
        return centerlinePainterModel
            .data(centerlinePainterModel.index(rowIndex, 0),
                  cwSketch::AbstractPainterPathModel::StrokeWidthRole)
            .toDouble();
    };

    // --- 7) Verify shot‑lines path (row 0)
    {
        QPainterPath shotLinesPath = getPainterPath(0);
        double shotLinesStrokeWidth = getStrokeWidth(0);

        // Expect exactly two elements: moveTo + lineTo
        CHECK(shotLinesPath.elementCount() == 2);
        CHECK(shotLinesStrokeWidth == Approx(0.1));

        QRectF shotLinesBoundingRect = shotLinesPath.boundingRect();
        CHECK(shotLinesBoundingRect.contains(QPointF(0.0, 0.0)));
        CHECK(shotLinesBoundingRect.contains(QPointF(1.0, 1.0)));
    }

    // --- 8) Verify station symbols path (row 1)
    {
        QPainterPath stationSymbolsPath = getPainterPath(1);
        double stationSymbolsStrokeWidth = getStrokeWidth(1);

        // Should have at least one ellipse element
        CHECK(stationSymbolsPath.elementCount() > 0);
        CHECK(stationSymbolsStrokeWidth == Approx(0.1));

        QRectF stationSymbolsBoundingRect = stationSymbolsPath.boundingRect();
        // Circle of radius 0.1 at (2,2) → bounding box ≈ [1.9,1.9]–[2.1,2.1]
        CHECK(stationSymbolsBoundingRect.left()   <= Approx(1.9));
        CHECK(stationSymbolsBoundingRect.right()  >= Approx(1.1));
        CHECK(stationSymbolsBoundingRect.top()    <= Approx(1.9));
        CHECK(stationSymbolsBoundingRect.bottom() >= Approx(1.1));
    }

    // --- 9) Verify station labels path (row 2)
    {
        QPainterPath stationLabelsPath = getPainterPath(2);
        double stationLabelsStrokeWidth = getStrokeWidth(2);

        // Should contain at least one text element
        CHECK(stationLabelsPath.elementCount() > 0);
        CHECK(stationLabelsStrokeWidth == Approx(0.0));

        QRectF stationLabelsBoundingRect = stationLabelsPath.boundingRect();
        // Text offset by (0.6, 0.2) → bounding box starts ≥ (2.6, 2.2)
        CHECK(stationLabelsBoundingRect.left() >= Approx(2.6));
        CHECK(stationLabelsBoundingRect.top()  >= Approx(2.2));
    }
}
