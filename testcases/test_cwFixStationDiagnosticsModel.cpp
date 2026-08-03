//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationDiagnosticsModel.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwSurveyNetwork.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

namespace {

//! The diagnostics proxy for a cave that already carries one blank fix, plus the
//! source model and the region behind it — the shape every check below starts
//! from.
struct FixFixture {
    cwCavingRegion region;
    cwCave* cave = nullptr;
    cwFixStationModel* source = nullptr;
    cwFixStationDiagnosticsModel* diagnostics = nullptr;
    QModelIndex sourceIndex;
    QModelIndex proxyIndex;

    FixFixture()
    {
        region.addCave();
        cave = region.cave(0);
        source = cave->fixStations();
        diagnostics = cave->fixStationDiagnostics();
        source->addFixStation();
        sourceIndex = source->index(0);
        proxyIndex = diagnostics->index(0, 0);
    }

    //! Edit the row's persisted data, exactly as the page's delegates do.
    void edit(int role, const QVariant& value) { source->setData(sourceIndex, value, role); }

    void setGlobalCS(const QString& cs)
    {
        region.geoReference()->setGlobalCoordinateSystem(cs);
    }

    QVariant read(int role) const { return diagnostics->data(proxyIndex, role); }

    QString domainError() const
    {
        return read(cwFixStationDiagnosticsModel::DomainErrorRole).toString();
    }
    QString stationError() const
    {
        return read(cwFixStationDiagnosticsModel::StationErrorRole).toString();
    }
    QString coordinateError() const
    {
        return read(cwFixStationDiagnosticsModel::CoordinateErrorRole).toString();
    }
    bool orderUnknown() const
    {
        return read(cwFixStationDiagnosticsModel::CoordinateOrderUnknownRole).toBool();
    }

    //! Put text on the row that cwFixStationModel::setCoordinateText() would
    //! refuse. Only the load path produces such a row — a hand-edited project —
    //! so there is no editing surface to go through.
    void setStoredCoordinate(const QString& text)
    {
        cwFixStation fix = source->fixStationAt(0);
        fix.setCoordinate(text);
        source->setFixStations({fix});
    }
    bool eastingFlagged() const
    {
        return read(cwFixStationDiagnosticsModel::EastingDomainErrorRole).toBool();
    }
    bool northingFlagged() const
    {
        return read(cwFixStationDiagnosticsModel::NorthingDomainErrorRole).toBool();
    }
};

//! Every role carried by every dataChanged the spy has seen, flattened.
QList<int> rolesSeen(const QSignalSpy& spy)
{
    QList<int> seen;
    for (int i = 0; i < spy.count(); ++i) {
        seen.append(spy.at(i).at(2).value<QList<int>>());
    }
    return seen;
}

} // namespace

TEST_CASE("cwFixStationDiagnosticsModel merges the source's role names with its own",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    FixFixture fixture;
    const QHash<int, QByteArray> roles = fixture.diagnostics->roleNames();

    // The persisted roles pass straight through, so the delegates keep binding
    // stationName / easting / ... exactly as they did against the source model.
    CHECK(roles.value(cwFixStationModel::StationNameRole) == "stationName");
    CHECK(roles.value(cwFixStationModel::InputCSRole) == "inputCS");
    CHECK(roles.value(cwFixStationModel::EastingRole) == "easting");
    CHECK(roles.value(cwFixStationModel::NorthingRole) == "northing");
    CHECK(roles.value(cwFixStationModel::ElevationRole) == "elevation");
    CHECK(roles.value(cwFixStationModel::IdRole) == "id");

    CHECK(roles.value(cwFixStationDiagnosticsModel::DomainErrorRole) == "domainError");
    CHECK(roles.value(cwFixStationDiagnosticsModel::EastingDomainErrorRole)
          == "eastingDomainError");
    CHECK(roles.value(cwFixStationDiagnosticsModel::NorthingDomainErrorRole)
          == "northingDomainError");
    CHECK(roles.value(cwFixStationDiagnosticsModel::StationErrorRole) == "stationError");
    CHECK(roles.value(cwFixStationDiagnosticsModel::CoordinateErrorRole) == "coordinateError");
    CHECK(roles.value(cwFixStationDiagnosticsModel::CoordinateOrderUnknownRole)
          == "coordinateOrderUnknown");

    // The two role blocks must not collide, or the merge would silently drop one
    // side: every name in the merged hash is still reachable by its own value.
    CHECK(roles.size() == fixture.source->roleNames().size() + 6);
}

