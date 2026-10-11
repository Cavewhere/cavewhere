/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

//Our includes
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeCases.h"
#include "cwPassageTubeMeshCheck.h"

//Qt includes
#include <QHash>
#include <QSet>
#include <QtMath>

//Std includes
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace {

using Cases = cwPassageTubeCases;

constexpr int kSmokeStride = 25;
constexpr int kRandomSeedCount = 50;
constexpr int kMaxRandomJunctions = 6;

const QVector<Cases::Case>& catalog()
{
    static const QVector<Cases::Case> cases = Cases::catalog();
    return cases;
}

const Cases::Case& catalogCase(const std::string& name)
{
    static const QHash<QString, int> indexByName = []() {
        QHash<QString, int> indices;
        for (int i = 0; i < catalog().size(); ++i) {
            indices.insert(catalog().at(i).name, i);
        }
        return indices;
    }();
    const int index = indexByName.value(QString::fromStdString(name), -1);
    REQUIRE(index >= 0);
    return catalog().at(index);
}

std::vector<std::string> caseNames(const std::function<bool(const Cases::Case&)>& keep, int stride = 1)
{
    std::vector<std::string> names;
    int matched = 0;
    for (const Cases::Case& entry : catalog()) {
        if (keep(entry)) {
            if (matched % stride == 0) {
                names.push_back(entry.name.toStdString());
            }
            ++matched;
        }
    }
    return names;
}

bool isSweep(const Cases::Case& entry)
{
    return entry.tags.contains(QStringLiteral("sweep"));
}

bool hasJunction(const Cases::Case& entry)
{
    return !entry.tags.contains(QStringLiteral("arms:0"));
}

cwPassageTubeBuilder::Options builderOptions(double sampleSpacing)
{
    cwPassageTubeBuilder::Options options;
    options.sampleSpacingMeters = sampleSpacing;
    return options;
}

void checkBuildsClean(const Cases::Trip& trip, double sampleSpacing = cwPassageTubeBuilder::Options().sampleSpacingMeters)
{
    const Cases::Survey survey = Cases::toSurvey(trip);
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(sampleSpacing));
    REQUIRE_FALSE(debug.mesh.isEmpty());
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
}

// Each trip's tube is watertight, consistently wound and outward facing.
void checkBuildsClosed(const Cases::Trip& trip, double sampleSpacing = cwPassageTubeBuilder::Options().sampleSpacingMeters)
{
    const Cases::Survey survey = Cases::toSurvey(trip);
    const cwPassageTubeMesh mesh = cwPassageTubeBuilder::build(survey.region, survey.positionsByCave, {}, builderOptions(sampleSpacing));
    REQUIRE_FALSE(mesh.isEmpty());
    for (const cwPassageTubeMesh::VertexRange& range : mesh.tripIndexRanges) {
        const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::closed(mesh, range);
        INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
        CHECK(report.issues.isEmpty());
    }
}

void checkCaseBuildsClean(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    checkBuildsClean(entry.trip, entry.sampleSpacing);
}

void checkCaseBuildsClosed(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    checkBuildsClosed(entry.trip, entry.sampleSpacing);
}

// Junction-free cases the builder gets wrong today, none of them owned by
// a plan task yet:
// - a sloped through passage: the ring tilts with the slope while its up
//   and down walls stay vertical, so WallMissed fires on them;
// - a 90 degree bend: the inner wall crosses itself, as in the builder's
//   "keeps the inner wall of a sharp bend clean";
// - the straight passage doubling its section past J: SelfIntersection
//   between neighboring strips as the tube widens;
// - the loop: the closing station's walls follow a facing from one
//   neighbor while its ring follows both, so WallMissed fires there.
bool hasKnownBuilderDefect(const Cases::Case& entry)
{
    const QStringList& tags = entry.tags;
    const bool sloped = tags.contains(QStringLiteral("through-profile:30")) || tags.contains(QStringLiteral("through-profile:-30"));
    const bool sharpBend = tags.contains(QStringLiteral("bend:90"));
    const bool widensStraight = tags.contains(QStringLiteral("bend:0")) && tags.contains(QStringLiteral("section:x2"));
    return sloped || sharpBend || widensStraight || tags.contains(QStringLiteral("loop"));
}

// Station names of degree above two: survey junctions.
int junctionCount(const Cases::Trip& trip)
{
    QHash<QString, QSet<QString>> neighbors;
    for (const QList<Cases::Station>& chunk : trip.chunks) {
        for (int i = 1; i < chunk.size(); ++i) {
            const QString& from = chunk.at(i - 1).name;
            const QString& to = chunk.at(i).name;
            if (from.isEmpty() || to.isEmpty()) {
                continue;
            }
            neighbors[from].insert(to);
            neighbors[to].insert(from);
        }
    }
    int count = 0;
    for (const QSet<QString>& adjacent : std::as_const(neighbors)) {
        if (adjacent.size() > 2) {
            ++count;
        }
    }
    return count;
}

QVector3D position(const Cases::Trip& trip, const QString& name)
{
    for (const QList<Cases::Station>& chunk : trip.chunks) {
        for (const Cases::Station& station : chunk) {
            if (station.name == name && station.position) {
                return *station.position;
            }
        }
    }
    FAIL("no station " << name.toStdString());
    return {};
}

} // namespace

// Every case with a junction fails until T07d merges the dome into the
// through tube: SelfIntersection and NestedShell fire where they overlap.
TEST_CASE("cwPassageTubeCases catalog builds clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return !isSweep(entry) && hasJunction(entry);
    })));
    checkCaseBuildsClean(name);
}

