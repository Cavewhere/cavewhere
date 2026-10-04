/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwCompassMakFile.h"

//Std includes
#include <cstdlib>

namespace {

// Compass's own default datum, which Compass reads a UTM zone in when its
// .mak names none (survex doc/compass.rst).
QString defaultDatum()
{
    return QStringLiteral("North American 1927");
}

// survex METRES_PER_FOOT, which cavern converts a "[f,...]" fix by.
constexpr double kMetersPerFoot = 0.3048;
constexpr int kFirstUtmZone = 1;
constexpr int kMaxUtmZone = 60;
constexpr int kCoordinateCount = 3;
// A base location's fields: easting, northing, elevation, zone, convergence.
constexpr int kBaseLocationFieldCount = 5;
constexpr int kBaseZoneField = 3;

// survex img.c img_compass_utm_proj_str: the EPSG code ranges each datum's
// UTM zones are cataloged in.
constexpr int kAdindanBase = 20100;
constexpr int kAdindanFirstZone = 35;
constexpr int kAdindanLastZone = 38;
constexpr int kArc1950Base = 20900;
constexpr int kArc1950FirstZone = -36;
constexpr int kArc1950LastZone = -34;
constexpr int kArc1960Base = 21000;
constexpr int kArc1960FirstZone = -37;
constexpr int kArc1960LastZone = -35;
constexpr int kCapeBase = 22200;
constexpr int kCapeFirstZone = -36;
constexpr int kCapeLastZone = -34;
constexpr int kEuropean1950Base = 23000;
constexpr int kEuropean1950FirstZone = 28;
constexpr int kEuropean1950LastZone = 38;
constexpr int kNzgd49Base = 27200;
constexpr int kNzgd49FirstZone = 58;
constexpr int kHuTzuShan1950Code = 3829;
constexpr int kHuTzuShan1950Zone = 51;
constexpr int kIndian1960Base = 3100;
constexpr int kIndian1960FirstZone = 48;
constexpr int kIndian1960LastZone = 49;
constexpr int kNad27Base = 26700;
constexpr int kNad27PacificBase = 3311;
constexpr int kNad83Base = 26900;
constexpr int kNad83PacificBase = 3313;
constexpr int kNad83Zone24Code = 9712;
constexpr int kNad83Zone24 = 24;
constexpr int kNorthAmericanLastZone = 23;
constexpr int kNorthAmericanPacificFirstZone = 59;
constexpr int kTokyoBase = 3041;
constexpr int kTokyoFirstZone = 51;
constexpr int kTokyoLastZone = 55;
constexpr int kWgs72NorthBase = 32200;
constexpr int kWgs72SouthBase = 32300;
constexpr int kWgs84NorthBase = 32600;
constexpr int kWgs84SouthBase = 32700;

enum class Datum {
    Unknown,
    Adindan,
    Arc1950,
    Arc1960,
    Cape,
    European1950,
    Nzgd49,
    HuTzuShan1950,
    Indian1960,
    Nad27,
    Nad83,
    Tokyo,
    Wgs72,
    Wgs84
};

// survex img.c img_parse_compass_datum_string: an exact, case-sensitive match.
Datum parseDatum(const QString& text)
{
    struct Name {
        const char* text;
        Datum datum;
    };
    static constexpr Name kNames[] = {
        {"WGS 1984", Datum::Wgs84},
        {"North American 1927", Datum::Nad27},
        {"North American 1983", Datum::Nad83},
        {"Adindan", Datum::Adindan},
        {"Arc 1950", Datum::Arc1950},
        {"Arc 1960", Datum::Arc1960},
        {"Cape", Datum::Cape},
        {"European 1950", Datum::European1950},
        {"Geodetic 1949", Datum::Nzgd49},
        {"Hu Tzu Shan", Datum::HuTzuShan1950},
        {"Indian", Datum::Indian1960},
        {"Tokyo", Datum::Tokyo},
        {"WGS 1972", Datum::Wgs72},
    };
    for (const Name& name : kNames) {
        if (text == QLatin1String(name.text)) {
            return name.datum;
        }
    }
    return Datum::Unknown;
}

bool inRange(int zone, int first, int last)
{
    return zone >= first && zone <= last;
}

//! The EPSG code survex gives \a datum at \a zone, or 0 when it gives none.
int epsgCode(Datum datum, int zone)
{
    switch (datum) {
    case Datum::Unknown:
        return 0;
    case Datum::Adindan:
        return inRange(zone, kAdindanFirstZone, kAdindanLastZone) ? kAdindanBase + zone : 0;
    case Datum::Arc1950:
        return inRange(zone, kArc1950FirstZone, kArc1950LastZone) ? kArc1950Base - zone : 0;
    case Datum::Arc1960:
        return inRange(zone, kArc1960FirstZone, kArc1960LastZone) ? kArc1960Base - zone : 0;
    case Datum::Cape:
        return inRange(zone, kCapeFirstZone, kCapeLastZone) ? kCapeBase - zone : 0;
    case Datum::European1950:
        return inRange(zone, kEuropean1950FirstZone, kEuropean1950LastZone)
                   ? kEuropean1950Base + zone : 0;
    case Datum::Nzgd49:
        return zone >= kNzgd49FirstZone ? kNzgd49Base + zone : 0;
    case Datum::HuTzuShan1950:
        return zone == kHuTzuShan1950Zone ? kHuTzuShan1950Code : 0;
    case Datum::Indian1960:
        return inRange(zone, kIndian1960FirstZone, kIndian1960LastZone)
                   ? kIndian1960Base + zone : 0;
    case Datum::Nad27:
        if (inRange(zone, kFirstUtmZone, kNorthAmericanLastZone)) {
            return kNad27Base + zone;
        }
        return zone >= kNorthAmericanPacificFirstZone ? kNad27PacificBase + zone : 0;
    case Datum::Nad83:
        if (inRange(zone, kFirstUtmZone, kNorthAmericanLastZone)) {
            return kNad83Base + zone;
        }
        if (zone == kNad83Zone24) {
            return kNad83Zone24Code;
        }
        return zone >= kNorthAmericanPacificFirstZone ? kNad83PacificBase + zone : 0;
    case Datum::Tokyo:
        return inRange(zone, kTokyoFirstZone, kTokyoLastZone) ? kTokyoBase + zone : 0;
    case Datum::Wgs72:
        return zone > 0 ? kWgs72NorthBase + zone : kWgs72SouthBase - zone;
    case Datum::Wgs84:
        return zone > 0 ? kWgs84NorthBase + zone : kWgs84SouthBase - zone;
    }
    return 0;
}

//! The PROJ datum name survex spells a NAD zone EPSG has no code for with.
QString projDatumName(Datum datum)
{
    switch (datum) {
    case Datum::Nad27:
        return QStringLiteral("NAD27");
    case Datum::Nad83:
        return QStringLiteral("NAD83");
    default:
        return QString();
    }
}

bool isLineEnd(QChar c)
{
    return c == QLatin1Char('\n') || c == QLatin1Char('\r');
}

/**
 * Walks a .mak's text the way cavern's free-form reader does: blanks, line
 * ends and comments between tokens are skipped, and a comment runs from '/'
 * to the next '/' or the end of the line.
 */
class Reader
{
public:
    explicit Reader(const QString& text) : m_text(text) {}

