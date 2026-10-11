/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeCases.h"

//Qt includes
#include <QtMath>

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <random>
#include <utility>

namespace {

using Station = cwPassageTubeCases::Station;
using Trip = cwPassageTubeCases::Trip;
using Section = cwPassageTubeCases::Section;
using ArmSpec = cwPassageTubeCases::ArmSpec;
using JunctionCase = cwPassageTubeCases::JunctionCase;

constexpr double kFullTurnDegrees = 360.0;
constexpr double kHalfTurnDegrees = 180.0;
constexpr double kVerticalDegrees = 90.0;

// Sweep parameters.
constexpr std::array<double, 5> kSweepPlanDegrees = {90.0, 60.0, 45.0, 30.0, 20.0};
constexpr std::array<double, 7> kSweepProfileDegrees = {0.0, 30.0, 60.0, 90.0, -30.0, -60.0, -90.0};
constexpr std::array<double, 3> kSweepSizeRatios = {0.3, 1.0, 2.5};
constexpr std::array<double, 3> kSweepBendDegrees = {0.0, 40.0, 90.0};
constexpr std::array<double, 3> kJunctionFreeProfileDegrees = {0.0, 30.0, -30.0};
constexpr double kSameSideSpreadDegrees = 45.0;
constexpr double kThreeArmSpreadDegrees = 120.0;
constexpr double kSectionChangeFactor = 2.0;
// Arms closer than this in plan to another passage at J lie inside it.
constexpr double kMinPlanSeparationDegrees = 15.0;
const Section kSweepThroughSection {1.2, 1.2, 1.5, 1.0};

// The junction-free loop: an octagon of 10 m legs.
constexpr int kLoopSides = 8;
constexpr double kLoopLegMeters = 10.0;
const Section kLoopSection {1.0, 1.0, 1.5, 1.0};

// Random survey parameters.
constexpr double kRandomMinLegMeters = 3.0;
constexpr double kRandomMaxLegMeters = 15.0;
constexpr double kRandomGentleDegrees = 20.0;
constexpr double kRandomSteepFraction = 0.1;
constexpr double kRandomMinLrudMeters = 0.1;
constexpr double kRandomMaxLrudMeters = 5.0;
constexpr double kRandomNoLrudFraction = 0.05;
constexpr int kRandomMinBranchLegs = 1;
constexpr int kRandomMaxBranchLegs = 3;
constexpr int kLrudReadingCount = 4;
// The main passage has this many legs more than junctions, so it always
// offers an interior station to branch from.
constexpr int kRandomExtraMainLegs = 3;

// The "Down" clino: the shot drops straight down whatever its azimuth.
constexpr double kClinoDown = -90.0;

struct PhakeShot {
    const char* from;
    const char* to;
    double distance;
    double azimuth;
    double clino;
};

// Phake Cave, trip "2025.2 Release": meters, frontsights only, declination 0.
// In survey order, so every shot starts at an already placed station.
constexpr PhakeShot kPhakeShots[] = {
    {"a1", "a2", 14.52, 49.0, -3.0},
    {"a2", "a3", 9.13, 352.0, -17.3},
    {"a3", "a4", 11.05, 82.0, -4.5},
    {"a4", "a5", 6.97, 41.0, 11.2},
    {"a4", "a6", 16.54, 147.5, -7.5},
    {"a6", "a7", 9.81, 43.0, 26.0},
    {"a7", "a8", 10.23, 74.0, 17.4},
    {"a6", "a9", 23.44, 0.0, kClinoDown},
    {"a9", "a10", 5.85, 230.0, -8.4},
    {"a10", "a11", 10.56, 172.0, -21.7},
    {"a11", "a12", 9.17, 200.0, -5.3},
    {"a10", "a13", 18.28, 116.0, -4.1},
    {"a13", "a14", 6.99, 91.0, -1.2},
};

QStringList lrudStrings(const Section& section)
{
    return {QString::number(section.left), QString::number(section.right), QString::number(section.up), QString::number(section.down)};
}

QStringList emptyLrud()
{
    return {QString(), QString(), QString(), QString()};
}

// Unit vector along azimuth (clockwise from north) at inclination (up from level).
QVector3D direction(double azimuthDegrees, double inclinationDegrees)
{
    const double azimuth = qDegreesToRadians(azimuthDegrees);
    const double inclination = qDegreesToRadians(inclinationDegrees);
    const double horizontal = std::cos(inclination);
    return QVector3D(static_cast<float>(horizontal * std::sin(azimuth)),
                     static_cast<float>(horizontal * std::cos(azimuth)),
                     static_cast<float>(std::sin(inclination)));
}

double angleBetween(double a, double b)
{
    const double difference = std::fmod(std::abs(a - b), kFullTurnDegrees);
    return std::min(difference, kFullTurnDegrees - difference);
}

bool isVertical(double profileDegrees)
{
    return std::abs(profileDegrees) >= kVerticalDegrees;
}

// Plan angles of the passages leaving J, measured like ArmSpec::planDegrees;
// true when an arm lies inside another passage.
bool armsCrowd(const JunctionCase& junctionCase)
{
    QVector<double> occupied;
    if (!isVertical(junctionCase.throughProfileDegrees)) {
        occupied.append(0.0);
        const double exitProfile = junctionCase.throughProfileDegrees + junctionCase.throughBendProfileDegrees;
        if (!isVertical(exitProfile)) {
            occupied.append(kHalfTurnDegrees + junctionCase.throughBendDegrees);
        }
    }
    for (const ArmSpec& arm : junctionCase.arms) {
        if (isVertical(arm.profileDegrees)) {
            continue;
        }
        for (double other : std::as_const(occupied)) {
            if (angleBetween(arm.planDegrees, other) < kMinPlanSeparationDegrees) {
                return true;
            }
        }
        occupied.append(arm.planDegrees);
    }
    return false;
}

JunctionCase galleryCase(const QString& name, const Section& through, const QVector<ArmSpec>& arms)
{
    JunctionCase junctionCase;
    junctionCase.name = name;
    junctionCase.tags = {QStringLiteral("gallery")};
    junctionCase.throughSection = through;
    junctionCase.throughAtJunction = through;
    junctionCase.throughPastJunction = through;
    junctionCase.arms = arms;
    return junctionCase;
}

ArmSpec arm(double planDegrees, const Section& section, double profileDegrees = 0.0, double lengthMeters = cwPassageTubeCases::kArmLengthMeters)
{
    ArmSpec spec;
    spec.planDegrees = planDegrees;
    spec.profileDegrees = profileDegrees;
    spec.lengthMeters = lengthMeters;
    spec.at = section;
    spec.atJunction = section;
    return spec;
}

Section roundSection(double radius)
{
    return {radius, radius, radius, radius};
}

cwPassageTubeCases::Case loopCase()
{
    const double circumradius = kLoopLegMeters / (2.0 * std::sin(std::numbers::pi / kLoopSides));
    QList<Station> stations;
    for (int i = 0; i <= kLoopSides; ++i) {
        const int corner = i % kLoopSides;
        const double angle = 2.0 * std::numbers::pi * corner / kLoopSides;
        const QVector3D position(static_cast<float>(circumradius * std::cos(angle)),
                                 static_cast<float>(circumradius * std::sin(angle)),
                                 0.0f);
        stations.append({QStringLiteral("o%1").arg(corner), position, lrudStrings(kLoopSection)});
    }

    cwPassageTubeCases::Case loop;
    loop.name = QStringLiteral("octagon loop");
    loop.tags = {QStringLiteral("loop"), QStringLiteral("arms:0")};
    loop.trip.chunks = {stations};
    return loop;
}

} // namespace