TEST_CASE("cwFixStationDiagnosticsModel passes persisted rows through untouched",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    FixFixture fixture;

    // An edit made through the proxy reaches the source model, and reads of the
    // persisted roles come back from it — this is what lets FixStationPage bind
    // its delegates to the proxy while still calling setData on the source.
    REQUIRE(fixture.diagnostics->setData(fixture.proxyIndex, QStringLiteral("A1"),
                                         cwFixStationModel::StationNameRole));
    CHECK(fixture.source->fixStationAt(0).stationName() == QStringLiteral("A1"));
    CHECK(fixture.read(cwFixStationModel::StationNameRole).toString()
          == QStringLiteral("A1"));

    // Rows line up one-to-one, so a delegate's index is a source row index.
    fixture.source->addFixStation();
    CHECK(fixture.diagnostics->rowCount() == 2);
    CHECK(fixture.diagnostics->rowCount() == fixture.source->rowCount());
}

TEST_CASE("cwFixStationDiagnosticsModel flags only the out-of-domain coordinate component",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    FixFixture fixture;

    // A clean in-zone UTM 13N coordinate flags neither component.
    fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));
    fixture.edit(cwFixStationModel::EastingRole, 478000.0);
    fixture.edit(cwFixStationModel::NorthingRole, 4430000.0);
    CHECK_FALSE(fixture.eastingFlagged());
    CHECK_FALSE(fixture.northingFlagged());
    CHECK(fixture.domainError().isEmpty());

    // A transposed-digit easting flags the easting cell alone.
    fixture.edit(cwFixStationModel::EastingRole, 1478000.0);
    CHECK(fixture.eastingFlagged());
    CHECK_FALSE(fixture.northingFlagged());
    CHECK_FALSE(fixture.domainError().isEmpty());
}

TEST_CASE("cwFixStationDiagnosticsModel StationErrorRole flags empty and unknown names",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    FixFixture fixture;

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    fixture.cave->setSurveyNetwork(network);

    // A blank row names no survey station — survex silently drops such a fix,
    // so it is flagged rather than treated as a valid anchor.
    CHECK_FALSE(fixture.stationError().isEmpty());

    // A name that exists in the survey (case-insensitively) is fine.
    fixture.edit(cwFixStationModel::StationNameRole, QStringLiteral("a1"));
    CHECK(fixture.stationError().isEmpty());

    // A name no station matches is flagged, and the message names it.
    fixture.edit(cwFixStationModel::StationNameRole, QStringLiteral("ZZ9"));
    CHECK(fixture.stationError().contains(QStringLiteral("ZZ9")));

    // A station appearing in the survey clears the flag live.
    cwSurveyNetwork grown;
    grown.addShot(QStringLiteral("A1"), QStringLiteral("ZZ9"));
    fixture.cave->setSurveyNetwork(grown);
    CHECK(fixture.stationError().isEmpty());
}

TEST_CASE("cwFixStationDiagnosticsModel StationErrorRole ignores surrounding whitespace",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // The survex export trims before matching (validateFixStations), so a stray
    // space still anchors the station — the warning must not contradict that.
    FixFixture fixture;

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    fixture.cave->setSurveyNetwork(network);

    fixture.edit(cwFixStationModel::StationNameRole, QStringLiteral(" A1 "));
    CHECK(fixture.stationError().isEmpty());

    // Whitespace-only is still an empty name, not a match.
    fixture.edit(cwFixStationModel::StationNameRole, QStringLiteral("   "));
    CHECK_FALSE(fixture.stationError().isEmpty());
}

TEST_CASE("cwFixStationDiagnosticsModel StationErrorRole defers a named fix when the network is empty",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // No survey network computed yet — a named fix can't be judged, so it must
    // not cry wolf. An empty name is invalid regardless of the network, though.
    FixFixture fixture;

    // Empty name is flagged even before a network exists (the empty check runs
    // before the "nothing to check against" deferral).
    CHECK_FALSE(fixture.stationError().isEmpty());

    // A named fix defers while there's no network to match it against.
    fixture.edit(cwFixStationModel::StationNameRole, QStringLiteral("A1"));
    CHECK(fixture.stationError().isEmpty());
}