    bool atEnd() const { return m_position >= m_text.size(); }
    QChar peek() const { return atEnd() ? QChar() : m_text.at(m_position); }
    void advance() { ++m_position; }

    void skipBlanks()
    {
        while (!atEnd()) {
            const QChar c = peek();
            if (c == QLatin1Char('/')) {
                skipComment();
            } else if (c.isSpace()) {
                advance();
            } else {
                return;
            }
        }
    }

    //! Skips blanks and line ends only, so a '/' that starts a path stays.
    void skipWhitespace()
    {
        while (!atEnd() && peek().isSpace()) {
            advance();
        }
    }

    //! The text up to the statement's ';', which is consumed, comments removed.
    QString statement() { return readUntil(false); }

    //! The text up to a ';' (consumed) or the end of the line.
    QString lineStatement() { return readUntil(true); }

    //! A '#' line's file name, up to the ',' or ';' after it, which is left
    //! for the caller. Embedded line ends are dropped, as cavern drops them.
    QString fileName()
    {
        QString name;
        if (peek() == QLatin1Char('"')) {
            advance();
            while (!atEnd() && peek() != QLatin1Char('"')) {
                name.append(peek());
                advance();
            }
            advance();
            skipToListSeparator();
            return name;
        }
        while (!atEnd() && peek() != QLatin1Char(',') && peek() != QLatin1Char(';')) {
            if (!isLineEnd(peek())) {
                name.append(peek());
            }
            advance();
        }
        return name.trimmed();
    }

