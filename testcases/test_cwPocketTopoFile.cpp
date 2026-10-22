/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwPocketTopoFile.h"

// Test helpers
#include "PocketTopoFixtureWriter.h"

// Qt
#include <QByteArray>
#include <QDate>
#include <QDir>
#include <QString>
#include <QTemporaryDir>

using cwPocketTopoFile::Document;
using cwPocketTopoFile::Reference;
using cwPocketTopoFile::Shot;
using cwPocketTopoFile::StationId;
using cwPocketTopoFile::Trip;
using PocketTopoFixtureWriter::Fixture;

namespace {

// 2024-02-29 23:59:59, the first trip of the Go project's mixed.top.
constexpr qint64 kLeapDayTicks = 638448479990000000LL;
// 2026-02-05 00:00.
constexpr qint64 kFebFifthTicks = 639058464000000000LL;
constexpr qint16 kManualDeclinationUnits = 1234;
constexpr double kManualDeclinationDegrees = 1234 * 360.0 / 65536.0;
constexpr quint16 kWestUnits = 0xC000;
constexpr qint16 kDownUnits = -16384;
constexpr qint16 kUpUnits = 16384;
constexpr int kLongCommentLength = 200;

//! Little-endian bytes laid down field by field, independent of the fixture
//! writer, so the reader is checked against the spec and not the writer.
class Bytes
{
public:
    Bytes& raw(const QByteArray& bytes) { m_bytes.append(bytes); return *this; }
    Bytes& u8(quint8 value) { m_bytes.append(char(value)); return *this; }
    Bytes& i16(qint16 value) { return little(quint64(quint16(value)), 2); }
    Bytes& u16(quint16 value) { return little(value, 2); }
    Bytes& i32(qint32 value) { return little(quint64(quint32(value)), 4); }
    Bytes& u32(quint32 value) { return little(value, 4); }
    Bytes& i64(qint64 value) { return little(quint64(value), 8); }

    //! A .NET string: 7-bit length prefix, then the UTF-8 bytes.
    Bytes& str(const QByteArray& utf8)
    {
        quint32 length = quint32(utf8.size());
        while (length >= 0x80) {
            u8(quint8(length | 0x80));
            length >>= 7;
        }
        u8(quint8(length));
        return raw(utf8);
    }

    Bytes& header(quint8 version = 3) { return raw("Top").u8(version); }

    Bytes& trip(qint64 ticks, const QByteArray& comment, qint16 declination)
    {
        return i64(ticks).str(comment).i16(declination);
    }

    Bytes& shot(quint32 from, quint32 to, qint32 distance, quint16 azimuth,
                qint16 inclination, quint8 flags, quint8 roll, qint16 tripIndex)
    {
        return u32(from).u32(to).i32(distance).u16(azimuth).i16(inclination)
            .u8(flags).u8(roll).i16(tripIndex);
    }

    Bytes& reference(quint32 station, qint64 east, qint64 north, qint32 altitude,
                     const QByteArray& comment)
    {
        return u32(station).i64(east).i64(north).i32(altitude).str(comment);
    }

    Bytes& mapping(qint32 x = 0, qint32 y = 0, qint32 scale = 500)
    {
        return i32(x).i32(y).i32(scale);
    }

    //! An overview mapping and two empty drawings.
    Bytes& emptyDrawings() { return mapping().mapping().u8(0).mapping().u8(0); }

    QByteArray bytes() const { return m_bytes; }

private:
    Bytes& little(quint64 value, int size)
    {
        for (int i = 0; i < size; ++i) {
            m_bytes.append(char((value >> (8 * i)) & 0xFF));
        }
        return *this;
    }

