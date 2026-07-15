//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCamera.h"
#include "cwCaptureCenterline.h"
#include "cwCaptureLabelPlacer.h"
#include "cwProjection.h"
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

} // namespace

TEST_CASE("cwCaptureCenterline buildLabelRequests culls off-viewport stations before measuring",
          "[cwCaptureCenterline]")
{
    cwCamera camera;
    camera.setViewport(TestViewport);
    cwProjection projection;
    projection.setOrtho(0.0, TestViewport.width(), 0.0, TestViewport.height(),
                        -1.0, 1.0);
    camera.setProjection(projection);

    QFont fixtureFont;
    fixtureFont.setPointSizeF(FixtureFontPointSize);

    cwCaptureCenterline centerline;
    centerline.setCamera(&camera);
    centerline.setViewport(TestViewport);
    centerline.setExportDpi(TestExportDpi);
    centerline.setLabelFont(fixtureFont);
    centerline.setNetwork(makeNetwork());

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
    QVector<cwCaptureLabelPlacer::Placement> placements;
    placements.reserve(culledRequests.size());
    for(int i = 0; i < culledRequests.size(); i++) {
        cwCaptureLabelPlacer::Placement placement;
        placement.placed = true;
        placement.labelRect = QRectF(10.0 * (i + 1), 5.0, 8.0, 4.0);
        placements.append(placement);
    }
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
