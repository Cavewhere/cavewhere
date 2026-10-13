//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCamera.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCaptureCenterline.h"
#include "cwCaptureLabelPlacer.h"
#include "cwProjection.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"

//Qt includes
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QVector3D>

//Std includes
#include <algorithm>

namespace {

constexpr int   TestExportDpi = 300;
constexpr qreal TestLabelMargin = 3.0;
// Injected through setLabelFont (the fixture-font seam), so the test never
// mirrors the item's private default point size. Deliberately different from
// that default: the cull expectations below only hold if the injected font is
// really the one buildLabelRequests measures with.
constexpr qreal FixtureFontPointSize = 10.0;

// The viewport in camera pixels; the ortho projection below maps world (x, y)
// onto it 1:1, so station world coordinates read directly as local paper
// coordinates (with y flipped).
const QRect TestViewport(0, 0, 100, 100);

cwSurveyNetwork makeNetwork()
{
    // Two connected stations inside the viewport, one just outside it (but
    // within any label's cull margin), and one far beyond any cull margin.
    cwSurveyNetwork network;
    network.addShot("in1", "in2");
    network.addShot("in2", "near");
    network.addShot("near", "far");
    network.setPosition("in1", QVector3D(30.0f, 50.0f, 0.0f));
    network.setPosition("in2", QVector3D(60.0f, 50.0f, 0.0f));
    network.setPosition("near", QVector3D(110.0f, 50.0f, 0.0f));
    network.setPosition("far", QVector3D(5000.0f, 50.0f, 0.0f));
    return network;
}

QHash<QString, QPointF> anchorsByName(
    const QVector<cwCaptureLabelPlacer::LabelRequest>& requests)
{
    QHash<QString, QPointF> anchors;
    for(const auto& request : requests) {
        anchors.insert(request.text, request.anchorPos);
    }
    return anchors;
}

QSet<QString> names(const QVector<cwCaptureLabelPlacer::LabelRequest>& requests)
{
    QSet<QString> result;
    for(const auto& request : requests) {
        result.insert(request.text);
    }
    return result;
}

QFont makeFixtureFont()
{
    QFont font;
    font.setPointSizeF(FixtureFontPointSize);
    return font;
}

// A centerline over makeNetwork(), seen through an ortho camera that maps the
// network's world (x, y) onto TestViewport 1:1.
struct CenterlineFixture
{
    CenterlineFixture()
    {
        camera.setViewport(TestViewport);
        cwProjection projection;
        projection.setOrtho(0.0, TestViewport.width(), 0.0, TestViewport.height(),
                            -1.0, 1.0);
        camera.setProjection(projection);

        centerline.setCamera(&camera);
        centerline.setViewport(TestViewport);
        centerline.setExportDpi(TestExportDpi);
        centerline.setLabelFont(font);
        centerline.setNetworks({makeNetwork()});
    }

    cwCamera camera;
    QFont font = makeFixtureFont();
    cwCaptureCenterline centerline;
};

QVector<cwCaptureLabelPlacer::Placement> placeEvery(
    const QVector<cwCaptureLabelPlacer::LabelRequest>& requests)
{
    QVector<cwCaptureLabelPlacer::Placement> placements;
    placements.reserve(requests.size());
    for(int i = 0; i < requests.size(); i++) {
        cwCaptureLabelPlacer::Placement placement;
        placement.placed = true;
        placement.labelRect = QRectF(10.0 * (i + 1), 5.0, 8.0, 4.0);
        placements.append(placement);
    }
    return placements;
}

} // namespace