    QByteArray m_bytes;
};

constexpr quint32 kRaw1_0 = 0x00010000u;
constexpr quint32 kRaw1_1 = 0x00010001u;
constexpr quint32 kRaw1_2 = 0x00010002u;
constexpr quint32 kRawPlain7 = 0x80000008u;
constexpr quint32 kRawUndefined = 0x80000000u;

Shot makeShot(StationId from, StationId to, qint32 distance, quint16 azimuth,
              qint16 inclination, qint16 tripIndex)
{
    Shot shot;
    shot.from = from;
    shot.to = to;
    shot.distance = distance;
    shot.azimuth = azimuth;
    shot.inclination = inclination;
    shot.tripIndex = tripIndex;
    return shot;
}

//! A fixture with every record type and the edge cases the reader must keep.
Fixture everyRecordFixture()
{
    using namespace PocketTopoFixtureWriter;

    Fixture fixture;
    Document& document = fixture.document;

    Trip manual;
    manual.ticks = kLeapDayTicks;
    manual.comment = QStringLiteral("Za\u017C\u00F3\u0142\u0107 \"quote\" \\path\r\nnext\nline");
    manual.declination = kManualDeclinationUnits;

    Trip autoTrip;
    autoTrip.ticks = 0;
    autoTrip.declination = Trip::kAutoDeclination;

    Trip longComment;
    longComment.ticks = kFebFifthTicks;
    longComment.comment = QString(kLongCommentLength, QLatin1Char('a'));
    longComment.declination = -kManualDeclinationUnits;

    document.trips = {manual, autoTrip, longComment};

    const StationId s1_0 = StationId::dotted(1, 0);
    const StationId s1_1 = StationId::dotted(1, 1);
    const StationId s1_2 = StationId::dotted(1, 2);
    const StationId plain7 = StationId::plain(7);

    Shot leadingZero = makeShot(s1_0, StationId(), 0, 0, 0, Shot::kNoTrip);
    Shot leg = makeShot(s1_0, s1_1, 3380, kWestUnits, -275, 0);
    Shot reversed = makeShot(s1_1, s1_0, 3380, 0x4000, 275, 0);
    reversed.flags = Shot::HasComment;
    reversed.comment = QStringLiteral("reverse");
    Shot splay = makeShot(s1_1, StationId(), 1540, 0xEDB4, 1076, 1);
    splay.flags = Shot::Flipped;
    splay.roll = 128;
    Shot outOfRange = makeShot(s1_1, s1_2, 3060, 0xD650, -762, 5);
    Shot blankDistance = makeShot(s1_2, plain7, Shot::kBlankDistance, 0, 0, 2);
    Shot emptyComment = makeShot(plain7, StationId::plain(0), 100, 0xFFFF, kDownUnits, 2);
    emptyComment.flags = Shot::HasComment | Shot::Flipped;

    document.shots = {leadingZero, leg, reversed, splay, outOfRange, blankDistance, emptyComment};

    Reference fixed;
    fixed.station = s1_0;
    fixed.east = 500123456;
    fixed.north = 4500321000LL;
    fixed.altitude = 812345;
    fixed.comment = QStringLiteral("entrance");

    Reference blank;
    blank.station = plain7;
    blank.east = Reference::kBlankCoordinate;
    blank.north = Reference::kBlankCoordinate;
    blank.altitude = Reference::kBlankAltitude;

    document.references = {fixed, blank};

    Polygon stroke;
    stroke.points = {Point{0, 0}, Point{1000, -2000}, Point{1500, 250}};
    stroke.color = 4;
    Polygon empty;
    Polygon onePoint;
    onePoint.points = {Point{12, 34}};
    onePoint.color = 9;
    XSection section;
    section.position = Point{-500, 700};
    section.station = s1_1;
    section.direction = -1;
    XSection vertical;
    vertical.position = Point{300, 300};
    vertical.station = s1_2;
    vertical.direction = 0x4000;

    fixture.overview = Mapping{10, -20, 2000};
    fixture.plan.mapping = Mapping{1, 2, 100};
    fixture.plan.elements = {stroke, section, empty, onePoint};
    fixture.side.mapping = Mapping{3, 4, 50000};
    fixture.side.elements = {stroke, vertical};

    return fixture;
}

} // namespace

