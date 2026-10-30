//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

//Our includes
#include "cwStationPositionLookup.h"
#include "cwStation.h"

//Qt includes
#include <QSet>
#include <QStringList>
#include <QVector3D>

namespace {

const QVector3D kP2Position(1.0f, 0.0f, 0.0f);
const QVector3D kLowerP2Position(0.0f, 30.48f, 0.0f);

}

TEST_CASE("cwStationPositionLookup keeps keys stored by key apart when they differ only in case",
          "[cwStationPositionLookup]") {
    cwStationPositionLookup lookup;
    lookup.setPositionAtKey(QStringLiteral("P2"), kP2Position);
    lookup.setPositionAtKey(QStringLiteral("p2"), kLowerP2Position);

    CHECK(lookup.positions().keys() == QStringList{QStringLiteral("P2"), QStringLiteral("p2")});
    CHECK(lookup.position(QStringLiteral("P2")) == kP2Position);
    CHECK(lookup.position(QStringLiteral("p2")) == kLowerP2Position);
    CHECK(lookup.hasPosition(QStringLiteral("P2")));
    CHECK(lookup.hasPosition(QStringLiteral("p2")));
    CHECK(lookup.key(QStringLiteral("P2")) == QStringLiteral("P2"));
    CHECK(lookup.key(QStringLiteral("p2")) == QStringLiteral("p2"));
}

TEST_CASE("cwStationPositionLookup by-name lookups try the exact name, then its canonical key",
          "[cwStationPositionLookup]") {
    cwStationPositionLookup lookup;
    lookup.setPositionAtKey(QStringLiteral("k1"), QVector3D(2.0f, 3.0f, 4.0f));
    lookup.setPositionAtKey(QStringLiteral("BB2"), QVector3D(5.0f, 0.0f, 0.0f));
    lookup.setPositionAtKey(QStringLiteral("Bb2"), QVector3D(6.0f, 0.0f, 0.0f));
    lookup.setPositionAtKey(QStringLiteral("Q1"), QVector3D(7.0f, 0.0f, 0.0f));
    REQUIRE(lookup.positions().size() == 4);

    SECTION("A name in another case finds the lowercase key") {
        CHECK(lookup.position(QStringLiteral("K1")) == QVector3D(2.0f, 3.0f, 4.0f));
        CHECK(lookup.hasPosition(QStringLiteral("K1")));
        CHECK(lookup.key(QStringLiteral("K1")) == QStringLiteral("k1"));
    }

    SECTION("A spelling matching neither member of a case pair finds nothing") {
        CHECK_FALSE(lookup.hasPosition(QStringLiteral("bB2")));
        CHECK(lookup.position(QStringLiteral("bB2")) == QVector3D());
        CHECK(lookup.key(QStringLiteral("bB2")).isEmpty());
        CHECK(lookup.position(QStringLiteral("BB2")) == QVector3D(5.0f, 0.0f, 0.0f));
        CHECK(lookup.position(QStringLiteral("Bb2")) == QVector3D(6.0f, 0.0f, 0.0f));
    }

    SECTION("A key that keeps its case answers only its own spelling") {
        CHECK(lookup.position(QStringLiteral("Q1")) == QVector3D(7.0f, 0.0f, 0.0f));
        CHECK(lookup.key(QStringLiteral("Q1")) == QStringLiteral("Q1"));
        CHECK_FALSE(lookup.hasPosition(QStringLiteral("q1")));
        CHECK(lookup.key(QStringLiteral("q1")).isEmpty());
    }
}

TEST_CASE("cwStationPositionLookup by-name inserts keep folding beside keys stored by key",
          "[cwStationPositionLookup]") {
    cwStationPositionLookup lookup;
    lookup.setPositionAtKey(QStringLiteral("P2"), kP2Position);
    lookup.setPosition(QStringLiteral("P2"), kLowerP2Position);

    CHECK(lookup.positions().keys() == QStringList{QStringLiteral("P2"), QStringLiteral("p2")});
    CHECK(lookup.position(QStringLiteral("P2")) == kP2Position);
    CHECK(lookup.position(QStringLiteral("p2")) == kLowerP2Position);
}

