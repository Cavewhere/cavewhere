/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeCases.h"
#include "cwPassageTubeMeshCheck.h"

//Qt includes
#include <QStringList>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

using Builder = cwPassageTubeBuilder;
using Cases = cwPassageTubeCases;

constexpr float kAxisTolerance = 1e-3f;
constexpr float kRadiusTolerance = 1e-3f;
constexpr double kSandboxShaftProfileDegrees = 90.0;
constexpr double kLeadInMeters = 10.0;
constexpr QVector3D kWorldUp(0.0f, 0.0f, 1.0f);

const Cases::Section kSandboxSection {1, 1, 1.5, 1};

Cases::JunctionCase sandboxCase(double profileDegrees)
{
    return Cases::JunctionCase {.name = QStringLiteral("Sandbox"),
                                .throughSection = kSandboxSection,
                                .throughAtJunction = kSandboxSection,
                                .throughPastJunction = kSandboxSection,
                                .throughBendDegrees = 0,
                                .throughProfileDegrees = 0,
                                .throughBendProfileDegrees = profileDegrees,
                                .arms = {},
                                .sampleSpacing = 1};
}

QStringList lrud(double left, double right, double up, double down)
{
    return {QString::number(left), QString::number(right), QString::number(up), QString::number(down)};
}

// A level lead-in north to s1, then a leg of length meters climbing north at
// clino degrees, every station reading the same walls.
Cases::Survey steepLegSurvey(double meters, double clinoDegrees, const QStringList& walls)
{
    const double clino = qDegreesToRadians(clinoDegrees);
    const QVector3D s1(0.0f, 0.0f, 0.0f);
    const QVector3D s2 = s1 + static_cast<float>(meters) * QVector3D(0.0f, static_cast<float>(std::cos(clino)), static_cast<float>(std::sin(clino)));
    return Cases::toSurvey(Cases::Trip {{{
        {QStringLiteral("s0"), QVector3D(0.0f, static_cast<float>(-kLeadInMeters), 0.0f), walls},
        {QStringLiteral("s1"), s1, walls},
        {QStringLiteral("s2"), s2, walls},
    }}});
}

QString vectorText(const QVector3D& v)
{
    return QStringLiteral("(%1, %2, %3)").arg(v.x()).arg(v.y()).arg(v.z());
}

struct LegFound {
    const Builder::Chain* chain = nullptr;
    int chainLeg = -1;
};

// The chain and chain leg of the leg climbing to station to.
LegFound legTo(const Builder::Debug& debug, const QString& to)
{
    for (const Builder::Chain& chain : debug.chains) {
        for (int i = 0; i < chain.legs.size(); ++i) {
            const Builder::Leg& leg = debug.legs.at(chain.legs.at(i).legIndex);
            if (leg.to == to || leg.from == to) {
                const bool isLevelLeadIn = leg.from == QStringLiteral("t0") || leg.to == QStringLiteral("t0")
                                           || leg.from == QStringLiteral("s0") || leg.to == QStringLiteral("s0");
                if (!isLevelLeadIn) {
                    return {&chain, i};
                }
            }
        }
    }
    return {};
}

QString padText(const Builder::LegPad& pad)
{
    return QStringLiteral("pad start %1, end %2, e %3, isShaft %4, rise %5, thin %6")
        .arg(pad.start)
        .arg(pad.end)
        .arg(pad.e)
        .arg(pad.isShaft)
        .arg(pad.riseRatio)
        .arg(pad.thinness);
}

int countOf(const cwPassageTubeMeshCheck::Report& report, cwPassageTubeMeshCheck::Type type)
{
    return static_cast<int>(std::count_if(report.issues.cbegin(), report.issues.cend(), [type](const cwPassageTubeMeshCheck::Issue& issue) {
        return issue.type == type;
    }));
}

} // namespace