TEST_CASE("cwPocketTopoFile packs station ids as the spec does", "[PocketTopo]")
{
    const StationId dotted = StationId::dotted(1, 5);
    CHECK(dotted.raw() == 0x00010005u);
    CHECK(dotted.isDefined());
    CHECK_FALSE(dotted.isPlain());
    CHECK(dotted.name() == QStringLiteral("1.5"));

    const StationId seven = StationId::plain(7);
    CHECK(seven.raw() == kRawPlain7);
    CHECK(seven.isDefined());
    CHECK(seven.isPlain());
    CHECK(seven.name() == QStringLiteral("7"));

    const StationId plainZero = StationId::plain(0);
    const StationId dottedZero = StationId::dotted(0, 0);
    CHECK(plainZero.raw() == 0x80000001u);
    CHECK(plainZero.name() == QStringLiteral("0"));
    CHECK(dottedZero.raw() == 0u);
    CHECK(dottedZero.name() == QStringLiteral("0.0"));
    CHECK(plainZero != dottedZero);

    CHECK(StationId::dotted(32767, 65535).name() == QStringLiteral("32767.65535"));
    CHECK(StationId::plain(1048319).name() == QStringLiteral("1048319"));

    const StationId undefined;
    CHECK(undefined.raw() == kRawUndefined);
    CHECK_FALSE(undefined.isDefined());
    CHECK(undefined.name().isEmpty());

    SECTION("fromRaw decodes every range")
    {
        CHECK(StationId::fromRaw(0x00010005u) == dotted);
        CHECK(StationId::fromRaw(kRawPlain7) == seven);
        CHECK(StationId::fromRaw(kRawPlain7).name() == QStringLiteral("7"));
        CHECK_FALSE(StationId::fromRaw(kRawUndefined).isDefined());

        // Raw 0x800FFF01 to 0x80100000 are reserved and display blank.
        CHECK(StationId::fromRaw(0x800FFF01u).name().isEmpty());
        CHECK(StationId::fromRaw(0x80100000u).name().isEmpty());

        // Above that, raw values alias major.minor ids.
        CHECK(StationId::fromRaw(0x80100001u).name() == QStringLiteral("0.0"));
        CHECK(StationId::fromRaw(0x80100001u + 0x00010005u).name() == QStringLiteral("1.5"));
    }
}

TEST_CASE("PocketTopoFixtureWriter converts dates and angles to file units", "[PocketTopo]")
{
    CHECK(PocketTopoFixtureWriter::ticks(QDate(2026, 2, 5)) == kFebFifthTicks);
    CHECK(PocketTopoFixtureWriter::ticks(QDate(1, 1, 1)) == 0);
    CHECK(PocketTopoFixtureWriter::azimuthUnits(270.0) == kWestUnits);
    CHECK(PocketTopoFixtureWriter::azimuthUnits(90.0) == 0x4000);
    CHECK(PocketTopoFixtureWriter::azimuthUnits(360.0) == 0);
    CHECK(PocketTopoFixtureWriter::angleUnits(-90.0) == kDownUnits);
    CHECK(PocketTopoFixtureWriter::angleUnits(90.0) == kUpUnits);
    CHECK(PocketTopoFixtureWriter::angleUnits(6.78) == kManualDeclinationUnits);
}

