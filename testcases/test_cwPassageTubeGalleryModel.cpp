/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwPassageTubeCases.h"
#include "cwPassageTubeGalleryModel.h"
#include "cwRenderLinePlot.h"
#include "cwRenderPassageTube.h"
#include "cwScene.h"

//Qt includes
#include <QSignalSpy>

//Std includes
#include <algorithm>
#include <cmath>

namespace {

using Model = cwPassageTubeGalleryModel;

const QString kSymmetricT = QStringLiteral("1. symmetric T");
const QString kJunctionFreeCase = QStringLiteral("no arms, through profile 0, bend 0, same section");
const QString kShaftFootCase = QStringLiteral("25. Zero floor reading at the foot of a shaft");
constexpr float kLengthTolerance = 1e-4f;

QString nameAt(const Model& model, int row)
{
    return model.data(model.index(row), Model::NameRole).toString();
}

// The rings of every chain of the named catalog case, built as the model
// builds it.
QVector<cwPassageTubeBuilder::Ring> caseRings(const QString& name)
{
    const QVector<cwPassageTubeCases::Case> catalog = cwPassageTubeCases::catalog();
    const auto entry = std::find_if(catalog.cbegin(), catalog.cend(), [&](const cwPassageTubeCases::Case& candidate) {
        return candidate.name == name;
    });
    if (entry == catalog.cend()) {
        return {};
    }
    const cwPassageTubeCases::Survey survey = cwPassageTubeCases::toSurvey(*entry);
    cwPassageTubeBuilder::Options options;
    options.sampleSpacingMeters = entry->sampleSpacing;
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, options);

    QVector<cwPassageTubeBuilder::Ring> rings;
    for (const cwPassageTubeBuilder::Chain& chain : debug.chains) {
        rings.append(chain.rings);
    }
    return rings;
}

} // namespace

TEST_CASE("cwPassageTubeGalleryModel lists the catalog", "[PassageTube][cwPassageTubeGalleryModel]")
{
    const QVector<cwPassageTubeCases::Case> catalog = cwPassageTubeCases::catalog();

    Model model;
    CHECK(model.currentIndex() == -1);

    SECTION("without the sweep, the catalog's hand-made cases in order")
    {
        QStringList expected;
        for (const cwPassageTubeCases::Case& entry : catalog) {
            if (!entry.tags.contains(QStringLiteral("sweep"))) {
                expected.append(entry.name);
            }
        }
        REQUIRE(model.rowCount() == expected.size());
        for (int i = 0; i < expected.size(); ++i) {
            CHECK(nameAt(model, i) == expected.at(i));
        }
    }

    SECTION("with the sweep, every catalog case")
    {
        model.setIncludeSweep(true);
        REQUIRE(model.rowCount() == catalog.size());
        CHECK(nameAt(model, model.rowCount() - 1) == catalog.constLast().name);
    }
}

TEST_CASE("cwPassageTubeGalleryModel builds the selected case", "[PassageTube][cwPassageTubeGalleryModel]")
{
    cwScene scene;
    Model model;
    model.setScene(&scene);
    REQUIRE(model.tube() != nullptr);
    REQUIRE(model.linePlot() != nullptr);

    const int row = model.indexOf(kSymmetricT);
    REQUIRE(row >= 0);
    CHECK(nameAt(model, row).startsWith(QStringLiteral("1. Symmetric T")));

    QSignalSpy currentSpy(&model, &Model::currentChanged);
    model.setCurrentIndex(row);
    CHECK(currentSpy.count() == 1);

    CHECK(model.currentName() == nameAt(model, row));
    CHECK_FALSE(model.mesh().isEmpty());
    CHECK(model.reportText().isEmpty() == model.report().issues.isEmpty());
    CHECK(model.issueTexts().size() == model.report().issues.size());
    CHECK(model.issuePositions().size() == model.report().issues.size());
    CHECK(model.data(model.index(row), Model::IssueCountRole).toInt() == model.report().issues.size());

    CHECK(model.tube()->positions() == model.mesh().positions);
    CHECK_FALSE(model.linePlot()->points().isEmpty());

    CHECK(model.junction() == QVector3D(0.0f, 0.0f, 0.0f));
    CHECK(model.boundsRadius() > 0.0);
    CHECK(qFuzzyCompare(model.armDirection().length(), 1.0f));
    CHECK(qFuzzyIsNull(model.armDirection().z()));
}