// T07ao step 1 (I14): at profile 90 the sandbox's J→t1 leg is 10 m straight
// up in a 2 m wide, 2.5 m tall section. It is a shaft: its rings stand square
// to the leg, round at 1 m, with kDomeRings dome rings past each end along
// the axis, and the union leaves its centerline inside the tube.
TEST_CASE("cwPassageTubeBuilder draws the sandbox's plumb leg as a round domed shaft", "[PassageTube][cwPassageTubeBuilder]")
{
    const Cases::Survey survey = Cases::toSurvey(sandboxCase(kSandboxShaftProfileDegrees));
    const Builder::Debug rings = Builder::buildChainRings(survey.region, survey.positionsByCave, Builder::Options());

    const LegFound shaft = legTo(rings, QStringLiteral("t1"));
    REQUIRE(shaft.chain != nullptr);
    REQUIRE(shaft.chainLeg < shaft.chain->legPads.size());
    const Builder::LegPad& pad = shaft.chain->legPads.at(shaft.chainLeg);
    INFO(padText(pad).toStdString());
    CHECK(pad.isShaft);
    CHECK(pad.riseRatio == Catch::Approx(Cases::kThroughLegMeters / (kSandboxSection.up + kSandboxSection.down)));
    CHECK(std::isinf(pad.thinness));
    CHECK(pad.e == Catch::Approx(0.0));

    const QVector3D bottom(0.0f, 0.0f, 0.0f);
    const QVector3D top = bottom + kWorldUp * static_cast<float>(Cases::kThroughLegMeters);
    QVector<const Builder::Ring*> shaftRings;
    for (const Builder::Ring& ring : shaft.chain->rings) {
        if (ring.chainLeg == shaft.chainLeg) {
            shaftRings.append(&ring);
        }
    }
    REQUIRE_FALSE(shaftRings.isEmpty());

    int offAxis = 0;
    int tilted = 0;
    int pastTop = 0;
    int pastBottom = 0;
    float lowest = std::numeric_limits<float>::max();
    float highest = std::numeric_limits<float>::lowest();
    for (const Builder::Ring* ring : std::as_const(shaftRings)) {
        const QVector3D onAxis(0.0f, 0.0f, ring->center.z());
        offAxis += ring->center.distanceToPoint(onAxis) > kAxisTolerance ? 1 : 0;
        const bool isAlongAxis = ring->frame.tangent.distanceToPoint(kWorldUp) <= kAxisTolerance
                                 || ring->frame.tangent.distanceToPoint(-kWorldUp) <= kAxisTolerance;
        tilted += isAlongAxis ? 0 : 1;
        pastTop += ring->center.z() > top.z() + kAxisTolerance ? 1 : 0;
        pastBottom += ring->center.z() < bottom.z() - kAxisTolerance ? 1 : 0;
        lowest = std::min(lowest, ring->center.z());
        highest = std::max(highest, ring->center.z());
    }
    INFO(shaftRings.size() << " shaft rings, centers z " << lowest << " to " << highest << "; " << offAxis << " off the axis, "
                           << tilted << " with a tangent off ±up, " << pastTop << " past the top station, " << pastBottom
                           << " past the bottom station; first ring center " << vectorText(shaftRings.first()->center).toStdString()
                           << " tangent " << vectorText(shaftRings.first()->frame.tangent).toStdString());
    CHECK(offAxis == 0);
    CHECK(tilted == 0);
    CHECK(pastTop == Builder::kDomeRings);
    CHECK(pastBottom == Builder::kDomeRings);

    // The end sections at the stations are round at mean(L, R) = 1 m.
    for (const QVector3D& station : {bottom, top}) {
        const auto it = std::find_if(shaftRings.cbegin(), shaftRings.cend(), [&](const Builder::Ring* ring) {
            return ring->center.distanceToPoint(station) <= kAxisTolerance;
        });
        INFO("end ring at station " << vectorText(station).toStdString());
        REQUIRE(it != shaftRings.cend());
        for (const QVector3D& vertex : (*it)->vertices) {
            CHECK(vertex.distanceToPoint((*it)->center) == Catch::Approx(kSandboxSection.left).margin(kRadiusTolerance));
        }
    }

    const Builder::Debug debug = Builder::buildDebug(survey.region, survey.positionsByCave, {}, Builder::Options());
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(countOf(report, cwPassageTubeMeshCheck::CenterlineOutside) == 0);
}