TEST_CASE("PocketTopoFixtureWriter writes byte-exact version 3 files", "[PocketTopo]")
{
    Fixture fixture;
    Trip trip;
    trip.ticks = kFebFifthTicks;
    trip.comment = QStringLiteral("Robin survey");
    fixture.document.trips = {trip};
    fixture.document.shots = {makeShot(StationId::dotted(1, 0), StationId::dotted(1, 1),
                                       3380, kWestUnits, -182, 0)};

    const QByteArray expected = QByteArray::fromHex(
        "546f7003"                  // "Top" v3
        "01000000"                  // tripCount
        "00406d814964de08"          // ticks 2026-02-05
        "0c" "526f62696e20737572766579" // "Robin survey"
        "0000"                      // declination
        "01000000"                  // shotCount
        "00000100"                  // from 1.0
        "01000100"                  // to 1.1
        "340d0000"                  // 3380 mm
        "00c0"                      // azimuth 270
        "4aff"                      // inclination -182 units
        "00" "00"                   // flags, roll
        "0000"                      // trip 0
        "00000000"                  // refCount
        "00000000" "00000000" "f4010000" // overview mapping
        "00000000" "00000000" "f4010000" "00" // plan
        "00000000" "00000000" "f4010000" "00" // side
        "00000000");                // trailer

    CHECK(PocketTopoFixtureWriter::write(fixture).toHex() == expected.toHex());

    SECTION("Trailer::None ends at the side drawing")
    {
        fixture.trailer = PocketTopoFixtureWriter::Trailer::None;
        CHECK(PocketTopoFixtureWriter::write(fixture).toHex()
              == expected.left(expected.size() - 4).toHex());
    }

    SECTION("A shot's comment is written only when its flag bit is set")
    {
        fixture.document.shots[0].comment = QStringLiteral("dropped");
        CHECK(PocketTopoFixtureWriter::write(fixture).toHex() == expected.toHex());

        fixture.document.shots[0].flags = Shot::HasComment;
        const QByteArray withComment = PocketTopoFixtureWriter::write(fixture);
        CHECK(withComment.size() == expected.size() + 1 + 7);
        CHECK(withComment.contains(QByteArray("\x07" "dropped", 8)));
    }

    SECTION("Drawing elements are laid out as id and payload")
    {
        using namespace PocketTopoFixtureWriter;
        Polygon polygon;
        polygon.points = {Point{1, -1}};
        polygon.color = 5;
        XSection section;
        section.position = Point{2, 3};
        section.station = StationId::dotted(1, 1);
        section.direction = 0x4000;
        fixture.plan.elements = {polygon, section};

        const QByteArray elements = QByteArray::fromHex(
            "01" "01000000" "01000000" "ffffffff" "05"       // polygon
            "03" "02000000" "03000000" "01000100" "00400000" // xsection
            "00");                                           // end
        const QByteArray written = PocketTopoFixtureWriter::write(fixture);
        const int planElementsAt = expected.size() - 4 - 13 - 1;
        CHECK(written.mid(planElementsAt, elements.size()).toHex() == elements.toHex());
        CHECK(written.size() == expected.size() + elements.size() - 1);
    }
}

