/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPocketTopoFile.h"

//Qt includes
#include <QFile>

namespace cwPocketTopoFile {

namespace {

constexpr QByteArrayView kMagic("Top");
constexpr quint8 kSupportedVersion = 3;

constexpr double kAngleUnitsPerCircle = 65536.0;
constexpr double kDegreesPerCircle = 360.0;
constexpr double kMillimetersPerMeter = 1000.0;

constexpr qint64 kTicksPerDay = 864000000000LL;
//! .NET DateTime.MaxValue.Ticks.
constexpr qint64 kMaxTicks = 3155378975999999999LL;

constexpr quint32 kPlainOffset = 0x80000001u;
constexpr int kMajorShift = 16;
constexpr quint32 kMinorMask = 0xFFFFu;
//! PocketTopo 1.372 maps a negative raw id into its native value space by
//! adding this; native values below kReservedNativeMin are plain numbers
//! (offset by kNativePlainOffset), the 256 above are reserved.
constexpr qint64 kNativeShift = 2146435071LL;
constexpr qint64 kReservedNativeMin = -256;
constexpr qint64 kNativePlainOffset = 1048576LL;

constexpr int kStringLengthBitsPerByte = 7;
constexpr quint8 kStringLengthMoreBit = 0x80;
constexpr quint8 kStringLengthValueMask = 0x7F;
constexpr int kStringLengthLastShift = 28;
constexpr quint8 kStringLengthLastByteMax = 7;

// Record sizes with every string empty (a one-byte length).
constexpr int kEmptyStringSize = 1;
constexpr int kTripMinSize = sizeof(qint64) + kEmptyStringSize + sizeof(qint16);
constexpr int kShotMinSize = 3 * sizeof(quint32) + 2 * sizeof(quint16) + 2 * sizeof(quint8) + sizeof(qint16);
constexpr int kReferenceMinSize = sizeof(quint32) + 2 * sizeof(qint64) + sizeof(qint32) + kEmptyStringSize;
constexpr int kPointSize = 2 * sizeof(qint32);

constexpr int kBitsPerByte = 8;

constexpr quint8 kElementEnd = 0;
constexpr quint8 kElementPolygon = 1;
constexpr quint8 kElementXSection = 3;

double angleDegrees(double units)
{
    return units * kDegreesPerCircle / kAngleUnitsPerCircle;
}

template<typename T>
std::optional<double> meters(T millimeters, T blank)
{
    if (millimeters == blank) {
        return std::nullopt;
    }
    return millimeters / kMillimetersPerMeter;
}

//! The raw id in PocketTopo 1.372's native value space.
qint64 nativeValue(quint32 raw)
{
    const qint64 value = qint32(raw);
    if (value < 0) {
        return value + kNativeShift;
    }
    return value;
}

//! Little-endian reads over a byte array. The first failure sticks: later
//! reads return zero and only the first error message is kept.
class Cursor
{
public:
    explicit Cursor(const QByteArray& bytes) :
        m_bytes(bytes)
    {
    }

    bool hasError() const { return !m_error.isEmpty(); }
    QString error() const { return m_error; }
    qsizetype remaining() const { return m_bytes.size() - m_position; }

    void fail(const QString& message)
    {
        if (m_error.isEmpty()) {
            m_error = message;
        }
    }

    QByteArray bytes(qsizetype size)
    {
        if (hasError()) {
            return QByteArray();
        }
        if (size > remaining()) {
            fail(QStringLiteral("The PocketTopo file ends at byte %1, inside a %2.")
                     .arg(m_bytes.size())
                     .arg(m_context));
            return QByteArray();
        }
        const QByteArray result = m_bytes.mid(m_position, size);
        m_position += size;
        return result;
    }

    quint8 u8() { return quint8(little(sizeof(quint8))); }
    quint16 u16() { return quint16(little(sizeof(quint16))); }
    qint16 i16() { return qint16(quint16(little(sizeof(quint16)))); }
    quint32 u32() { return quint32(little(sizeof(quint32))); }
    qint32 i32() { return qint32(quint32(little(sizeof(qint32)))); }
    qint64 i64() { return qint64(little(sizeof(qint64))); }