TEST_CASE("cwFixStationDiagnosticsModel domain roles are judged under the row's own CS",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // The row is judged under the system it declares and nothing else. The
    // region's is geographic, where the corrected easting below is nowhere near
    // a valid longitude — so a build that judged under the region's CS would
    // flag the coordinate this test ends by clearing.
    FixFixture fixture;
    fixture.setGlobalCS(QStringLiteral("EPSG:4326"));

    fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));
    // A transposed leading digit on the easting — out of zone 13's area of use.
    fixture.edit(cwFixStationModel::EastingRole, 1478000.0);
    fixture.edit(cwFixStationModel::NorthingRole, 4430000.0);
    CHECK(fixture.eastingFlagged());
    CHECK_FALSE(fixture.domainError().isEmpty());

    // Correcting it clears the flag.
    fixture.edit(cwFixStationModel::EastingRole, 478000.0);
    CHECK_FALSE(fixture.eastingFlagged());
}

TEST_CASE("cwFixStationDiagnosticsModel does not judge a row that declares no CS",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // The region's CS is not a stand-in for one the row never declared: nothing
    // is derived from the coordinate at all, so there is no point to place
    // inside or outside a domain. Such a row wants a different complaint.
    FixFixture fixture;
    fixture.setGlobalCS(QStringLiteral("EPSG:32613"));

    fixture.edit(cwFixStationModel::InputCSRole, QString());
    REQUIRE(fixture.source->setCoordinateText(0, QStringLiteral("1478000, 4430000, 1655m"),
                                              cwUnits::Metric) == QString());
    REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::NoSystem);

    CHECK_FALSE(fixture.eastingFlagged());
    CHECK(fixture.domainError().isEmpty());

    // The premise: naming the system really does flag it, so the checks above
    // can't pass for free.
    fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));
    CHECK(fixture.eastingFlagged());
}

TEST_CASE("A solve refreshes the diagnostics without touching the source model",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // The whole point of the proxy. The derived warnings refresh on every solve,
    // but cwFixStationModel::dataChanged means "persisted data changed" and
    // nothing else — cwSaveLoad, cwLinePlotManager, cwFixStationValidator and
    // cwTripCalibration connect to the source, so they cannot see a diagnostics
    // refresh even by accident. Before the split each had to remember a role
    // filter, and the two that forgot dirtied the project and re-solved in a
    // loop, silently.
    FixFixture fixture;
    fixture.edit(cwFixStationModel::StationNameRole, QStringLiteral("A1"));

    QSignalSpy sourceSpy(fixture.source, &QAbstractItemModel::dataChanged);
    QSignalSpy proxySpy(fixture.diagnostics, &QAbstractItemModel::dataChanged);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    fixture.cave->setSurveyNetwork(network);

    CHECK(sourceSpy.count() == 0);
    REQUIRE(proxySpy.count() == 1);
    CHECK(rolesSeen(proxySpy) == QList<int>{cwFixStationDiagnosticsModel::StationErrorRole});
}

TEST_CASE("A global CS change leaves the fix stations alone",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // Rows follow their own CS, so re-projecting the project must not disturb
    // them — not the persisted data, and not the warnings drawn on it. This is
    // what "the fix station stays where it was entered" amounts to in signals.
    FixFixture fixture;
    fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));
    fixture.edit(cwFixStationModel::EastingRole, 478000.0);
    fixture.edit(cwFixStationModel::NorthingRole, 4430000.0);

    QSignalSpy sourceSpy(fixture.source, &QAbstractItemModel::dataChanged);
    QSignalSpy proxySpy(fixture.diagnostics, &QAbstractItemModel::dataChanged);

    // Geographic, so a build that re-judged the row under it would find this
    // easting far outside any valid longitude and flag it.
    fixture.setGlobalCS(QStringLiteral("EPSG:4326"));

    CHECK(sourceSpy.count() == 0);
    CHECK(proxySpy.count() == 0);
    CHECK_FALSE(fixture.eastingFlagged());
}