TEST_CASE("cwPocketTopoFile reads hand-built records field by field", "[PocketTopo]")
{
    const QByteArray bytes = Bytes()
        .header()
        .i32(2)
        .trip(kLeapDayTicks, "Robin survey\rBecka", kManualDeclinationUnits)
        .trip(0, "", Trip::kAutoDeclination)
        .i32(3)
        .shot(kRaw1_0, kRaw1_1, 3380, kWestUnits, kDownUnits, 0, 64, 0)
        .shot(kRaw1_1, kRawUndefined, 1540, 0xFFFF, kUpUnits, Shot::Flipped, 0, 1)
        .shot(kRaw1_1, kRawPlain7, Shot::kBlankDistance, 0x4000, -1, 0, 0, Shot::kNoTrip)
        .i32(1)
        .reference(kRaw1_0, 500123456, 4500321000LL, 812345, "base")
        .emptyDrawings()
        .i32(0)
        .bytes();

    const auto result = cwPocketTopoFile::read(bytes);
    REQUIRE_FALSE(result.hasError());
    const Document document = result.value();

    REQUIRE(document.trips.size() == 2);
    const Trip& first = document.trips.at(0);
    CHECK(first.ticks == kLeapDayTicks);
    CHECK(first.date() == QDate(2024, 2, 29));
    CHECK(first.comment == QStringLiteral("Robin survey\rBecka"));
    CHECK(first.declination == kManualDeclinationUnits);
    CHECK_FALSE(first.isAutoDeclination());
    CHECK(first.declinationDegrees() == Catch::Approx(kManualDeclinationDegrees));

    const Trip& second = document.trips.at(1);
    CHECK(second.date() == QDate(1, 1, 1));
    CHECK(second.comment.isEmpty());
    CHECK(second.isAutoDeclination());
    CHECK(second.declinationDegrees() == 0.0);

    REQUIRE(document.shots.size() == 3);
    const Shot& leg = document.shots.at(0);
    CHECK(leg.from.name() == QStringLiteral("1.0"));
    CHECK(leg.to.name() == QStringLiteral("1.1"));
    CHECK_FALSE(leg.isSplay());
    REQUIRE(leg.distanceMeters().has_value());
    CHECK(*leg.distanceMeters() == Catch::Approx(3.380));
    CHECK(leg.azimuthDegrees() == Catch::Approx(270.0));
    CHECK(leg.inclinationDegrees() == Catch::Approx(-90.0));
    CHECK(leg.roll == 64);
    CHECK(leg.tripIndex == 0);

    const Shot& splay = document.shots.at(1);
    CHECK(splay.isSplay());
    CHECK(splay.isFlipped());
    CHECK_FALSE(splay.hasComment());
    CHECK(splay.azimuthDegrees() == Catch::Approx(65535 * 360.0 / 65536.0));
    CHECK(splay.inclinationDegrees() == Catch::Approx(90.0));
    CHECK(splay.tripIndex == 1);

    const Shot& blank = document.shots.at(2);
    CHECK(blank.to.name() == QStringLiteral("7"));
    CHECK(blank.distance == Shot::kBlankDistance);
    CHECK_FALSE(blank.distanceMeters().has_value());
    CHECK(blank.azimuthDegrees() == Catch::Approx(90.0));
    CHECK(blank.inclinationDegrees() == Catch::Approx(-360.0 / 65536.0));
    CHECK(blank.tripIndex == Shot::kNoTrip);

    REQUIRE(document.references.size() == 1);
    const Reference& reference = document.references.at(0);
    CHECK(reference.station.name() == QStringLiteral("1.0"));
    REQUIRE(reference.eastMeters().has_value());
    REQUIRE(reference.northMeters().has_value());
    REQUIRE(reference.altitudeMeters().has_value());
    CHECK(*reference.eastMeters() == Catch::Approx(500123.456));
    CHECK(*reference.northMeters() == Catch::Approx(4500321.000));
    CHECK(*reference.altitudeMeters() == Catch::Approx(812.345));
    CHECK(reference.comment == QStringLiteral("base"));

    CHECK(document.planElementCount == 0);
    CHECK(document.sideElementCount == 0);
}

TEST_CASE("cwPocketTopoFile round-trips every record type through the fixture writer", "[PocketTopo]")
{
    const Fixture fixture = everyRecordFixture();
    const QByteArray bytes = PocketTopoFixtureWriter::write(fixture);
    REQUIRE_FALSE(bytes.isEmpty());

    const auto result = cwPocketTopoFile::read(bytes);
    REQUIRE_FALSE(result.hasError());
    const Document document = result.value();

    CHECK(document.trips == fixture.document.trips);
    CHECK(document.shots == fixture.document.shots);
    CHECK(document.references == fixture.document.references);
    CHECK(document.planElementCount == 4);
    CHECK(document.sideElementCount == 2);

    SECTION("Through a file on disk")
    {
        QTemporaryDir dir;
        REQUIRE(dir.isValid());
        const QString path = QDir(dir.path()).filePath(QStringLiteral("everyRecord.top"));
        REQUIRE(PocketTopoFixtureWriter::writeFile(fixture, path));

        const auto fromFile = cwPocketTopoFile::readFile(path);
        REQUIRE_FALSE(fromFile.hasError());
        CHECK(fromFile.value() == document);
    }
}

