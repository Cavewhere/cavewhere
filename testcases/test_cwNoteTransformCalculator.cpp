//Catch includes
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "CasePairTestHelpers.h"
#include "cwNoteLiDARStation.h"
#include "cwNoteStation.h"
#include "cwNoteTransformCalculator.h"

//Qt includes
#include <QStringList>

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