Section cwPassageTubeCases::Section::scaled(double factor) const
{
    return {left * factor, right * factor, up * factor, down * factor};
}

cwPassageTubeCases::Trip cwPassageTubeCases::toTrip(const JunctionCase& junctionCase)
{
    const QString junctionName = QStringLiteral("J");
    const QVector3D junction(0.0f, 0.0f, 0.0f);
    const float throughLeg = static_cast<float>(kThroughLegMeters);
    const double exitProfile = junctionCase.throughProfileDegrees + junctionCase.throughBendProfileDegrees;

    const QVector3D t0 = junction - throughLeg * direction(0.0, junctionCase.throughProfileDegrees);
    const QVector3D t1 = junction + throughLeg * direction(-junctionCase.throughBendDegrees, exitProfile);

    Trip trip;
    trip.chunks.append({
        {QStringLiteral("t0"), t0, lrudStrings(junctionCase.throughSection)},
        {junctionName, junction, lrudStrings(junctionCase.throughAtJunction)},
        {QStringLiteral("t1"), t1, lrudStrings(junctionCase.throughPastJunction)},
    });

    for (int i = 0; i < junctionCase.arms.size(); ++i) {
        const ArmSpec& spec = junctionCase.arms.at(i);
        const QVector3D far = junction + static_cast<float>(spec.lengthMeters)
                                             * direction(kHalfTurnDegrees - spec.planDegrees, spec.profileDegrees);
        const QStringList junctionLrud = spec.hasJunctionLrud ? lrudStrings(spec.atJunction) : emptyLrud();
        trip.chunks.append({
            {QStringLiteral("a%1%2").arg(i).arg(0), far, lrudStrings(spec.at)},
            {junctionName, junction, junctionLrud},
        });
    }
    return trip;
}