TEST_CASE("cwPocketTopoFile reads the Auto and blank sentinels as missing values", "[PocketTopo]")
{
    const Fixture fixture = everyRecordFixture();
    const auto result = cwPocketTopoFile::read(PocketTopoFixtureWriter::write(fixture));
    REQUIRE_FALSE(result.hasError());
    const Document document = result.value();
    REQUIRE(document.trips.size() == 3);
    REQUIRE(document.shots.size() == 7);
    REQUIRE(document.references.size() == 2);

    CHECK(document.trips.at(1).declination == Trip::kAutoDeclination);
    CHECK(document.trips.at(1).isAutoDeclination());
    CHECK(document.trips.at(1).declinationDegrees() == 0.0);
    CHECK(document.trips.at(2).declinationDegrees() == Catch::Approx(-kManualDeclinationDegrees));

    CHECK_FALSE(document.shots.at(5).distanceMeters().has_value());
    REQUIRE(document.shots.at(0).distanceMeters().has_value());
    CHECK(*document.shots.at(0).distanceMeters() == 0.0);

    const Reference& blank = document.references.at(1);
    CHECK(blank.station.name() == QStringLiteral("7"));
    CHECK_FALSE(blank.eastMeters().has_value());
    CHECK_FALSE(blank.northMeters().has_value());
    CHECK_FALSE(blank.altitudeMeters().has_value());
    CHECK(blank.comment.isEmpty());
}

TEST_CASE("cwPocketTopoFile reads a shot comment only when flag bit 1 is set", "[PocketTopo]")
{
    // The flipped bit (0x01) carries no comment; a bit-1 shot does, empty or
    // not. Reading a comment for the wrong shot would desynchronize the rest.
    const QByteArray bytes = Bytes()
        .header()
        .i32(0)
        .i32(4)
        .shot(kRaw1_0, kRaw1_1, 1000, 0, 0, Shot::Flipped, 0, Shot::kNoTrip)
        .shot(kRaw1_1, kRaw1_2, 2000, 0, 0, Shot::HasComment, 0, Shot::kNoTrip).str("wet")
        .shot(kRaw1_2, kRawPlain7, 3000, 0, 0, Shot::HasComment | Shot::Flipped, 0, Shot::kNoTrip).str("")
        .shot(kRawPlain7, kRawUndefined, 4000, 0, 0, 0, 0, Shot::kNoTrip)
        .i32(0)
        .emptyDrawings()
        .bytes();

    const auto result = cwPocketTopoFile::read(bytes);
    REQUIRE_FALSE(result.hasError());
    const QList<Shot> shots = result.value().shots;
    REQUIRE(shots.size() == 4);

    CHECK(shots.at(0).isFlipped());
    CHECK_FALSE(shots.at(0).hasComment());
    CHECK(shots.at(0).comment.isEmpty());

    CHECK(shots.at(1).hasComment());
    CHECK(shots.at(1).comment == QStringLiteral("wet"));
    CHECK(*shots.at(1).distanceMeters() == Catch::Approx(2.0));

    CHECK(shots.at(2).hasComment());
    CHECK(shots.at(2).isFlipped());
    CHECK(shots.at(2).comment.isEmpty());
    CHECK(*shots.at(2).distanceMeters() == Catch::Approx(3.0));

    CHECK(shots.at(3).from.name() == QStringLiteral("7"));
    CHECK(shots.at(3).isSplay());
    CHECK(*shots.at(3).distanceMeters() == Catch::Approx(4.0));
}