TEST_CASE("A persisted edit re-emits the derived roles it invalidates",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // The delegates bind domainError / stationError to the proxy, so an edit to
    // the coordinate or the station name has to reach them — the source's own
    // dataChanged carries only the edited role now.
    FixFixture fixture;
    QSignalSpy proxySpy(fixture.diagnostics, &QAbstractItemModel::dataChanged);

    fixture.edit(cwFixStationModel::EastingRole, 478000.0);
    // Two emits: the identity proxy forwards the source's edited role, then we
    // add the derived roles that edit invalidates.
    CHECK(proxySpy.count() == 2);
    QList<int> seen = rolesSeen(proxySpy);
    CHECK(seen.contains(cwFixStationModel::EastingRole));
    CHECK(seen.contains(cwFixStationDiagnosticsModel::DomainErrorRole));
    CHECK(seen.contains(cwFixStationDiagnosticsModel::EastingDomainErrorRole));
    CHECK(seen.contains(cwFixStationDiagnosticsModel::NorthingDomainErrorRole));
    CHECK_FALSE(seen.contains(cwFixStationDiagnosticsModel::StationErrorRole));

    proxySpy.clear();
    fixture.edit(cwFixStationModel::StationNameRole, QStringLiteral("A1"));
    CHECK(proxySpy.count() == 2);
    seen = rolesSeen(proxySpy);
    CHECK(seen.contains(cwFixStationModel::StationNameRole));
    CHECK(seen.contains(cwFixStationDiagnosticsModel::StationErrorRole));
    CHECK_FALSE(seen.contains(cwFixStationDiagnosticsModel::DomainErrorRole));

    // A variance edit touches no derived verdict, so only the passthrough fires.
    proxySpy.clear();
    fixture.edit(cwFixStationModel::HorizontalVarianceRole, 2.5);
    CHECK(proxySpy.count() == 1);
    CHECK(rolesSeen(proxySpy) == QList<int>{cwFixStationModel::HorizontalVarianceRole});
}

TEST_CASE("cwFixStationDiagnosticsModel says why a coordinate can't be read",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // Before this role nothing surfaced either failure: such a row renders and
    // solves as a fix at the origin, and the only place its text appeared was
    // inside an editor nobody had reason to open.
    FixFixture fixture;

    SECTION("a row nobody has filled in yet is not a complaint") {
        REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::Empty);
        CHECK(fixture.coordinateError().isEmpty());
    }

    SECTION("a coordinate that reads is not a complaint") {
        fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));
        REQUIRE(fixture.source->setCoordinateText(0, QStringLiteral("478000, 4430000, 1655m"),
                                                  cwUnits::Metric) == QString());
        REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::Valid);
        CHECK(fixture.coordinateError().isEmpty());
    }

    SECTION("no coordinate system asks for one, and doesn't blame the text") {
        fixture.edit(cwFixStationModel::InputCSRole, QString());
        fixture.setStoredCoordinate(QStringLiteral("610016.792, 5615117.075, 304m"));
        REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::NoSystem);

        CHECK(fixture.coordinateError().contains(QStringLiteral("coordinate system")));
        // The text is perfectly readable; what is missing is the system to read
        // it under. Telling the user their coordinate can't be read would send
        // them to correct a string that has nothing wrong with it.
        CHECK_FALSE(fixture.coordinateError().contains(QStringLiteral("can't be read")));
    }

    SECTION("text that won't parse carries the parser's own reason") {
        fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));

        fixture.setStoredCoordinate(QStringLiteral("1, 2, 3, 4"));
        REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::Unreadable);
        const QString tooMany = fixture.coordinateError();
        CHECK(tooMany.contains(QStringLiteral("at most three numbers")));

        fixture.setStoredCoordinate(QStringLiteral("478000"));
        REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::Unreadable);
        const QString tooFew = fixture.coordinateError();
        CHECK(tooFew.contains(QStringLiteral("easting and a northing")));

        // Forwarded, not restated: a message that named the failure in its own
        // words would be the same string for both of these, and would drift from
        // what the parser actually refused.
        CHECK(tooMany != tooFew);
    }

    SECTION("the parse reason is read in the row's own axis order") {
        // The same missing component, under a geographic system, is a missing
        // latitude and longitude — the message has to match the row the user is
        // looking at, not the model's easting/northing order.
        fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:4326"));
        fixture.setStoredCoordinate(QStringLiteral("46.12113"));
        REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::Unreadable);

        CHECK(fixture.coordinateError().contains(QStringLiteral("latitude and a longitude")));
        CHECK_FALSE(fixture.coordinateError().contains(QStringLiteral("easting")));
    }
}