cwPassageTubeCases::Case cwPassageTubeCases::toCase(const JunctionCase& junctionCase)
{
    return {junctionCase.name, junctionCase.tags, toTrip(junctionCase), junctionCase.sampleSpacing};
}

cwPassageTubeCases::Survey cwPassageTubeCases::toSurvey(const QList<Trip>& trips)
{
    cwCaveData cave;
    cave.id = QUuid::createUuid();

    cwStationPositionLookup positions;
    for (const Trip& input : trips) {
        cwTripData trip;
        trip.id = QUuid::createUuid();
        trip.calibrations.setDistanceUnit(input.distanceUnit);

        for (const QList<Station>& stations : input.chunks) {
            cwSurveyChunkData chunk;
            chunk.id = QUuid::createUuid();
            for (const Station& station : stations) {
                cwStation stationData(station.name);
                stationData.setLeft(cwDistanceReading(station.lrud.at(0)));
                stationData.setRight(cwDistanceReading(station.lrud.at(1)));
                stationData.setUp(cwDistanceReading(station.lrud.at(2)));
                stationData.setDown(cwDistanceReading(station.lrud.at(3)));
                chunk.stations.append(stationData);
                if (station.position) {
                    positions.setPosition(station.name, *station.position);
                }
            }
            for (int i = 1; i < stations.size(); ++i) {
                chunk.shots.append(cwShot());
            }
            trip.chunks.append(chunk);
        }
        cave.trips.append(trip);
    }

    Survey survey;
    survey.region.caves.append(cave);
    survey.positionsByCave.insert(cave.id, positions);
    return survey;
}

cwPassageTubeCases::Survey cwPassageTubeCases::toSurvey(const Trip& trip)
{
    return toSurvey(QList<Trip> {trip});
}

cwPassageTubeCases::Survey cwPassageTubeCases::toSurvey(const JunctionCase& junctionCase)
{
    return toSurvey(toTrip(junctionCase));
}

cwPassageTubeCases::Trip cwPassageTubeCases::phakeCave()
{
    QHash<QString, QVector3D> positions {{QStringLiteral("a1"), QVector3D(0, 0, 0)}};
    for (const PhakeShot& shot : kPhakeShots) {
        const double azimuth = qDegreesToRadians(shot.azimuth);
        const double clino = qDegreesToRadians(shot.clino);
        const double horizontal = shot.clino == kClinoDown ? 0.0 : shot.distance * std::cos(clino);
        const QVector3D delta(static_cast<float>(horizontal * std::sin(azimuth)),
                              static_cast<float>(horizontal * std::cos(azimuth)),
                              static_cast<float>(shot.distance * std::sin(clino)));
        positions.insert(QString::fromLatin1(shot.to), positions.value(QString::fromLatin1(shot.from)) + delta);
    }

    const auto station = [&](const char* name, const QStringList& dimensions = emptyLrud()) {
        const QString stationName = QString::fromLatin1(name);
        return Station {stationName, positions.value(stationName), dimensions};
    };
    const Station empty {QString(), std::nullopt};

    // a4 and a6 both carry the a6 pit's 23 m as down, a9 and a10 its 24 m as
    // up, the dead end a12 has 35 m down into an unsurveyed pit, a10 repeats
    // in chunk 3 without LRUD, and chunk 3 ends in two empty shots.
    Trip trip;
    trip.chunks = {
        {station("a1", lrudStrings({0, 5, 2, 1.3})), station("a2", lrudStrings({2.5, 0, 1.5, 2})), station("a3", lrudStrings({1.5, 3, 4.5, 1})),
         station("a4", lrudStrings({0, 2, 2, 1.7})), station("a5", lrudStrings({2.5, 2.5, 11, 2}))},
        {station("a4", lrudStrings({5, 0, 3, 23})), station("a6", lrudStrings({5, 0, 3, 23})), station("a7", lrudStrings({0.2, 3.5, 2.5, 1})),
         station("a8", lrudStrings({1, 5, 2, 1.2}))},
        {station("a6", lrudStrings({1, 5, 2, 1.2})), station("a9", lrudStrings({2.3, 2, 24, 0})), station("a10", lrudStrings({2.3, 2, 24, 0})),
         station("a11", lrudStrings({1, 0.2, 3, 0.5})), station("a12", lrudStrings({0, 2, 8, 35}))},
        {station("a10"), station("a13", lrudStrings({1.8, 0, 1.8, 1.7})), station("a14", lrudStrings({0, 2, 2, 1.9})), empty, empty},
    };
    return trip;
}