    //! A .NET BinaryWriter string: a 7-bit encoded byte length, then UTF-8.
    //! Invalid UTF-8 comes back as U+FFFD.
    QString string()
    {
        quint32 length = 0;
        for (int shift = 0; !hasError(); shift += kStringLengthBitsPerByte) {
            const quint8 byte = u8();
            if (shift == kStringLengthLastShift && byte > kStringLengthLastByteMax) {
                fail(QStringLiteral("The PocketTopo file has a string length longer than five bytes in a %1.")
                         .arg(m_context));
                break;
            }
            length |= quint32(byte & kStringLengthValueMask) << shift;
            if ((byte & kStringLengthMoreBit) == 0) {
                break;
            }
        }
        return QString::fromUtf8(bytes(length));
    }

    //! A table count, checked against what the remaining bytes can hold.
    qint32 count(int minRecordSize)
    {
        const qint32 value = i32();
        if (hasError()) {
            return 0;
        }
        if (value < 0 || qint64(value) * minRecordSize > remaining()) {
            fail(QStringLiteral("The PocketTopo file has an impossible %1 count of %2.")
                     .arg(m_context)
                     .arg(value));
            return 0;
        }
        return value;
    }

    //! Names the record being read, for error messages.
    void setContext(const QString& context) { m_context = context; }

private:
    quint64 little(qsizetype size)
    {
        const QByteArray data = bytes(size);
        quint64 value = 0;
        for (qsizetype i = 0; i < data.size(); ++i) {
            value |= quint64(quint8(data.at(i))) << (kBitsPerByte * i);
        }
        return value;
    }