TEST_CASE("cwFixStationDiagnosticsModel never raises both coordinate complaints at once",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // The page gives the two one warning slot between them, which is only sound
    // because they can't both speak: the domain check judges a coordinate the
    // row has, and CoordinateErrorRole says there isn't one to judge.
    FixFixture fixture;

    SECTION("an out-of-domain coordinate is the domain's complaint alone") {
        fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));
        REQUIRE(fixture.source->setCoordinateText(0, QStringLiteral("1478000, 4430000, 1655m"),
                                                  cwUnits::Metric) == QString());
        CHECK_FALSE(fixture.domainError().isEmpty());
        CHECK(fixture.coordinateError().isEmpty());
    }

    SECTION("a coordinate with no system is the coordinate's complaint alone") {
        // A row with no CS reaches no domain check at all — cwCoordinateTransform
        // returns early on an empty key — so "the domain is quiet" holds here
        // however the fix-level deferral behaves, and asserting only that would
        // prove nothing. What is worth pinning is the hand-off: naming the system
        // makes these same numbers the domain's complaint, and silences this one.
        fixture.edit(cwFixStationModel::InputCSRole, QString());
        fixture.setStoredCoordinate(QStringLiteral("1478000, 4430000, 1655m"));
        CHECK_FALSE(fixture.coordinateError().isEmpty());
        CHECK(fixture.domainError().isEmpty());

        fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));
        CHECK(fixture.coordinateError().isEmpty());
        CHECK_FALSE(fixture.domainError().isEmpty());
    }

    SECTION("unreadable text is the coordinate's complaint alone") {
        // A southern-hemisphere zone on purpose. An unreadable row reports three
        // zeros, and in a northern zone the origin lands inside the area of use —
        // so the domain would stay quiet whether or not it deferred, and this
        // would pass for free. Here the origin is out of domain, so the silence
        // has to come from the deferral.
        const QString cs = QStringLiteral("EPSG:28355");
        REQUIRE_FALSE(cwCoordinateTransform::domainCheck(cs, cwGeoPoint(0.0, 0.0, 0.0))
                          .northingValid);

        fixture.edit(cwFixStationModel::InputCSRole, cs);
        fixture.setStoredCoordinate(QStringLiteral("N 46 07 16 W 115 35 56"));
        REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::Unreadable);
        CHECK_FALSE(fixture.coordinateError().isEmpty());
        CHECK(fixture.domainError().isEmpty());
    }
}

TEST_CASE("cwFixStationDiagnosticsModel marks the row whose text has no recorded order",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // What the entry surfaces ask about before they let a geographic system be
    // named on such a row: naming one reads the text latitude-first whatever it
    // was written as, and afterwards there is nothing left to detect.
    FixFixture fixture;

    SECTION("a row that names a system derives its order from it") {
        fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));
        REQUIRE(fixture.source->setCoordinateText(0, QStringLiteral("478000, 4430000, 1655m"),
                                                  cwUnits::Metric) == QString());
        CHECK_FALSE(fixture.orderUnknown());
    }

    SECTION("a row with no coordinate has no order to be unsure of") {
        // Blank CS, blank coordinate: nothing was written, so nothing is at
        // stake and there is nothing to ask about.
        fixture.edit(cwFixStationModel::InputCSRole, QString());
        REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::Empty);
        CHECK_FALSE(fixture.orderUnknown());
    }

    SECTION("text with no system to read it under is the one case") {
        fixture.edit(cwFixStationModel::InputCSRole, QString());
        fixture.setStoredCoordinate(QStringLiteral("610016.792, 5615117.075, 304m"));
        CHECK(fixture.orderUnknown());

        // And naming a system settles it, which is why the surfaces have to read
        // this before they commit one rather than after.
        fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:4326"));
        CHECK_FALSE(fixture.orderUnknown());
    }

    SECTION("unreadable text under a system still has an order") {
        // Unreadable is not this: the row knows how it would read the text if
        // the text were readable, so there is no question to put to the user.
        fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));
        fixture.setStoredCoordinate(QStringLiteral("N 46 07 16 W 115 35 56"));
        REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::Unreadable);
        CHECK_FALSE(fixture.orderUnknown());
    }
}