// T07ao: a Z-shaped shaft, entered heading north and left heading south, has
// stations facing opposite ways in plan. Its end rings share one right, so the
// rings lerped between them stay round instead of pinching to the axis.
TEST_CASE("cwPassageTubeBuilder keeps a shaft round between stations facing opposite ways", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kShaftMeters = 10.0;
    const QStringList walls = lrud(kSandboxSection.left, kSandboxSection.right, kSandboxSection.up, kSandboxSection.down);
    const QVector3D top = kWorldUp * static_cast<float>(kShaftMeters);
    const Cases::Survey survey = Cases::toSurvey(Cases::Trip {{{
        {QStringLiteral("s0"), QVector3D(0.0f, static_cast<float>(-kLeadInMeters), 0.0f), walls},
        {QStringLiteral("b"), QVector3D(0.0f, 0.0f, 0.0f), walls},
        {QStringLiteral("c"), top, walls},
        {QStringLiteral("d"), top + QVector3D(0.0f, static_cast<float>(-kLeadInMeters), 0.0f), walls},
    }}});
    const Builder::Debug rings = Builder::buildChainRings(survey.region, survey.positionsByCave, Builder::Options());

    const LegFound shaft = legTo(rings, QStringLiteral("b"));
    REQUIRE(shaft.chain != nullptr);
    REQUIRE(shaft.chainLeg < shaft.chain->legPads.size());
    REQUIRE(shaft.chain->legPads.at(shaft.chainLeg).isShaft);

    const double expectedRadius = (kSandboxSection.left + kSandboxSection.right) / 2.0;
    int betweenStations = 0;
    for (const Builder::Ring& ring : shaft.chain->rings) {
        const bool isBetweenStations = ring.chainLeg == shaft.chainLeg && ring.center.z() >= -kAxisTolerance
                                       && ring.center.z() <= top.z() + kAxisTolerance;
        if (isBetweenStations) {
            ++betweenStations;
            float smallest = std::numeric_limits<float>::max();
            for (const QVector3D& vertex : ring.vertices) {
                smallest = std::min(smallest, vertex.distanceToPoint(ring.center));
            }
            INFO("ring at " << vectorText(ring.center).toStdString());
            CHECK(smallest == Catch::Approx(expectedRadius).margin(kRadiusTolerance));
        }
    }
    CHECK(betweenStations > 2);
}

