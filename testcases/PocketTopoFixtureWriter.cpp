/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "PocketTopoFixtureWriter.h"

//Qt includes
#include <QFile>

//Std includes
#include <cmath>

namespace PocketTopoFixtureWriter {

namespace {

constexpr double kAngleUnitsPerCircle = 65536.0;
constexpr double kDegreesPerCircle = 360.0;
constexpr qint64 kTicksPerDay = 864000000000LL;
constexpr quint8 kStringLengthMoreBit = 0x80;
constexpr int kStringLengthBitsPerByte = 7;

constexpr int kBitsPerByte = 8;
constexpr quint64 kByteMask = 0xFF;

constexpr quint8 kElementEnd = 0;
constexpr quint8 kElementPolygon = 1;
constexpr quint8 kElementXSection = 3;
constexpr qint32 kTrailer = 0;

class Writer
{
public:
    void raw(const QByteArray& bytes) { m_bytes.append(bytes); }
    void u8(quint8 value) { m_bytes.append(char(value)); }
    void i16(qint16 value) { little(quint16(value), sizeof(value)); }
    void u16(quint16 value) { little(value, sizeof(value)); }
    void i32(qint32 value) { little(quint32(value), sizeof(value)); }
    void u32(quint32 value) { little(value, sizeof(value)); }
    void i64(qint64 value) { little(quint64(value), sizeof(value)); }

    void string(const QString& text)
    {
        const QByteArray utf8 = text.toUtf8();
        quint32 length = quint32(utf8.size());
        while (length >= kStringLengthMoreBit) {
            u8(quint8(length | kStringLengthMoreBit));
            length >>= kStringLengthBitsPerByte;
        }
        u8(quint8(length));
        raw(utf8);
    }

    void point(const Point& point)
    {
        i32(point.x);
        i32(point.y);
    }

    void mapping(const Mapping& mapping)
    {
        i32(mapping.x);
        i32(mapping.y);
        i32(mapping.scale);
    }

    void element(const Polygon& polygon)
    {
        u8(kElementPolygon);
        i32(qint32(polygon.points.size()));
        for (const Point& p : polygon.points) {
            point(p);
        }
        u8(polygon.color);
    }

    void element(const XSection& section)
    {
        u8(kElementXSection);
        point(section.position);
        u32(section.station.raw());
        i32(section.direction);
    }

    void drawing(const Drawing& drawing)
    {
        mapping(drawing.mapping);
        for (const Element& e : drawing.elements) {
            std::visit([this](const auto& value) { element(value); }, e);
        }
        u8(kElementEnd);
    }

    QByteArray bytes() const { return m_bytes; }

private:
    void little(quint64 value, qsizetype size)
    {
        for (qsizetype i = 0; i < size; ++i) {
            m_bytes.append(char((value >> (kBitsPerByte * i)) & kByteMask));
        }
    }

    QByteArray m_bytes;
};

} // namespace

QByteArray write(const Fixture& fixture)
{
    using namespace cwPocketTopoFile;

    Writer writer;
    writer.raw(QByteArrayLiteral("Top"));
    writer.u8(fixture.version);

    const Document& document = fixture.document;
    writer.i32(qint32(document.trips.size()));
    for (const Trip& trip : document.trips) {
        writer.i64(trip.ticks);
        writer.string(trip.comment);
        writer.i16(trip.declination);
    }

    writer.i32(qint32(document.shots.size()));
    for (const Shot& shot : document.shots) {
        writer.u32(shot.from.raw());
        writer.u32(shot.to.raw());
        writer.i32(shot.distance);
        writer.u16(shot.azimuth);
        writer.i16(shot.inclination);
        writer.u8(shot.flags);
        writer.u8(shot.roll);
        writer.i16(shot.tripIndex);
        if (shot.hasComment()) {
            writer.string(shot.comment);
        }
    }

    writer.i32(qint32(document.references.size()));
    for (const Reference& reference : document.references) {
        writer.u32(reference.station.raw());
        writer.i64(reference.east);
        writer.i64(reference.north);
        writer.i32(reference.altitude);
        writer.string(reference.comment);
    }

    writer.mapping(fixture.overview);
    writer.drawing(fixture.plan);
    writer.drawing(fixture.side);

    if (fixture.trailer == Trailer::Zero) {
        writer.i32(kTrailer);
    }

    return writer.bytes();
}

bool writeFile(const Fixture& fixture, const QString& path)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QByteArray bytes = write(fixture);
        return file.write(bytes) == bytes.size();
    }
    return false;
}

qint64 ticks(const QDate& date)
{
    return QDate(1, 1, 1).daysTo(date) * kTicksPerDay;
}

quint16 azimuthUnits(double degrees)
{
    return quint16(qint64(std::lround(degrees * kAngleUnitsPerCircle / kDegreesPerCircle)));
}

qint16 angleUnits(double degrees)
{
    return qint16(std::lround(degrees * kAngleUnitsPerCircle / kDegreesPerCircle));
}

} // namespace PocketTopoFixtureWriter
