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

// Cave-like random surveys: a leg turns at most this much in 3D from the one
// before.
constexpr double kRandomMaxBendDegrees = 90.0;
// A caver sets stations farther apart than the passage is wide: a leg is at
// least this many times the largest reading at its stations.
constexpr double kRandomLegPerReading = 2.5;
// A passage is at most this many times wider than tall or taller than wide.
constexpr double kRandomMaxSectionAspect = 3.0;
constexpr int kRandomPlacementAttempts = 200;

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

double distanceToSegment(const QVector3D& point, const QVector3D& start, const QVector3D& end)
{
    const QVector3D along = end - start;
    const float lengthSquared = along.lengthSquared();
    const float t = lengthSquared > 0.0f ? std::clamp(QVector3D::dotProduct(point - start, along) / lengthSquared, 0.0f, 1.0f) : 0.0f;
    return point.distanceToPoint(start + t * along);
}

// The shortest distance between the segments ab and cd.
double distanceBetweenSegments(const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d)
{
    const QVector3D u = b - a;
    const QVector3D v = d - c;
    const QVector3D w = a - c;
    const double uu = QVector3D::dotProduct(u, u);
    const double uv = QVector3D::dotProduct(u, v);
    const double vv = QVector3D::dotProduct(v, v);
    const double uw = QVector3D::dotProduct(u, w);
    const double vw = QVector3D::dotProduct(v, w);
    const double denominator = uu * vv - uv * uv;
    double s = denominator > 0.0 ? std::clamp((uv * vw - vv * uw) / denominator, 0.0, 1.0) : 0.0;
    double t = vv > 0.0 ? std::clamp((uv * s + vw) / vv, 0.0, 1.0) : 0.0;
    s = uu > 0.0 ? std::clamp((uv * t - uw) / uu, 0.0, 1.0) : 0.0;
    const QVector3D closestOnAb = a + static_cast<float>(s) * u;
    const QVector3D closestOnCd = c + static_cast<float>(t) * v;
    return closestOnAb.distanceToPoint(closestOnCd);
}