TEST_CASE("cwPocketTopoFile keeps reversed and untripped shots as recorded", "[PocketTopo]")
{
    const Fixture fixture = everyRecordFixture();
    const auto result = cwPocketTopoFile::read(PocketTopoFixtureWriter::write(fixture));
    REQUIRE_FALSE(result.hasError());
    const QList<Shot> shots = result.value().shots;
    REQUIRE(shots.size() == 7);

    // The leading zero-length untripped splay stays; dropping it is the
    // translator's job.
    CHECK(shots.at(0).from.name() == QStringLiteral("1.0"));
    CHECK(shots.at(0).isSplay());
    CHECK(shots.at(0).tripIndex == Shot::kNoTrip);

    // A reverse reading has no flag: its endpoints are swapped and its
    // angles are as measured from its own from-station.
    const Shot& forward = shots.at(1);
    const Shot& reversed = shots.at(2);
    CHECK(forward.from.name() == QStringLiteral("1.0"));
    CHECK(forward.to.name() == QStringLiteral("1.1"));
    CHECK(reversed.from.name() == QStringLiteral("1.1"));
    CHECK(reversed.to.name() == QStringLiteral("1.0"));
    CHECK(forward.azimuthDegrees() == Catch::Approx(270.0));
    CHECK(reversed.azimuthDegrees() == Catch::Approx(90.0));
    CHECK(forward.inclinationDegrees() == Catch::Approx(-275 * 360.0 / 65536.0));
    CHECK(reversed.inclinationDegrees() == Catch::Approx(275 * 360.0 / 65536.0));
    CHECK(reversed.comment == QStringLiteral("reverse"));

    // An index past the trip table is kept as written.
    CHECK(shots.at(4).tripIndex == 5);
    CHECK(shots.at(4).to.name() == QStringLiteral("1.2"));

    CHECK(shots.at(6).from.name() == QStringLiteral("7"));
    CHECK(shots.at(6).to.name() == QStringLiteral("0"));
}

TEST_CASE("cwPocketTopoFile skips the drawings and whatever trails them", "[PocketTopo]")
{
    const auto withTrailer = [](const QByteArray& trailer) {
        return Bytes()
            .header()
            .i32(1)
            .trip(kFebFifthTicks, "t", 0)
            .i32(1)
            .shot(kRaw1_0, kRaw1_1, 1000, 0, 0, 0, 0, 0)
            .i32(1)
            .reference(kRaw1_0, 1000, 2000, 3000, "")
            .mapping(5, 6, 700)
            // Plan: a three-point polygon, an empty one, a cross-section.
            .mapping(1, 2, 100)
            .u8(1).i32(3).i32(0).i32(0).i32(10).i32(-10).i32(20).i32(5).u8(4)
            .u8(1).i32(0).u8(2)
            .u8(3).i32(-500).i32(700).u32(kRaw1_1).i32(-1)
            .u8(0)
            // Side: one cross-section.
            .mapping(3, 4, 200)
            .u8(3).i32(1).i32(1).u32(kRaw1_0).i32(0x4000)
            .u8(0)
            .raw(trailer)
            .bytes();
    };

    const QByteArray trailers[] = {
        QByteArray(),
        QByteArray(4, '\0'),
        QByteArray("trailing garbage"),
    };

    for (const QByteArray& trailer : trailers) {
        INFO("trailer: " << trailer.toHex().toStdString());
        const auto result = cwPocketTopoFile::read(withTrailer(trailer));
        REQUIRE_FALSE(result.hasError());
        const Document document = result.value();
        REQUIRE(document.trips.size() == 1);
        REQUIRE(document.shots.size() == 1);
        REQUIRE(document.references.size() == 1);
        CHECK(document.trips.at(0).comment == QStringLiteral("t"));
        CHECK(document.shots.at(0).to.name() == QStringLiteral("1.1"));
        CHECK(*document.references.at(0).altitudeMeters() == Catch::Approx(3.0));
        CHECK(document.planElementCount == 3);
        CHECK(document.sideElementCount == 1);
    }
}