TEST_CASE("cwCaptureCenterline buildLabelRequests culls off-viewport stations before measuring",
          "[cwCaptureCenterline]")
{
    CenterlineFixture fixture;
    cwCaptureCenterline& centerline = fixture.centerline;
    const QFont& fixtureFont = fixture.font;

    const QRectF viewportBounds = centerline.boundingRect();
    REQUIRE(viewportBounds == QRectF(0.0, 0.0, 100.0, 100.0));

    // Without viewport bounds every named station gets measured — including
    // the two off-viewport ones.
    const auto allRequests = centerline.buildLabelRequests();
    REQUIRE(names(allRequests)
            == QSet<QString>({"in1", "in2", "near", "far"}));

    // The request sizes prove the injected fixture font is the one the item
    // measures with: re-measure one name directly and compare.
    QPainterPath referencePath;
    referencePath.addText(QPointF(0.0, 0.0),
                          cwCaptureLabelPlacer::scaledFont(fixtureFont, TestExportDpi),
                          QStringLiteral("in1"));
    for(const auto& request : allRequests) {
        if(request.text == QStringLiteral("in1")) {
            CHECK(request.size == referencePath.boundingRect().size());
        }
    }

    // The same cull-rect computation the item performs — from the injected
    // fixture font, so the expectations below hold regardless of the
    // platform's font metrics.
    const QFontMetricsF metrics(
        cwCaptureLabelPlacer::scaledFont(fixtureFont, TestExportDpi));
    auto cullRectFor = [&](const QString& name) {
        return cwCaptureLabelPlacer::viewportCullRect(
            viewportBounds,
            cwCaptureLabelPlacer::labelSizeUpperBound(metrics, name.length()),
            TestLabelMargin);
    };

    // Sanity of the fixture: "near" sits outside the viewport but inside its
    // cull rect (a 10 pt label at 300 DPI clears >40 paper px of margin);
    // "far" is beyond any plausible label margin.
    const auto anchors = anchorsByName(allRequests);
    CHECK_FALSE(viewportBounds.contains(anchors.value("near")));
    REQUIRE(cullRectFor("near").contains(anchors.value("near")));
    REQUIRE_FALSE(cullRectFor("far").contains(anchors.value("far")));

    // With viewport bounds only "far" is dropped: the cull is conservative,
    // so a station the placer might still label (within the cull margin)
    // must survive to measurement.
    const auto culledRequests =
        centerline.buildLabelRequests({}, {viewportBounds, TestLabelMargin});
    CHECK(names(culledRequests) == QSet<QString>({"in1", "in2", "near"}));

    // The surviving requests are identical to the unculled ones — the cull
    // only removes entries, it never perturbs measurement.
    for(const auto& request : culledRequests) {
        CHECK(request.anchorPos == anchors.value(request.text));
    }

    // applyPlacements maps results through the culled request list back to
    // the right stations: each placed rect lands on its request's station and
    // the culled station stays unlabeled.
    const QVector<cwCaptureLabelPlacer::Placement> placements =
        placeEvery(culledRequests);
    centerline.applyPlacements(placements);

    const auto placedLabels = centerline.placedLabels();
    REQUIRE(placedLabels.size() == culledRequests.size());
    QHash<QString, QRectF> placedByName;
    for(const auto& label : placedLabels) {
        placedByName.insert(label.first, label.second);
    }
    CHECK_FALSE(placedByName.contains("far"));
    for(int i = 0; i < culledRequests.size(); i++) {
        CHECK(placedByName.value(culledRequests.at(i).text)
              == placements.at(i).labelRect);
    }
}

TEST_CASE("cwCaptureCenterline defaults every part to visible",
          "[cwCaptureCenterline]")
{
    const cwCaptureCenterline centerline;
    CHECK(centerline.dotsVisible());
    CHECK(centerline.legsVisible());
    CHECK(centerline.labelsVisible());
}

TEST_CASE("cwCaptureCenterline builds no label requests while labels are hidden",
          "[cwCaptureCenterline]")
{
    CenterlineFixture fixture;
    cwCaptureCenterline& centerline = fixture.centerline;

    REQUIRE(centerline.labelsVisible());
    const auto visibleRequests = centerline.buildLabelRequests();
    REQUIRE_FALSE(visibleRequests.isEmpty());
    centerline.applyPlacements(placeEvery(visibleRequests));
    REQUIRE(centerline.placedLabels().size() == visibleRequests.size());

    centerline.setLabelsVisible(false);
    CHECK_FALSE(centerline.labelsVisible());
    CHECK(centerline.buildLabelRequests().isEmpty());

    // The request index from the visible build is cleared, so the empty slice
    // matches it, and the earlier placements are dropped.
    centerline.applyPlacements({});
    CHECK(centerline.placedLabels().isEmpty());

    centerline.setLabelsVisible(true);
    CHECK(names(centerline.buildLabelRequests()) == names(visibleRequests));
}