double angleBetweenDegrees(const QVector3D& a, const QVector3D& b)
{
    const double cosine = QVector3D::dotProduct(a.normalized(), b.normalized());
    return qRadiansToDegrees(std::acos(std::clamp(cosine, -1.0, 1.0)));
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
        const auto lrudOf = [&](const Section& section) { return spec.hasLrud ? lrudStrings(section) : emptyLrud(); };
        double azimuth = kHalfTurnDegrees - spec.planDegrees;
        const QVector3D far = junction + static_cast<float>(spec.lengthMeters) * direction(azimuth, spec.profileDegrees);
        const QStringList junctionLrud = spec.hasJunctionLrud ? lrudOf(spec.atJunction) : emptyLrud();
        const Station farStation {QStringLiteral("a%1%2").arg(i).arg(0), far, lrudOf(spec.at)};
        trip.chunks.append({farStation, {junctionName, junction, junctionLrud}});

        if (spec.legsBeyond.isEmpty()) {
            continue;
        }
        QList<Station> beyond {farStation};
        for (const ArmLeg& leg : spec.legsBeyond) {
            azimuth -= leg.turnDegrees;
            const QVector3D next = *beyond.last().position + static_cast<float>(leg.lengthMeters) * direction(azimuth, leg.profileDegrees);
            beyond.append({QStringLiteral("a%1%2").arg(i).arg(beyond.size()), next, lrudOf(leg.at)});
        }
        trip.chunks.append(beyond);
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

    // The arm leaves the room through its side wall, turns back and runs
    // along it, outside, past the room's end, its wall reaching into the room.
    const Section sideArm {3, 2, 3.5, 2};
    ArmSpec alongWall = arm(70, sideArm, 20.0);
    alongWall.legsBeyond = {{45.0, 0.0, 3.0, sideArm}, {80.0, 0.0, 16.0, sideArm}};
    cases.append(galleryCase(QStringLiteral("21. Small arm along the wall of a big room"), {8, 8, 6, 1.5}, {alongWall}));

    // The arm drops out through the floor and, 4.9 m from J, turns 74 degrees
    // to climb back beside it, outside, its wall reaching into the room.
    ArmSpec unread = arm(90, {}, -22.0, 4.9);
    unread.hasLrud = false;
    unread.legsBeyond = {{61.7, 21.0, 6.0, {}}};
    cases.append(galleryCase(QStringLiteral("22. Default-radius arm bending inside a big through's reach"), {6, 6, 6, 1.5}, {unread}));

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

cwPassageTubeCases::Trip cwPassageTubeCases::randomWild(unsigned int seed, int junctionCount)
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

cwPassageTubeCases::Trip cwPassageTubeCases::random(unsigned int seed, int junctionCount)
{
    std::mt19937 generator(seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    const auto between = [&](double low, double high) {
        return low + (high - low) * unit(generator);
    };

    // Readings as left, right, up and down in log meters.
    using LogLrud = std::array<double, kLrudReadingCount>;
    const double logMinLrud = std::log(kRandomMinLrudMeters);
    const double logMaxLrud = std::log(kRandomMaxLrudMeters);
    const double logMaxStep = std::log(kRandomMaxLrudStep);
    enum Reading {
        Left,
        Right,
        Up,
        Down
    };

    struct Placed {
        QVector3D position;
        LogLrud walk;
        // The legs at this station, as the station index at their other end.
        QVector<int> neighbors;
    };
    QVector<Placed> placed;
    const auto name = [](int index) {
        return QStringLiteral("s%1").arg(index);
    };
    const auto toStrings = [](const LogLrud& walk) {
        QStringList dimensions;
        for (double logMeters : walk) {
            dimensions.append(QString::number(std::exp(logMeters)));
        }
        return dimensions;
    };

    const auto largestMeters = [](const LogLrud& walk) {
        return std::exp(*std::max_element(walk.cbegin(), walk.cend()));
    };

    // Whether a leg from the station at fromIndex to end, with walls reaching
    // reach, keeps its stations kRandomClearanceMeters from the other legs
    // and its walls that far from theirs.
    const auto isClear = [&](int fromIndex, const QVector3D& end, double reach) {
        const QVector3D& start = placed.at(fromIndex).position;
        for (int i = 0; i < placed.size(); ++i) {
            const Placed& station = placed.at(i);
            if (i != fromIndex && distanceToSegment(station.position, start, end) < kRandomClearanceMeters) {
                return false;
            }
            for (int neighbor : station.neighbors) {
                const Placed& other = placed.at(neighbor);
                if (distanceToSegment(end, station.position, other.position) < kRandomClearanceMeters) {
                    return false;
                }
                const bool sharesStation = i == fromIndex || neighbor == fromIndex;
                const double otherReach = std::max(largestMeters(station.walk), largestMeters(other.walk));
                if (!sharesStation
                    && distanceBetweenSegments(start, end, station.position, other.position) < kRandomClearanceMeters + reach + otherReach) {
                    return false;
                }
            }
        }
        return true;
    };

    const auto drawInclination = [&](bool isEnd) {
        if (!isEnd && unit(generator) < kRandomSteepFraction) {
            const double sign = unit(generator) < 0.5 ? -1.0 : 1.0;
            return sign * between(kRandomGentleDegrees, kVerticalDegrees);
        }
        return between(-kRandomGentleDegrees, kRandomGentleDegrees);
    };

    // A leg from the station at fromIndex: along a bearing drifting from the
    // passage's previous one, or for a branch's first leg clear of the legs
    // already at its junction. A passage starts and ends on a gentle leg, as
    // a survey reaches the floor of a pit before it stops. Empty when every
    // attempt runs into the cave.
    struct Draw {
        QVector3D end;
        double bearing = 0.0;
        double inclination = 0.0;
    };
    const auto drawLeg = [&](int fromIndex, const std::optional<Draw>& previous, const LogLrud& walk, bool isEnd) -> std::optional<Draw> {
        const Placed& from = placed.at(fromIndex);
        const double reach = std::max(largestMeters(from.walk), largestMeters(walk));
        const double minLength = std::max(kRandomMinLegMeters, kRandomLegPerReading * reach);
        const double maxLength = std::max(minLength, kRandomMaxLegMeters);
        for (int attempt = 0; attempt < kRandomPlacementAttempts; ++attempt) {
            Draw draw;
            draw.bearing = previous ? previous->bearing + between(-kRandomMaxTurnDegrees, kRandomMaxTurnDegrees)
                                    : between(0.0, kFullTurnDegrees);
            draw.inclination = drawInclination(isEnd);
            const QVector3D heading = direction(draw.bearing, draw.inclination);
            draw.end = from.position + static_cast<float>(between(minLength, maxLength)) * heading;

            const bool turnsGently = !previous
                                     || angleBetweenDegrees(direction(previous->bearing, previous->inclination), heading) <= kRandomMaxBendDegrees;
            const bool leavesJunctionClear = previous || std::all_of(from.neighbors.cbegin(), from.neighbors.cend(), [&](int neighbor) {
                return angleBetweenDegrees(placed.at(neighbor).position - from.position, heading) >= kRandomMinBranchDegrees;
            });
            if (turnsGently && leavesJunctionClear && isClear(fromIndex, draw.end, reach)) {
                return draw;
            }
        }
        return std::nullopt;
    };

    const auto isPassageShaped = [](const LogLrud& walk) {
        const double width = std::exp(walk.at(Left)) + std::exp(walk.at(Right));
        const double height = std::exp(walk.at(Up)) + std::exp(walk.at(Down));
        return std::max(width, height) <= kRandomMaxSectionAspect * std::min(width, height);
    };
    // A step of the walk that keeps the section passage-shaped, or the walk
    // unchanged when every attempt flattens it.
    const auto nextWalk = [&](const LogLrud& walk) {
        for (int attempt = 0; attempt < kRandomPlacementAttempts; ++attempt) {
            LogLrud next;
            for (int i = 0; i < kLrudReadingCount; ++i) {
                next[i] = std::clamp(walk.at(i) + between(-logMaxStep, logMaxStep), logMinLrud, logMaxLrud);
            }
            if (isPassageShaped(next)) {
                return next;
            }
        }
        return walk;
    };

    const auto addStation = [&](const QVector3D& position, const LogLrud& walk) {
        placed.append(Placed {position, walk, {}});
        return static_cast<int>(placed.size()) - 1;
    };

    // A passage of up to legCount legs from the placed station at fromIndex,
    // or empty when its first leg finds no room. A shaft's top station reads
    // down to the bottom's floor and its bottom station up to the top's
    // ceiling; a station between two shafts keeps the deeper reading.
    const auto addPassage = [&](int fromIndex, int legCount) {
        QList<Station> chunk;
        std::optional<Draw> previous;
        int current = fromIndex;
        LogLrud currentLrud = placed.at(fromIndex).walk;
        for (int i = 0; i < legCount; ++i) {
            const LogLrud walk = nextWalk(placed.at(current).walk);
            const bool isMainPassageStart = i == 0 && fromIndex == 0;
            const std::optional<Draw> draw = drawLeg(current, previous, walk, isMainPassageStart || i == legCount - 1);
            if (!draw) {
                break;
            }
            if (chunk.isEmpty()) {
                chunk.append(Station {name(current), placed.at(current).position, toStrings(currentLrud)});
            }
            const int next = addStation(draw->end, walk);
            placed[current].neighbors.append(next);
            placed[next].neighbors.append(current);

            LogLrud nextLrud = placed.at(next).walk;
            if (std::abs(draw->inclination) >= kRandomShaftDegrees) {
                const double depth = std::abs(draw->end.z() - placed.at(current).position.z());
                const bool isDescending = draw->inclination < 0.0;
                const Reading towardNext = isDescending ? Down : Up;
                const Reading towardCurrent = isDescending ? Up : Down;
                currentLrud[towardNext] = std::max(currentLrud.at(towardNext), std::log(depth + std::exp(nextLrud.at(towardNext))));
                nextLrud[towardCurrent] = std::log(depth + std::exp(currentLrud.at(towardCurrent)));
                chunk.last().lrud = toStrings(currentLrud);
            }
            chunk.append(Station {name(next), draw->end, toStrings(nextLrud)});
            previous = draw;
            current = next;
            currentLrud = nextLrud;
        }
        return chunk;
    };

    Trip trip;
    LogLrud firstWalk;
    do {
        for (double& reading : firstWalk) {
            reading = between(logMinLrud, logMaxLrud);
        }
    } while (!isPassageShaped(firstWalk));
    addStation(QVector3D(0.0f, 0.0f, 0.0f), firstWalk);
    trip.chunks.append(addPassage(0, junctionCount + kRandomExtraMainLegs));

    std::uniform_int_distribution<int> branchLegs(kRandomMinBranchLegs, kRandomMaxBranchLegs);
    for (int j = 0; j < junctionCount; ++j) {
        QVector<int> interior;
        for (int i = 0; i < placed.size(); ++i) {
            if (placed.at(i).neighbors.size() == 2) {
                interior.append(i);
            }
        }
        if (interior.isEmpty()) {
            break;
        }
        std::uniform_int_distribution<int> pick(0, static_cast<int>(interior.size()) - 1);
        const int legCount = branchLegs(generator);
        for (int attempt = 0; attempt < kRandomPlacementAttempts; ++attempt) {
            const QList<Station> branch = addPassage(interior.at(pick(generator)), legCount);
            if (!branch.isEmpty()) {
                trip.chunks.append(branch);
                break;
            }
        }
    }
    return trip;
}