    //! A station name in a '#' line's list.
    QString stationName()
    {
        QString name;
        while (!atEnd()) {
            const QChar c = peek();
            if (c.isSpace() || c == QLatin1Char(',') || c == QLatin1Char(';')
                || c == QLatin1Char('[')) {
                break;
            }
            name.append(c);
            advance();
        }
        return name;
    }

    //! The text inside a '[...]' at the reader, which is consumed.
    QString bracket()
    {
        advance();
        QString content;
        while (!atEnd() && peek() != QLatin1Char(']')) {
            content.append(peek());
            advance();
        }
        advance();
        return content.trimmed();
    }

    void skipToListSeparator()
    {
        while (!atEnd() && peek() != QLatin1Char(',') && peek() != QLatin1Char(';')) {
            advance();
        }
    }

private:
    void skipComment()
    {
        advance();
        while (!atEnd() && peek() != QLatin1Char('/') && !isLineEnd(peek())) {
            advance();
        }
        if (peek() == QLatin1Char('/')) {
            advance();
        }
    }

    QString readUntil(bool stopAtLineEnd)
    {
        QString text;
        while (!atEnd() && peek() != QLatin1Char(';')) {
            const QChar c = peek();
            if (stopAtLineEnd && isLineEnd(c)) {
                return text.trimmed();
            }
            if (c == QLatin1Char('/')) {
                skipComment();
                continue;
            }
            text.append(c);
            advance();
        }
        advance();
        return text.trimmed();
    }

    const QString& m_text;
    qsizetype m_position = 0;
};

std::optional<std::array<double, 3>> fixMeters(const QString& coordinate)
{
    if (coordinate.isEmpty()) {
        return std::nullopt;
    }
    const QChar unit = coordinate.at(0).toLower();
    if (unit != QLatin1Char('f') && unit != QLatin1Char('m')) {
        return std::nullopt;
    }
    const QStringList fields = coordinate.mid(1).split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (fields.size() < kCoordinateCount) {
        return std::nullopt;
    }
    const double scale = unit == QLatin1Char('f') ? kMetersPerFoot : 1.0;
    std::array<double, 3> meters {};
    for (int i = 0; i < kCoordinateCount; ++i) {
        bool ok = false;
        meters[i] = fields.at(i).trimmed().toDouble(&ok) * scale;
        if (!ok) {
            return std::nullopt;
        }
    }
    return meters;
}

std::optional<cwCompassMakFile::BaseLocation> baseLocation(const QString& text)
{
    const QStringList fields = text.split(QLatin1Char(','));
    if (fields.size() < kBaseLocationFieldCount) {
        return std::nullopt;
    }
    std::array<double, 3> coordinate {};
    for (int i = 0; i < kCoordinateCount; ++i) {
        bool ok = false;
        coordinate[i] = fields.at(i).trimmed().toDouble(&ok);
        if (!ok) {
            return std::nullopt;
        }
    }
    bool zoneOk = false;
    const int zone = fields.at(kBaseZoneField).trimmed().toInt(&zoneOk);
    if (!zoneOk) {
        return std::nullopt;
    }
    return cwCompassMakFile::BaseLocation{coordinate[0], coordinate[1], coordinate[2], zone};
}

QString withForwardSlashes(QString path)
{
    return path.replace(QLatin1Char('\\'), QLatin1Char('/'));
}

} // namespace