QVector<cwPassageTubeCases::JunctionCase> cwPassageTubeCases::galleryCases()
{
    const Section equal {1.0, 1.0, 1.5, 1.0};
    const Section medium {1.2, 1.2, 1.5, 1.0};
    const Section wide {1.5, 1.5, 2.0, 1.0};
    const Section flatFloor {1.5, 1.5, 2.0, 0.05};

    QVector<JunctionCase> cases;
    cases.append(galleryCase(QStringLiteral("1. Symmetric T, equal sections"), equal, {arm(90, equal)}));
    cases.append(galleryCase(QStringLiteral("2. Small arm into a big passage"), {3, 3, 2, 0.05}, {arm(90, {0.5, 0.5, 0.8, 0.5})}));
    cases.append(galleryCase(QStringLiteral("3. Big arm into a small passage"), {0.8, 0.8, 1, 0.5}, {arm(90, {2, 2, 4, 1})}));
    cases.append(galleryCase(QStringLiteral("4. Oblique Y, arm 60 degrees off the through axis"), medium, {arm(60, equal)}));
    cases.append(galleryCase(QStringLiteral("5. Grazing Y, arm 20 degrees off the through axis"), medium, {arm(20, equal, 0.0, 12.0)}));
    cases.append(galleryCase(QStringLiteral("6. Arm floor 2.5 m below the through floor"), flatFloor, {arm(90, {1, 1, 1.5, 2.5})}));
    cases.append(galleryCase(QStringLiteral("7. Arm ceiling 5 m above the through ceiling"), wide, {arm(90, {0.8, 0.8, 7, 1})}));
    cases.append(galleryCase(QStringLiteral("8. Pit dropping into a passage"), flatFloor, {arm(90, roundSection(1.5), kVerticalDegrees, 12.0)}));

    JunctionCase shaft = galleryCase(QStringLiteral("9. Passage joining a shaft"), roundSection(2.0), {arm(90, equal)});
    shaft.throughProfileDegrees = -kVerticalDegrees;
    cases.append(shaft);

    JunctionCase lip = galleryCase(QStringLiteral("10. Passage turning straight down a pit, side arm at the lip"), wide, {arm(90, equal)});
    lip.throughBendProfileDegrees = -kVerticalDegrees;
    cases.append(lip);

    cases.append(galleryCase(QStringLiteral("11. Off-center section, arm enters on the station's wall side"), {5, 0, 3, 1}, {arm(90, {2, 2, 1.5, 1})}));

    JunctionCase bend = galleryCase(QStringLiteral("12. Through passage bends 40 degrees at the junction"), wide, {arm(120, equal)});
    bend.throughBendDegrees = 40.0;
    cases.append(bend);

    cases.append(galleryCase(QStringLiteral("13. Four-way crossing"), medium, {arm(90, equal), arm(270, {1.5, 1.5, 1.2, 0.8})}));
    cases.append(galleryCase(QStringLiteral("14. Arm sloping down 30 degrees into the passage"), wide, {arm(90, equal, 30.0)}));
    // LRUD read off the gallery's 12-bin radii at right, up, left and down.
    cases.append(galleryCase(QStringLiteral("15. Splay-shaped sections on both tubes"), {2.8, 2.2, 0.9, 0.8}, {arm(90, {1.3, 1.0, 1.2, 0.6})}));

    ArmSpec narrowing = arm(90, {3, 3, 3, 1});
    narrowing.atJunction = {2, 2, 4, 1};
    cases.append(galleryCase(QStringLiteral("16. Arm with its own section at J larger than the through"), {1, 1, 1.5, 0.5}, {narrowing}));

    JunctionCase change = galleryCase(QStringLiteral("17. Arm joins where the through passage changes section"), {0.5, 0.5, 1, 0.5}, {arm(90, equal)});
    change.throughAtJunction = {1, 1, 1.5, 0.5};
    change.throughPastJunction = {3, 3, 2, 1};
    cases.append(change);

    return cases;
}