TEST_CASE("cwPocketTopoFile decodes 7-bit string lengths and UTF-8", "[PocketTopo]")
{
    const QByteArray longComment(kLongCommentLength, 'a');
    const QByteArray bytes = Bytes()
        .header()
        .i32(4)
        .trip(0, longComment, 0)
        // A non-minimal zero length, which PocketTopo accepts.
        .i64(0).u8(0x80).u8(0x00).i16(0)
        .trip(0, "Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87", 0)
        // Invalid UTF-8 becomes U+FFFD.
        .trip(0, "ab\xFF" "cd", 0)
        .i32(0)
        .i32(0)
        .emptyDrawings()
        .bytes();

    // A 200-byte string takes a two-byte length.
    REQUIRE(bytes.mid(4 + 4 + 8, 2) == QByteArray::fromHex("c801"));

    const auto result = cwPocketTopoFile::read(bytes);
    REQUIRE_FALSE(result.hasError());
    const QList<Trip> trips = result.value().trips;
    REQUIRE(trips.size() == 4);
    CHECK(trips.at(0).comment == QString::fromLatin1(longComment));
    CHECK(trips.at(1).comment.isEmpty());
    CHECK(trips.at(2).comment == QStringLiteral("Za\u017C\u00F3\u0142\u0107"));
    CHECK(trips.at(3).comment == QStringLiteral("ab\uFFFDcd"));
}

TEST_CASE("cwPocketTopoFile rejects files it cannot read", "[PocketTopo]")
{
    const auto oneShotFile = [](const Bytes& prefix) {
        return Bytes(prefix)
            .i32(0)
            .i32(1)
            .shot(kRaw1_0, kRaw1_1, 1000, 0, 0, 0, 0, Shot::kNoTrip)
            .i32(0)
            .emptyDrawings()
            .bytes();
    };

    const QByteArray valid = oneShotFile(Bytes().header());
    REQUIRE_FALSE(cwPocketTopoFile::read(valid).hasError());
    CHECK(cwPocketTopoFile::read(valid).value().shots.size() == 1);

    SECTION("Empty input")
    {
        CHECK(cwPocketTopoFile::read(QByteArray()).hasError());
    }

    SECTION("Wrong magic")
    {
        CHECK(cwPocketTopoFile::read(oneShotFile(Bytes().raw("Tip").u8(3))).hasError());
    }

    SECTION("Unsupported version")
    {
        CHECK(cwPocketTopoFile::read(oneShotFile(Bytes().header(2))).hasError());
    }

    SECTION("Ends inside a shot")
    {
        const QByteArray cut = valid.left(4 + 4 + 4 + 10);
        CHECK(cwPocketTopoFile::read(cut).hasError());
    }

    SECTION("Ends inside a shot comment")
    {
        const QByteArray bytes = Bytes().header().i32(0).i32(1)
            .shot(kRaw1_0, kRaw1_1, 1000, 0, 0, Shot::HasComment, 0, Shot::kNoTrip)
            .u8(10).raw("abc")
            .bytes();
        CHECK(cwPocketTopoFile::read(bytes).hasError());
    }

    SECTION("Ends before the reference table")
    {
        const QByteArray bytes = Bytes().header().i32(0).i32(0).bytes();
        CHECK(cwPocketTopoFile::read(bytes).hasError());
    }

    SECTION("A trip date before 0001-01-01")
    {
        const QByteArray bytes = Bytes().header().i32(1).trip(-1, "", 0)
            .i32(0).i32(0).emptyDrawings().bytes();
        CHECK(cwPocketTopoFile::read(bytes).hasError());
    }

    SECTION("A string length longer than five bytes")
    {
        const QByteArray bytes = Bytes().header().i32(1)
            // Without the five-byte guard this decodes as an empty comment.
            .i64(0).u8(0x80).u8(0x80).u8(0x80).u8(0x80).u8(0x10).i16(0)
            .i32(0).i32(0).emptyDrawings()
            .bytes();
        CHECK(cwPocketTopoFile::read(bytes).hasError());
    }

    SECTION("A negative table count")
    {
        const QByteArray bytes = Bytes().header().i32(-1).bytes();
        CHECK(cwPocketTopoFile::read(bytes).hasError());
    }

    SECTION("A file that does not exist")
    {
        QTemporaryDir dir;
        REQUIRE(dir.isValid());
        CHECK(cwPocketTopoFile::readFile(QDir(dir.path()).filePath(QStringLiteral("missing.top"))).hasError());
    }
}