TEST_CASE("cwPassageTubeGalleryModel appends random surveys", "[PassageTube][cwPassageTubeGalleryModel]")
{
    Model model;
    const int rowsBefore = model.rowCount();
    QSignalSpy countSpy(&model, &Model::countChanged);

    model.addRandom(7, 3);

    REQUIRE(model.rowCount() == rowsBefore + 1);
    CHECK(countSpy.count() == 1);
    const QString name = nameAt(model, rowsBefore);
    CHECK(name.contains(QStringLiteral("seed 7")));

    model.setCurrentIndex(rowsBefore);
    CHECK_FALSE(model.mesh().isEmpty());

    SECTION("random rows survive a catalog reload")
    {
        model.setIncludeSweep(true);
        CHECK(nameAt(model, model.rowCount() - 1) == name);
    }
}

TEST_CASE("cwPassageTubeGalleryModel counts no issues on a junction-free case", "[PassageTube][cwPassageTubeGalleryModel]")
{
    Model model;
    model.setIncludeSweep(true);

    const int row = model.indexOf(kJunctionFreeCase);
    REQUIRE(row >= 0);
    CHECK(model.data(model.index(row), Model::IssueCountRole).toInt() == 0);
    CHECK(model.data(model.index(row), Model::IssueSummaryRole).toString().isEmpty());
}

// Case 25 turns twice: level into the shaft at f1 and out of it at f2.
TEST_CASE("cwPassageTubeGalleryModel draws the builder's rings", "[PassageTube][cwPassageTubeGalleryModel]")
{
    cwScene scene;
    Model model;
    model.setScene(&scene);
    REQUIRE(model.ringPlot() != nullptr);

    const int row = model.indexOf(kShaftFootCase);
    REQUIRE(row >= 0);
    model.setCurrentIndex(row);

    const Model::RingGeometry geometry = model.ringGeometry();
    const QVector<cwPassageTubeBuilder::Ring> rings = caseRings(kShaftFootCase);
    REQUIRE_FALSE(rings.isEmpty());
    REQUIRE(geometry.loops.size() == rings.size());
    REQUIRE(geometry.rights.count == 2 * rings.size());
    REQUIRE(geometry.ups.count == 2 * rings.size());
    CHECK(model.ringPlot()->points() == geometry.points);

    int elbowRings = 0;
    for (int i = 0; i < rings.size(); ++i) {
        INFO("ring " << i);
        const cwPassageTubeBuilder::Ring& ring = rings.at(i);
        const cwLinePlotGeometry::VertexRange& loop = geometry.loops.at(i);
        const int vertexCount = static_cast<int>(ring.vertices.size());
        REQUIRE(vertexCount > 0);
        if (ring.isGrown) {
            CHECK(loop.count == 2 * ((vertexCount + 1) / 2));
        } else {
            REQUIRE(loop.count == 2 * vertexCount);
            CHECK(geometry.points.at(loop.start) == ring.vertices.constFirst());
            CHECK(geometry.points.at(loop.start + loop.count - 1) == ring.vertices.constFirst());
        }

        const double reach = Model::kFrameAxisMeters * (ring.isElbow ? Model::kElbowFrameScale : 1.0);
        const QVector3D rightStart = geometry.points.at(geometry.rights.start + 2 * i);
        const QVector3D rightEnd = geometry.points.at(geometry.rights.start + 2 * i + 1);
        const QVector3D upStart = geometry.points.at(geometry.ups.start + 2 * i);
        const QVector3D upEnd = geometry.points.at(geometry.ups.start + 2 * i + 1);
        CHECK(rightStart == ring.center);
        CHECK(upStart == ring.center);
        CHECK(std::abs(rightStart.distanceToPoint(rightEnd) - reach) < kLengthTolerance);
        CHECK(std::abs(upStart.distanceToPoint(upEnd) - reach) < kLengthTolerance);
        CHECK(QVector3D::dotProduct((upEnd - upStart).normalized(), ring.frame.up) > 1.0f - kLengthTolerance);
        if (ring.isElbow) {
            ++elbowRings;
        }
    }
    CHECK(elbowRings > 0);

    SECTION("the report lists each elbow with its roll")
    {
        const QString report = model.reportText();
        INFO(report.toStdString());
        CHECK(report.contains(QStringLiteral("elbow at f2")));
        const QStringList elbowLines = report.split(QLatin1Char('\n')).filter(QStringLiteral("elbow at "));
        REQUIRE_FALSE(elbowLines.isEmpty());
        for (const QString& line : elbowLines) {
            CHECK(line.contains(QStringLiteral("turn ")));
            CHECK(line.contains(QStringLiteral("roll ")));
        }
    }

    SECTION("a ring line names the nearest ring to a position")
    {
        const int nearest = model.nearestRing(rings.constFirst().center);
        CHECK(nearest == 0);
        CHECK(model.ringLine(nearest).startsWith(QStringLiteral("chain 0 ring 0: ")));
        CHECK(model.ringLine(static_cast<int>(rings.size())).isEmpty());
    }
}
