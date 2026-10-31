//Catch includes
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "CasePairTestHelpers.h"
#include "cwNoteLiDARStation.h"
#include "cwNoteStation.h"
#include "cwNoteTransformCalculator.h"
#include "cwStationPositionLookup.h"

//Qt includes
#include <QStringList>
#include <QVector3D>

//Std includes
#include <limits>

namespace {

template<typename NoteStation>
QList<NoteStation> noteStationsNamed(const QStringList& names)
{
    QList<NoteStation> stations;
    for (const QString& name : names) {
        NoteStation station;
        station.setName(name);
        stations.append(station);
    }
    return stations;
}

template<typename NoteStation>
QStringList shotNames(const QList<QPair<NoteStation, NoteStation>>& shots)
{
    QStringList names;
    for (const auto& shot : shots) {
        names.append(shot.first.name() + QStringLiteral(" - ") + shot.second.name());
    }
    return names;
}

} // namespace

TEMPLATE_TEST_CASE("A note holding both members of a case pair finds each member's own shots",
                   "[cwNoteTransformCalculator][CasePairs]", cwNoteStation, cwNoteLiDARStation)
{
    // Q1 is typed in uppercase and resolves to q1 through the lowercase
    // fallback. p3 is off the note, so p2 - p3 is no note shot.
    const QList<TestType> noteStations = noteStationsNamed<TestType>(
        {QStringLiteral("p2"), QStringLiteral("Q1"), QStringLiteral("P2"),
         QStringLiteral("p1"), QStringLiteral("P3"), QStringLiteral("P1")});

    const auto shots = cwNoteTransformCalculator::noteShots(noteStations, casePairs::solvedNetwork());

    // Stations run in ascending order, a pair's uppercase member first, so a
    // shot between pair members always runs the same way.
    CHECK(shotNames(shots).join(QStringLiteral(", ")).toStdString()
          == "P1 - p1, P1 - P2, p1 - p2, P2 - P3, P3 - Q1");
}

namespace {

constexpr double kGoodScale = 100.0;
constexpr double kOtherGoodScale = 300.0;
constexpr double kStrayScale = 50.0;

const QVector3D kNorthError(0.0f, 1.0f, 0.0f);
const QVector3D kEastError(1.0f, 0.0f, 0.0f);

cwNoteTransformCalculator::ProfileTransform planTransform(const cwStationPositionLookup& positions)
{
    return cwNoteTransformCalculator::ProfileTransform {cwScrapType::Plan,
                                                        QMatrix4x4(),
                                                        QMatrix4x4(),
                                                        QMatrix4x4(),
                                                        QMatrix4x4(),
                                                        positions};
}

cwNoteStation noteStationAt(const QString& name, const QPointF& positionOnNote)
{
    cwNoteStation station;
    station.setName(name);
    station.setPositionOnNote(positionOnNote);
    return station;
}

} // namespace

TEST_CASE("Averaging skips shots with a non-finite scale or error vector",
          "[cwNoteTransformCalculator][DegenerateShots]")
{
    using ShotTransform = cwNoteTransformCalculator::ShotTransform;
    const double quietNan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const float nanf = std::numeric_limits<float>::quiet_NaN();
    const QVector3D nanError(nanf, nanf, nanf);

    SECTION("Good shots average among themselves")
    {
        const ShotTransform average = cwNoteTransformCalculator::averageTransformations(
            {ShotTransform(kGoodScale, kNorthError),
             ShotTransform(quietNan, nanError),
             ShotTransform(infinity, kEastError),
             ShotTransform(-kStrayScale, kEastError),
             ShotTransform(kStrayScale, nanError),
             ShotTransform(kOtherGoodScale, kNorthError)});

        CHECK(average.Scale == Catch::Approx((kGoodScale + kOtherGoodScale) / 2.0));
        CHECK(average.ErrorVector.x() == Catch::Approx(kNorthError.x()));
        CHECK(average.ErrorVector.y() == Catch::Approx(kNorthError.y()));
        CHECK(average.ErrorVector.z() == Catch::Approx(kNorthError.z()));
    }

    SECTION("Only degenerate shots give the no-valid-transform result")
    {
        const ShotTransform average = cwNoteTransformCalculator::averageTransformations(
            {ShotTransform(quietNan, nanError), ShotTransform(infinity, kEastError)});

        CHECK(average.Scale == 0.0);
    }
}

TEST_CASE("A plan shot whose stations coincide on the page is an invalid shot transform",
          "[cwNoteTransformCalculator][DegenerateShots]")
{
    cwStationPositionLookup positions;
    positions.setPosition(QStringLiteral("B"), QVector3D(0.0f, 10.0f, 0.0f));
    positions.setPosition(QStringLiteral("C"), QVector3D(0.0f, 10.0f, -5.0f));
    positions.setPosition(QStringLiteral("D"), QVector3D(4.0f, 10.0f, 0.0f));

    const QPointF samePagePosition(0.5, 0.5);
    const cwNoteStation b = noteStationAt(QStringLiteral("B"), samePagePosition);

    SECTION("A vertical leg has no length in the cave or on the page")
    {
        const cwNoteStation c = noteStationAt(QStringLiteral("C"), samePagePosition);
        const auto transform = cwNoteTransformCalculator::calculateShotTransformation(
            b, c, planTransform(positions));
        CHECK(transform.Scale == 0.0);
    }

    SECTION("A horizontal leg drawn as a point has no length on the page")
    {
        const cwNoteStation d = noteStationAt(QStringLiteral("D"), samePagePosition);
        const auto transform = cwNoteTransformCalculator::calculateShotTransformation(
            b, d, planTransform(positions));
        CHECK(transform.Scale == 0.0);
    }
}