    const QByteArray& m_bytes;
    qsizetype m_position = 0;
    QString m_context;
    QString m_error;
};

Trip readTrip(Cursor& cursor)
{
    Trip trip;
    trip.ticks = cursor.i64();
    if (!cursor.hasError() && (trip.ticks < 0 || trip.ticks > kMaxTicks)) {
        cursor.fail(QStringLiteral("The PocketTopo file has a trip date outside 0001-01-01 to 9999-12-31."));
    }
    trip.comment = cursor.string();
    trip.declination = cursor.i16();
    return trip;
}

Shot readShot(Cursor& cursor)
{
    Shot shot;
    shot.from = StationId::fromRaw(cursor.u32());
    shot.to = StationId::fromRaw(cursor.u32());
    shot.distance = cursor.i32();
    shot.azimuth = cursor.u16();
    shot.inclination = cursor.i16();
    shot.flags = cursor.u8();
    shot.roll = cursor.u8();
    shot.tripIndex = cursor.i16();
    if (shot.hasComment()) {
        shot.comment = cursor.string();
    }
    return shot;
}

Reference readReference(Cursor& cursor)
{
    Reference reference;
    reference.station = StationId::fromRaw(cursor.u32());
    reference.east = cursor.i64();
    reference.north = cursor.i64();
    reference.altitude = cursor.i32();
    reference.comment = cursor.string();
    return reference;
}

void skipMapping(Cursor& cursor)
{
    cursor.i32();
    cursor.i32();
    cursor.i32();
}

//! Skips a drawing, returning how many polygons and cross-sections it holds.
//! Unknown element ids carry no payload, as in PocketTopo's own reader.
int skipDrawing(Cursor& cursor)
{
    skipMapping(cursor);
    int elementCount = 0;
    while (!cursor.hasError()) {
        const quint8 id = cursor.u8();
        if (cursor.hasError() || id == kElementEnd) {
            break;
        }
        if (id == kElementPolygon) {
            const qint32 pointCount = cursor.count(kPointSize);
            cursor.bytes(qsizetype(pointCount) * kPointSize);
            cursor.u8();
            ++elementCount;
        } else if (id == kElementXSection) {
            cursor.i32();
            cursor.i32();
            cursor.u32();
            cursor.i32();
            ++elementCount;
        }
    }
    return elementCount;
}

} // namespace

StationId StationId::fromRaw(quint32 raw)
{
    StationId id;
    id.m_raw = raw;
    return id;
}

StationId StationId::plain(quint32 number)
{
    return fromRaw(number + kPlainOffset);
}

StationId StationId::dotted(quint16 major, quint16 minor)
{
    return fromRaw((quint32(major) << kMajorShift) | minor);
}

quint32 StationId::raw() const
{
    return m_raw;
}

bool StationId::isDefined() const
{
    return m_raw != kUndefinedRaw;
}

bool StationId::isPlain() const
{
    return isDefined() && nativeValue(m_raw) < kReservedNativeMin;
}

QString StationId::name() const
{
    if (!isDefined()) {
        return QString();
    }

    const qint64 native = nativeValue(m_raw);
    if (native >= 0) {
        return QStringLiteral("%1.%2").arg(native >> kMajorShift).arg(native & kMinorMask);
    } else if (native < kReservedNativeMin) {
        return QString::number(native + kNativePlainOffset);
    } else {
        return QString();
    }
}

QDate Trip::date() const
{
    return QDate(1, 1, 1).addDays(ticks / kTicksPerDay);
}

bool Trip::isAutoDeclination() const
{
    return declination == kAutoDeclination;
}

double Trip::declinationDegrees() const
{
    if (isAutoDeclination()) {
        return 0.0;
    }
    return angleDegrees(declination);
}

bool Shot::isSplay() const
{
    return !to.isDefined();
}

bool Shot::hasComment() const
{
    return (flags & HasComment) != 0;
}

bool Shot::isFlipped() const
{
    return (flags & Flipped) != 0;
}

std::optional<double> Shot::distanceMeters() const
{
    return meters(distance, kBlankDistance);
}

double Shot::azimuthDegrees() const
{
    return angleDegrees(azimuth);
}

double Shot::inclinationDegrees() const
{
    return angleDegrees(inclination);
}

std::optional<double> Reference::eastMeters() const
{
    return meters(east, kBlankCoordinate);
}

std::optional<double> Reference::northMeters() const
{
    return meters(north, kBlankCoordinate);
}

std::optional<double> Reference::altitudeMeters() const
{
    return meters(altitude, kBlankAltitude);
}

Monad::Result<Document> read(const QByteArray& bytes)
{
    Cursor cursor(bytes);

    cursor.setContext(QStringLiteral("header"));
    const QByteArray magic = cursor.bytes(kMagic.size());
    const quint8 version = cursor.u8();
    if (cursor.hasError() || magic != kMagic) {
        return Monad::Result<Document>(QStringLiteral("The file is not a PocketTopo .top file."));
    }
    if (version != kSupportedVersion) {
        return Monad::Result<Document>(
            QStringLiteral("The PocketTopo file is version %1; only version %2 can be read.")
                .arg(version)
                .arg(kSupportedVersion));
    }

    Document document;

    cursor.setContext(QStringLiteral("trip"));
    const qint32 tripCount = cursor.count(kTripMinSize);
    for (qint32 i = 0; i < tripCount && !cursor.hasError(); ++i) {
        document.trips.append(readTrip(cursor));
    }

    cursor.setContext(QStringLiteral("shot"));
    const qint32 shotCount = cursor.count(kShotMinSize);
    for (qint32 i = 0; i < shotCount && !cursor.hasError(); ++i) {
        document.shots.append(readShot(cursor));
    }

    cursor.setContext(QStringLiteral("reference"));
    const qint32 referenceCount = cursor.count(kReferenceMinSize);
    for (qint32 i = 0; i < referenceCount && !cursor.hasError(); ++i) {
        document.references.append(readReference(cursor));
    }

    cursor.setContext(QStringLiteral("drawing"));
    skipMapping(cursor);
    document.planElementCount = skipDrawing(cursor);
    document.sideElementCount = skipDrawing(cursor);

    if (cursor.hasError()) {
        return Monad::Result<Document>(cursor.error());
    }
    return Monad::Result<Document>(document);
}

Monad::Result<Document> readFile(const QString& path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        return read(file.readAll());
    }
    return Monad::Result<Document>(
        QStringLiteral("Can't open PocketTopo file \"%1\": %2").arg(path, file.errorString()));
}

} // namespace cwPocketTopoFile