namespace cwCompassMakFile {

int DatReference::fixZone() const
{
    if (datum.isEmpty()) {
        return 0;
    }
    if (zone != 0) {
        return zone;
    }
    return baseLocation.has_value() ? baseLocation->zone : 0;
}

QString DatReference::coordinateSystemName() const
{
    const int zone = fixZone();
    return zone != 0 ? utmSystemName(datum, zone) : QString();
}

QString DatReference::coordinateSystem() const
{
    return utmCoordinateSystem(datum, fixZone());
}

QString DatReference::baseLocationCoordinateSystem() const
{
    return baseLocation.has_value() ? utmCoordinateSystem(datum, baseLocation->zone) : QString();
}

Project parse(const QString& text)
{
    Project project;
    QString datum;
    int zone = 0;
    bool namesSystem = false;
    bool hasDatumLine = false;
    bool namesZone = false;
    std::optional<BaseLocation> base;
    QStringList folders;

    Reader reader(text);
    reader.skipBlanks();
    while (!reader.atEnd()) {
        const QChar command = reader.peek();
        reader.advance();
        switch (command.unicode()) {
        case '#': {
            reader.skipWhitespace();
            const QString name = withForwardSlashes(reader.fileName());
            DatReference reference;
            reference.file = folders.isEmpty() ? name
                                               : folders.constLast() + QLatin1Char('/') + name;
            reference.datum = datum;
            reference.zone = zone;
            reference.namesSystem = namesSystem;
            reference.baseLocation = base;
            while (!reader.atEnd() && reader.peek() != QLatin1Char(';')) {
                reader.advance();
                reader.skipBlanks();
                const QString station = reader.stationName();
                reader.skipBlanks();
                if (reader.peek() == QLatin1Char('[')) {
                    const QString coordinate = reader.bracket();
                    if (!station.isEmpty()) {
                        reference.fixes.append({station, coordinate, fixMeters(coordinate)});
                    }
                } else if (!station.isEmpty()) {
                    reference.linkStations.append(station);
                }
                reader.skipToListSeparator();
            }
            reader.advance();
            if (!name.isEmpty()) {
                project.references.append(reference);
            }
            break;
        }
        case '&':
            reader.skipBlanks();
            datum = reader.lineStatement();
            namesSystem = true;
            hasDatumLine = true;
            break;
        case '$':
            zone = reader.statement().toInt();
            if (std::abs(zone) > kMaxUtmZone) {
                zone = 0;
            }
            namesSystem = true;
            namesZone = namesZone || zone != 0;
            break;
        case '@':
            if (const auto location = baseLocation(reader.statement()); location.has_value()) {
                base = location;
                namesZone = namesZone || location->zone != 0;
            }
            break;
        case '[': {
            const QString folder = withForwardSlashes(reader.lineStatement());
            folders.append(folders.isEmpty() ? folder
                                             : folders.constLast() + QLatin1Char('/') + folder);
            break;
        }
        case ']':
            if (!folders.isEmpty()) {
                folders.removeLast();
            }
            reader.skipBlanks();
            if (reader.peek() == QLatin1Char(';')) {
                reader.advance();
            }
            break;
        default:
            // '%' and '*' (convergence) and '!' (project parameters) are
            // ignored, as cavern ignores them; so is anything unknown.
            reader.statement();
            break;
        }
        reader.skipBlanks();
    }

    project.namedZone = zone != 0 ? zone : (base.has_value() ? base->zone : 0);
    project.datumDefaulted = !hasDatumLine && namesZone;
    if (project.datumDefaulted) {
        for (DatReference& reference : project.references) {
            reference.datum = defaultDatum();
        }
    }
    return project;
}

bool isMakFile(const QString& path)
{
    return path.endsWith(QLatin1String(".mak"), Qt::CaseInsensitive);
}

QString datSurveyName(const QString& datReference)
{
    const qsizetype lastSeparator =
        qMax(datReference.lastIndexOf(QLatin1Char('/')), datReference.lastIndexOf(QLatin1Char('\\')));
    const QString leaf = datReference.mid(lastSeparator + 1);
    const qsizetype extension = leaf.lastIndexOf(QLatin1Char('.'));
    const QString name = (extension >= 0 ? leaf.left(extension) : leaf).toLower();
    return name.isEmpty() ? QStringLiteral("dat") : name;
}

QString utmZoneName(int zone)
{
    return QStringLiteral("%1%2").arg(std::abs(zone)).arg(
        zone < 0 ? QLatin1Char('S') : QLatin1Char('N'));
}

QString utmSystemName(const QString& datum, int zone)
{
    return datum.isEmpty() ? QStringLiteral("UTM zone %1").arg(utmZoneName(zone))
                           : QStringLiteral("%1, UTM zone %2").arg(datum, utmZoneName(zone));
}

QString utmCoordinateSystem(const QString& datum, int zone)
{
    if (zone == 0 || std::abs(zone) > kMaxUtmZone) {
        return QString();
    }
    const Datum parsed = parseDatum(datum);
    const int code = epsgCode(parsed, zone);
    if (code != 0) {
        return QStringLiteral("EPSG:%1").arg(code);
    }
    const QString projDatum = projDatumName(parsed);
    if (projDatum.isEmpty()) {
        return QString();
    }
    return QStringLiteral("+proj=utm +zone=%1 %2+datum=%3 +units=m +no_defs +type=crs")
        .arg(std::abs(zone))
        .arg(zone < 0 ? QStringLiteral("+south ") : QString())
        .arg(projDatum);
}

} // namespace cwCompassMakFile