// T07ao: a leg is a shaft when it rises more than its section is tall and its
// vertical-ring slab is thinner along the shot than wide across it.
TEST_CASE("cwPassageTubeBuilder classifies a steep leg as a shaft from its readings", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kSteepDegrees = 80.0;
    constexpr double kShortMeters = 2.0;
    constexpr double kLongMeters = 10.0;
    const QStringList sandboxWalls = lrud(kSandboxSection.left, kSandboxSection.right, kSandboxSection.up, kSandboxSection.down);
    constexpr double kFissureHalfWidth = 0.5;
    constexpr double kFissureHalfHeight = 3.0;
    const QStringList fissureWalls = lrud(kFissureHalfWidth, kFissureHalfWidth, kFissureHalfHeight, kFissureHalfHeight);

    const auto padOf = [](const Cases::Survey& survey, const Builder::Options& options) {
        const Builder::Debug debug = Builder::buildChainRings(survey.region, survey.positionsByCave, options);
        const LegFound found = legTo(debug, QStringLiteral("s2"));
        REQUIRE(found.chain != nullptr);
        REQUIRE(found.chainLeg < found.chain->legPads.size());
        return found.chain->legPads.at(found.chainLeg);
    };

    SECTION("a 2 m leg at 80° in the sandbox section rises less than its section")
    {
        const Builder::LegPad pad = padOf(steepLegSurvey(kShortMeters, kSteepDegrees, sandboxWalls), Builder::Options());
        INFO(padText(pad).toStdString());
        CHECK(pad.riseRatio < 1.0);
        CHECK_FALSE(pad.isShaft);
    }

    SECTION("a 10 m leg at 80° in a tall narrow fissure is thicker along the shot than wide")
    {
        const Builder::LegPad pad = padOf(steepLegSurvey(kLongMeters, kSteepDegrees, fissureWalls), Builder::Options());
        INFO(padText(pad).toStdString());
        CHECK(pad.riseRatio > 1.0);
        CHECK(pad.thinness < 1.0);
        CHECK_FALSE(pad.isShaft);
    }

    SECTION("the same fissure leg with no thinness cutoff is a shaft")
    {
        Builder::Options options;
        options.shaftThinnessRatio = 0.0;
        const Builder::LegPad pad = padOf(steepLegSurvey(kLongMeters, kSteepDegrees, fissureWalls), options);
        INFO(padText(pad).toStdString());
        CHECK(pad.isShaft);
    }

    SECTION("a level 2 m leg between 5 m walls stands on its stations (I01)")
    {
        constexpr double kWideWall = 5.0;
        const Cases::Survey survey = steepLegSurvey(kShortMeters, 0.0, lrud(kWideWall, kWideWall, 1, 1));
        const Builder::Debug debug = Builder::buildChainRings(survey.region, survey.positionsByCave, Builder::Options());
        INFO(debug.chains.size() << " chains");
        REQUIRE(debug.chains.size() == 1);
        for (const Builder::LegPad& pad : debug.chains.first().legPads) {
            INFO(padText(pad).toStdString());
            CHECK(pad.e == Catch::Approx(0.0));
            CHECK_FALSE(pad.isShaft);
        }
    }
}

// T07ao step 4: with shaft rings off and padScale 0.5, a plumb leg pads
// mean(L, R) / 2 off its stations along its bearing, as before the shaft rule.
TEST_CASE("cwPassageTubeBuilder pads a plumb leg as before with shaft rings off", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kPitRadius = 3.0;
    constexpr double kDropMeters = 12.0;
    constexpr double kLipMeters = 1.0;
    constexpr double kHeadPadScale = 0.5;
    Cases::Survey survey = Cases::toSurvey(Cases::Trip {{{
        {QStringLiteral("top"), QVector3D(0, 0, 0), lrud(kPitRadius, kPitRadius, kLipMeters, kDropMeters)},
        {QStringLiteral("bottom"), QVector3D(0, 0, -kDropMeters), lrud(kPitRadius, kPitRadius, kDropMeters, kLipMeters)},
    }}});
    cwShot& shot = survey.region.caves.first().trips.first().chunks.first().shots.first();
    shot.setCompass(cwCompassReading(90.0));

    Builder::Options options;
    options.shaftRingsSquare = false;
    options.padScale = kHeadPadScale;
    const Builder::Debug debug = Builder::buildDebug(survey.region, survey.positionsByCave, {}, options);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    const float pad = static_cast<float>(kPitRadius / 2.0);
    INFO("first ring " << vectorText(chain.rings.first().center).toStdString() << ", last ring "
                       << vectorText(chain.rings.last().center).toStdString());
    CHECK(chain.rings.first().center.distanceToPoint(QVector3D(-pad, 0, 0)) < kAxisTolerance);
    CHECK(chain.rings.last().center.distanceToPoint(QVector3D(pad, 0, -kDropMeters)) < kAxisTolerance);
    for (const Builder::LegPad& legPad : chain.legPads) {
        CHECK_FALSE(legPad.isShaft);
    }
}