TEST_CASE("cwCaptureCenterline keeps its geometry while parts are hidden",
          "[cwCaptureCenterline]")
{
    CenterlineFixture fixture;
    cwCaptureCenterline& centerline = fixture.centerline;

    const QVector<QPointF> positions = centerline.stationPositions();
    const QVector<QLineF> lines = centerline.lines();
    REQUIRE_FALSE(positions.isEmpty());
    REQUIRE_FALSE(lines.isEmpty());

    centerline.setDotsVisible(false);
    centerline.setLegsVisible(false);
    CHECK_FALSE(centerline.dotsVisible());
    CHECK_FALSE(centerline.legsVisible());

    // The viewport, not the item, decides what is a placement obstacle.
    CHECK(centerline.stationPositions() == positions);
    CHECK(centerline.lines() == lines);
}

TEST_CASE("cwCaptureCenterline keeps caves that share station names apart",
          "[cwCaptureCenterline]")
{
    // Two caves, both starting at "a1", laid side by side in the viewport.
    // Cave 1 spans x 10..30 and cave 2 spans x 70..90, so any leg crossing
    // the 30..70 gap joins the two caves.
    constexpr qreal CaveGapLeft = 30.0;
    constexpr qreal CaveGapRight = 70.0;
    constexpr float StationY = 50.0f;
    constexpr float Cave1A1X = 10.0f;
    constexpr float Cave2A1X = 90.0f;
    constexpr qreal PositionTolerance = 1e-3;

    auto addCave = [](cwCavingRegion& region,
                      const QList<QPair<QString, float>>& stations) {
        cwSurveyNetwork network;
        cwStationPositionLookup lookup;
        for(int i = 0; i < stations.size(); i++) {
            lookup.setPosition(stations.at(i).first,
                               QVector3D(stations.at(i).second, StationY, 0.0f));
            if(i > 0) {
                network.addShot(stations.at(i - 1).first, stations.at(i).first);
            }
        }
        auto* cave = new cwCave();
        cave->setSurveyNetwork(network);
        cave->setStationPositionLookup(lookup);
        region.addCave(cave);
    };

    cwCavingRegion region;
    addCave(region, {{"a1", Cave1A1X}, {"a2", 20.0f}, {"a3", 30.0f}});
    addCave(region, {{"a1", Cave2A1X}, {"b2", 80.0f}, {"b3", 70.0f}});

    cwCamera camera;
    camera.setViewport(TestViewport);
    cwProjection projection;
    projection.setOrtho(0.0, TestViewport.width(), 0.0, TestViewport.height(),
                        -1.0, 1.0);
    camera.setProjection(projection);

    cwCaptureCenterline centerline;
    centerline.setCamera(&camera);
    centerline.setViewport(TestViewport);
    centerline.setExportDpi(TestExportDpi);
    centerline.setLabelFont(makeFixtureFont());
    centerline.setNetworks(cwCaptureCenterline::caveNetworks(&region));

    // Every leg stays on its own side of the gap.
    const QVector<QLineF> lines = centerline.lines();
    CHECK(lines.size() == 4);
    for(const QLineF& line : lines) {
        INFO("Leg from x=" << line.x1() << " to x=" << line.x2());
        const qreal left = qMin(line.x1(), line.x2());
        const qreal right = qMax(line.x1(), line.x2());
        CHECK((right <= CaveGapLeft || left >= CaveGapRight));
    }

    // Each cave keeps its own "a1" dot and label.
    const QVector<QPointF> positions = centerline.stationPositions();
    CHECK(positions.size() == 6);
    // Every station sits on one row, so x alone identifies it.
    auto hasStationAtX = [&](qreal x) {
        return std::any_of(positions.begin(), positions.end(), [&](const QPointF& position) {
            return qAbs(position.x() - x) < PositionTolerance;
        });
    };
    CHECK(hasStationAtX(Cave1A1X));
    CHECK(hasStationAtX(Cave2A1X));

    const auto requests = centerline.buildLabelRequests();
    int a1Labels = 0;
    for(const auto& request : requests) {
        if(request.text == QStringLiteral("a1")) {
            ++a1Labels;
        }
    }
    CHECK(requests.size() == 6);
    CHECK(a1Labels == 2);
}