TEST_CASE("A coordinate or CS edit re-emits what the row's coordinate amounts to",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // Both roles are read through data(), so a delegate only re-reads them when
    // the proxy says to. Naming a system is the case that would be missed by
    // keying on the coordinate text alone: it turns unreadable text into a
    // coordinate without touching a character of it.
    FixFixture fixture;
    fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:32613"));

    QSignalSpy proxySpy(fixture.diagnostics, &QAbstractItemModel::dataChanged);

    REQUIRE(fixture.source->setCoordinateText(0, QStringLiteral("478000, 4430000, 1655m"),
                                              cwUnits::Metric) == QString());
    QList<int> seen = rolesSeen(proxySpy);
    CHECK(seen.contains(cwFixStationDiagnosticsModel::CoordinateErrorRole));
    CHECK(seen.contains(cwFixStationDiagnosticsModel::CoordinateOrderUnknownRole));

    proxySpy.clear();
    fixture.edit(cwFixStationModel::InputCSRole, QStringLiteral("EPSG:4326"));
    seen = rolesSeen(proxySpy);
    CHECK(seen.contains(cwFixStationDiagnosticsModel::CoordinateErrorRole));
    CHECK(seen.contains(cwFixStationDiagnosticsModel::CoordinateOrderUnknownRole));

    // A variance edit moves neither, so the delegates are not woken for it.
    proxySpy.clear();
    fixture.edit(cwFixStationModel::HorizontalVarianceRole, 2.5);
    seen = rolesSeen(proxySpy);
    CHECK_FALSE(seen.contains(cwFixStationDiagnosticsModel::CoordinateErrorRole));
    CHECK_FALSE(seen.contains(cwFixStationDiagnosticsModel::CoordinateOrderUnknownRole));
}

TEST_CASE("A text edit that moves no component still re-emits the domain verdict",
          "[FixStation][cwFixStationDiagnosticsModel]") {
    // The domain verdict defers on any state but Valid, so it moves with the
    // state, and the state comes from the text. Keying the domain roles on the
    // components alone misses the case where the text changes what the row *is*
    // without changing a number: an unreadable row already reports three zeros,
    // so becoming a coordinate at the origin moves nothing.
    //
    // A southern-hemisphere zone, where the origin really is out of domain —
    // in a northern one the row would end up valid *and* unflagged, and the
    // missing emission would have nothing to show for it.
    const QString cs = QStringLiteral("EPSG:28355");
    REQUIRE_FALSE(cwCoordinateTransform::domainCheck(cs, cwGeoPoint(0.0, 0.0, 0.0))
                      .northingValid);

    FixFixture fixture;
    fixture.edit(cwFixStationModel::InputCSRole, cs);
    fixture.setStoredCoordinate(QStringLiteral("N 46 07 16 W 115 35 56"));
    REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::Unreadable);
    REQUIRE_FALSE(fixture.northingFlagged());

    QSignalSpy proxySpy(fixture.diagnostics, &QAbstractItemModel::dataChanged);

    REQUIRE(fixture.source->setCoordinateText(0, QStringLiteral("0, 0, 0m"),
                                              cwUnits::Metric) == QString());
    REQUIRE(fixture.source->fixStationAt(0).state() == cwFixStation::Valid);

    // The verdict really did move, so a delegate that isn't told is showing a
    // stale one.
    CHECK(fixture.northingFlagged());
    const QList<int> seen = rolesSeen(proxySpy);
    CHECK(seen.contains(cwFixStationDiagnosticsModel::DomainErrorRole));
    CHECK(seen.contains(cwFixStationDiagnosticsModel::EastingDomainErrorRole));
    CHECK(seen.contains(cwFixStationDiagnosticsModel::NorthingDomainErrorRole));
}