QVector<cwPassageTubeCases::JunctionCase> cwPassageTubeCases::sweepCases()
{
    enum class Layout {
        Single,
        Opposite,
        SameSide,
        Three
    };
    struct LayoutSpec {
        Layout layout;
        const char* tag;
        const char* label;
    };
    constexpr std::array<LayoutSpec, 4> kLayouts = {{
        {Layout::Single, "single", "1 arm"},
        {Layout::Opposite, "opposite", "2 arms opposite"},
        {Layout::SameSide, "same-side", "2 arms same side"},
        {Layout::Three, "three-way", "3 arms"},
    }};

    const auto planAngles = [](Layout layout, double plan) -> QVector<double> {
        switch (layout) {
        case Layout::Single:
            return {plan};
        case Layout::Opposite:
            return {plan, -plan};
        case Layout::SameSide:
            return {plan, plan + kSameSideSpreadDegrees};
        case Layout::Three:
            return {plan, plan + kThreeArmSpreadDegrees, plan + 2.0 * kThreeArmSpreadDegrees};
        }
        return {};
    };

    const auto throughCase = [](double bend, bool sectionChange) {
        JunctionCase junctionCase;
        junctionCase.throughSection = kSweepThroughSection;
        junctionCase.throughAtJunction = kSweepThroughSection;
        junctionCase.throughPastJunction = sectionChange ? kSweepThroughSection.scaled(kSectionChangeFactor) : kSweepThroughSection;
        junctionCase.throughBendDegrees = bend;
        junctionCase.tags = {QStringLiteral("sweep"),
                             QStringLiteral("bend:") + QString::number(bend),
                             sectionChange ? QStringLiteral("section:x2") : QStringLiteral("section:same")};
        return junctionCase;
    };
    const auto sectionLabel = [](bool sectionChange) {
        return sectionChange ? QStringLiteral("section x2 past J") : QStringLiteral("same section");
    };

    QVector<JunctionCase> cases;
    for (double bend : kSweepBendDegrees) {
        for (bool sectionChange : {false, true}) {
            for (double throughProfile : kJunctionFreeProfileDegrees) {
                JunctionCase junctionCase = throughCase(bend, sectionChange);
                junctionCase.throughProfileDegrees = throughProfile;
                junctionCase.name = QStringLiteral("no arms, through profile %1, bend %2, %3")
                                        .arg(QString::number(throughProfile), QString::number(bend), sectionLabel(sectionChange));
                junctionCase.tags << QStringLiteral("arms:0") << QStringLiteral("through-profile:") + QString::number(throughProfile);
                cases.append(junctionCase);
            }
        }
    }

    for (double plan : kSweepPlanDegrees) {
        for (double profile : kSweepProfileDegrees) {
            if (isVertical(profile) && plan != kVerticalDegrees) {
                continue;
            }
            for (double ratio : kSweepSizeRatios) {
                for (const LayoutSpec& layout : kLayouts) {
                    const QVector<double> armPlans = planAngles(layout.layout, plan);
                    // Several arms straight up or down at J would coincide.
                    if (isVertical(profile) && armPlans.size() > 1) {
                        continue;
                    }
                    for (double bend : kSweepBendDegrees) {
                        for (bool sectionChange : {false, true}) {
                            JunctionCase junctionCase = throughCase(bend, sectionChange);
                            for (double armPlan : armPlans) {
                                junctionCase.arms.append(arm(armPlan, kSweepThroughSection.scaled(ratio), profile));
                            }
                            junctionCase.name = QStringLiteral("plan %1, profile %2, ratio %3, %4, bend %5, %6")
                                                    .arg(QString::number(plan), QString::number(profile), QString::number(ratio),
                                                         QString::fromLatin1(layout.label), QString::number(bend), sectionLabel(sectionChange));
                            junctionCase.tags << QStringLiteral("plan:") + QString::number(plan)
                                              << QStringLiteral("profile:") + QString::number(profile)
                                              << QStringLiteral("ratio:") + QString::number(ratio)
                                              << QStringLiteral("arms:%1").arg(armPlans.size())
                                              << QStringLiteral("layout:") + QString::fromLatin1(layout.tag);
                            if (armsCrowd(junctionCase)) {
                                junctionCase.tags << QStringLiteral("crowded");
                            }
                            cases.append(junctionCase);
                        }
                    }
                }
            }
        }
    }
    return cases;
}