// A smoke subsample of the sweep; fails until T07d like the catalog above.
TEST_CASE("cwPassageTubeCases sweep subsample builds clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return isSweep(entry) && hasJunction(entry);
    }, kSmokeStride)));
    checkCaseBuildsClean(name);
}

// The full sweep, run with "[Sweep]"; fails until T07d like the catalog
// above. It lacks the [PassageTube] tag because a tag filter also selects
// hidden tests, and the sweep would add minutes to every "[PassageTube]" run.
TEST_CASE("cwPassageTubeCases full sweep builds clean meshes", "[cwPassageTubeCases][Sweep][.][!mayfail]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return isSweep(entry) && hasJunction(entry);
    })));
    checkCaseBuildsClean(name);
}

TEST_CASE("cwPassageTubeCases catalog cases without a junction build clean meshes", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return !hasJunction(entry) && !hasKnownBuilderDefect(entry);
    })));
    checkCaseBuildsClean(name);
}

TEST_CASE("cwPassageTubeCases catalog cases without a junction build closed tubes", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return !hasJunction(entry);
    })));
    checkCaseBuildsClosed(name);
}

// See hasKnownBuilderDefect for what each case reports.
TEST_CASE("cwPassageTubeCases catalog cases without a junction and with known builder defects build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return !hasJunction(entry) && hasKnownBuilderDefect(entry);
    })));
    checkCaseBuildsClean(name);
}

// Fails until T07d like the catalog above.
TEST_CASE("cwPassageTubeCases random surveys build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const int seed = GENERATE(range(1, kRandomSeedCount + 1));
    const int junctions = 1 + (seed - 1) % kMaxRandomJunctions;
    INFO("seed " << seed << ", " << junctions << " junctions");
    checkBuildsClean(Cases::random(static_cast<unsigned int>(seed), junctions));
}

// Random bearings bend the passage sharply at most stations, so the inner
// walls cross as in the builder's "keeps the inner wall of a sharp bend
// clean"; the tube is still watertight.
TEST_CASE("cwPassageTubeCases random surveys without a junction build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const int seed = GENERATE(range(1, kRandomSeedCount + 1));
    INFO("seed " << seed);
    checkBuildsClean(Cases::random(static_cast<unsigned int>(seed), 0));
}

TEST_CASE("cwPassageTubeCases random surveys without a junction build closed tubes", "[PassageTube][cwPassageTubeCases]")
{
    const int seed = GENERATE(range(1, kRandomSeedCount + 1));
    INFO("seed " << seed);
    checkBuildsClosed(Cases::random(static_cast<unsigned int>(seed), 0));
}

TEST_CASE("cwPassageTubeCases a case survey round-trips its parameters", "[PassageTube][cwPassageTubeCases]")
{
    SECTION("an arm's far station lies at its bearing and inclination")
    {
        constexpr double kPlanDegrees = 60.0;
        constexpr double kProfileDegrees = 30.0;
        constexpr double kLengthMeters = 8.0;
        // Plan angles count from the bearing back toward t0, due south.
        constexpr double kExpectedBearing = 120.0;

        Cases::JunctionCase junctionCase;
        Cases::ArmSpec arm;
        arm.planDegrees = kPlanDegrees;
        arm.profileDegrees = kProfileDegrees;
        arm.lengthMeters = kLengthMeters;
        junctionCase.arms.append(arm);

        const Cases::Trip trip = Cases::toTrip(junctionCase);
        const QVector3D offset = position(trip, QStringLiteral("a00")) - position(trip, QStringLiteral("J"));
        const double bearing = qRadiansToDegrees(std::atan2(offset.x(), offset.y()));
        const double inclination = qRadiansToDegrees(std::asin(offset.z() / offset.length()));
        CHECK(offset.length() == Catch::Approx(kLengthMeters).margin(1e-4));
        CHECK(bearing == Catch::Approx(kExpectedBearing).margin(1e-3));
        CHECK(inclination == Catch::Approx(kProfileDegrees).margin(1e-3));

        const Cases::Survey survey = Cases::toSurvey(junctionCase);
        REQUIRE(survey.region.caves.size() == 1);
        REQUIRE(survey.region.caves.at(0).trips.size() == 1);
        CHECK(survey.region.caves.at(0).trips.at(0).chunks.size() == 2);
    }

    SECTION("random surveys are deterministic and have the requested junctions")
    {
        const unsigned int seed = GENERATE(1u, 7u, 42u);
        const int junctions = GENERATE(0, 1, 3, 6);
        INFO("seed " << seed << ", " << junctions << " junctions");
        const Cases::Trip trip = Cases::random(seed, junctions);
        CHECK(trip == Cases::random(seed, junctions));
        CHECK(junctionCount(trip) == junctions);
    }

    SECTION("catalog names are unique and the sweep covers every parameter")
    {
        QSet<QString> names;
        QSet<QString> tags;
        for (const Cases::Case& entry : catalog()) {
            names.insert(entry.name);
            for (const QString& tag : entry.tags) {
                tags.insert(tag);
            }
        }
        CHECK(names.size() == catalog().size());
        for (const char* tag : {"gallery", "phake", "loop", "plan:20", "plan:90", "profile:90", "profile:-90",
                                "ratio:0.3", "ratio:2.5", "arms:0", "arms:1", "arms:2", "arms:3",
                                "bend:40", "bend:90", "section:x2"}) {
            INFO(tag);
            CHECK(tags.contains(QString::fromLatin1(tag)));
        }
        CHECK(Cases::galleryCases().size() == 17);
    }
}