TEST_CASE("A scoped lookup keeps both members of a case pair",
          "[cwStationPositionLookup]") {
    cwStationPositionLookup lookup;
    lookup.setPositionAtKey(QStringLiteral("cave.P2"), kP2Position);
    lookup.setPositionAtKey(QStringLiteral("cave.p2"), kLowerP2Position);
    lookup.setPositionAtKey(QStringLiteral("cave.q1"), QVector3D(7.0f, 0.0f, 0.0f));
    lookup.setPositionAtKey(QStringLiteral("other.P2"), QVector3D(8.0f, 0.0f, 0.0f));

    const QString prefix = GENERATE(QStringLiteral("cave."), QStringLiteral("Cave."));
    INFO("prefix " << prefix.toStdString());
    const cwStationPositionLookup scoped = lookup.scoped(prefix);

    CHECK(scoped.positions().keys()
          == QStringList{QStringLiteral("P2"), QStringLiteral("p2"), QStringLiteral("q1")});
    CHECK(scoped.position(QStringLiteral("P2")) == kP2Position);
    CHECK(scoped.position(QStringLiteral("p2")) == kLowerP2Position);
    CHECK(scoped.position(QStringLiteral("Q1")) == QVector3D(7.0f, 0.0f, 0.0f));
}

TEST_CASE("cwStation::caseDistinctKey lowercases survey levels and keeps the station name",
          "[cwStationPositionLookup]") {
    CHECK(cwStation::caseDistinctKey({QStringLiteral("Cave"), QStringLiteral("Foo")},
                                     QStringLiteral("P2"))
          == QStringLiteral("cave.foo.P2"));
    CHECK(cwStation::caseDistinctKey({QStringLiteral("cave")}, QStringLiteral("p2"))
          == QStringLiteral("cave.p2"));
    CHECK(cwStation::caseDistinctKey({}, QStringLiteral("P2")) == QStringLiteral("P2"));
}

TEST_CASE("cwStation::keyIn answers the name when the keys hold it, otherwise its canonical key",
          "[cwStationPositionLookup]") {
    const QSet<QString> pair{QStringLiteral("P2"), QStringLiteral("p2")};
    CHECK(cwStation::keyIn(QStringLiteral("P2"), pair) == QStringLiteral("P2"));
    CHECK(cwStation::keyIn(QStringLiteral("p2"), pair) == QStringLiteral("p2"));
    CHECK(cwStation::keyIn(QStringLiteral("K1"), {QStringLiteral("k1")}) == QStringLiteral("k1"));
    CHECK(cwStation::keyIn(QStringLiteral("Z9"), {}) == QStringLiteral("z9"));
}

TEST_CASE("cwStation::spellingIn answers the exact spelling, otherwise the one case-insensitive match",
          "[cwStationPositionLookup]") {
    CHECK(cwStation::spellingIn(QStringLiteral("p2"), {QStringLiteral("P2")}) == QStringLiteral("P2"));
    CHECK(cwStation::spellingIn(QStringLiteral("P2"), {QStringLiteral("P2"), QStringLiteral("p2")})
          == QStringLiteral("P2"));
    CHECK(cwStation::spellingIn(QStringLiteral("p2"), {QStringLiteral("P2"), QStringLiteral("p2")})
          == QStringLiteral("p2"));

    // Several case-insensitive matches name no one spelling.
    CHECK(cwStation::spellingIn(QStringLiteral("bb2"), {QStringLiteral("BB2"), QStringLiteral("Bb2")})
          .isEmpty());
    CHECK(cwStation::spellingIn(QStringLiteral("x1"), {QStringLiteral("P2")}).isEmpty());
}