QVector<cwPassageTubeCases::Case> cwPassageTubeCases::catalog()
{
    QVector<Case> cases;
    for (const JunctionCase& junctionCase : galleryCases()) {
        cases.append(toCase(junctionCase));
    }
    cases.append({QStringLiteral("Phake Cave"), {QStringLiteral("phake")}, phakeCave()});
    cases.append(loopCase());
    for (const JunctionCase& junctionCase : sweepCases()) {
        cases.append(toCase(junctionCase));
    }
    return cases;
}

cwPassageTubeCases::Trip cwPassageTubeCases::random(unsigned int seed, int junctionCount)
{
    std::mt19937 generator(seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    const auto between = [&](double low, double high) {
        return low + (high - low) * unit(generator);
    };

    const auto legVector = [&]() {
        const double length = between(kRandomMinLegMeters, kRandomMaxLegMeters);
        const double bearing = between(0.0, kFullTurnDegrees);
        double inclination = between(-kRandomGentleDegrees, kRandomGentleDegrees);
        if (unit(generator) < kRandomSteepFraction) {
            const double sign = unit(generator) < 0.5 ? -1.0 : 1.0;
            inclination = sign * between(kRandomGentleDegrees, kVerticalDegrees);
        }
        return static_cast<float>(length) * direction(bearing, inclination);
    };

    const double logMinLrud = std::log(kRandomMinLrudMeters);
    const double logMaxLrud = std::log(kRandomMaxLrudMeters);
    const auto randomLrud = [&]() {
        if (unit(generator) < kRandomNoLrudFraction) {
            return emptyLrud();
        }
        QStringList dimensions;
        for (int i = 0; i < kLrudReadingCount; ++i) {
            dimensions.append(QString::number(std::exp(between(logMinLrud, logMaxLrud))));
        }
        return dimensions;
    };

    QVector<Station> placed;
    QVector<int> degree;
    const auto addStation = [&](const QVector3D& position) {
        const Station station {QStringLiteral("s%1").arg(placed.size()), position, randomLrud()};
        placed.append(station);
        degree.append(0);
        return station;
    };
    // A passage of legCount legs from the placed station at fromIndex.
    const auto addPassage = [&](int fromIndex, int legCount) {
        QList<Station> chunk;
        chunk.append(Station {placed.at(fromIndex).name, placed.at(fromIndex).position, randomLrud()});
        int previous = fromIndex;
        for (int i = 0; i < legCount; ++i) {
            const QVector3D position = *placed.at(previous).position + legVector();
            chunk.append(addStation(position));
            const int current = static_cast<int>(placed.size()) - 1;
            degree[previous]++;
            degree[current]++;
            previous = current;
        }
        return chunk;
    };

    Trip trip;
    addStation(QVector3D(0.0f, 0.0f, 0.0f));
    trip.chunks.append(addPassage(0, junctionCount + kRandomExtraMainLegs));

    std::uniform_int_distribution<int> branchLegs(kRandomMinBranchLegs, kRandomMaxBranchLegs);
    for (int j = 0; j < junctionCount; ++j) {
        QVector<int> interior;
        for (int i = 0; i < degree.size(); ++i) {
            if (degree.at(i) == 2) {
                interior.append(i);
            }
        }
        std::uniform_int_distribution<int> pick(0, static_cast<int>(interior.size()) - 1);
        trip.chunks.append(addPassage(interior.at(pick(generator)), branchLegs(generator)));
    }
    return trip;
}
