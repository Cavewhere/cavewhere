/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwPocketTopoTranslator.h"

//Qt includes
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTextStream>

//Std includes
#include <cmath>

namespace {

using cwPocketTopoFile::Document;
using cwPocketTopoFile::Shot;
using cwPocketTopoFile::StationId;
using cwPocketTopoFile::Trip;

constexpr int kTapeDecimals = 3;
constexpr int kAngleDecimals = 2;
constexpr double kHalfCircle = 180.0;
constexpr double kFullCircle = 360.0;

const QString kRenameSuffix = QStringLiteral("_");
const QChar kSeriesSeparator = QLatin1Char('.');

//! \a value to \a decimals places, with a value that rounds to zero written
//! as a plain zero rather than "-0.00".
QString fixed(double value, int decimals)
{
    const QString text = QString::number(value, 'f', decimals);
    if (text.startsWith(QLatin1Char('-')) && text.toDouble() == 0.0) {
        return text.mid(1);
    }
    return text;
}

//! \a degrees as an azimuth in [0, 360) once rounded.
QString azimuthText(double degrees)
{
    double wrapped = std::fmod(degrees, kFullCircle);
    if (wrapped < 0.0) {
        wrapped += kFullCircle;
    }
    const QString text = fixed(wrapped, kAngleDecimals);
    if (text.toDouble() >= kFullCircle) {
        return fixed(0.0, kAngleDecimals);
    }
    return text;
}

//! True when \a shot's trip index names one of the file's trips.
bool belongsToTrip(const Shot& shot, const Document& document)
{
    return shot.tripIndex >= 0 && shot.tripIndex < document.trips.size();
}

//! The comment's lines, trimmed, always at least one; PocketTopo separates
//! them with CR.
QStringList commentLines(const QString& comment)
{
    static const QRegularExpression kLineBreak(QStringLiteral("\r\n|\r|\n"));
    QStringList lines = comment.split(kLineBreak);
    for (QString& line : lines) {
        line = line.trimmed();
    }
    return lines;
}

//! An unordered station pair, for grouping repeated readings.
bool samePair(const Shot& a, const Shot& b)
{
    return (a.from == b.from && a.to == b.to) || (a.from == b.to && a.to == b.from);
}

class TripWriter
{
public:
    TripWriter(QTextStream& stream, const QHash<quint32, QString>& names) :
        m_stream(stream),
        m_names(names)
    {
    }

    void add(const Shot& shot)
    {
        const QString from = m_names.value(shot.from.raw());
        if (from.isEmpty()) {
            return;
        }
        const std::optional<double> distance = shot.distanceMeters();
        if (!distance.has_value()) {
            return;
        }

        if (shot.isSplay()) {
            m_splays.append(shot);
            return;
        }

        const QString to = m_names.value(shot.to.raw());
        if (to.isEmpty()) {
            return;
        }

        const bool continuesGroup = m_groupFirst.has_value() && samePair(*m_groupFirst, shot);
        if (!continuesGroup) {
            flushSplays();
            m_groupFirst.reset();
        }

        if (*distance == 0.0) {
            m_stream << "*equate " << from << ' ' << to << Qt::endl;
            return;
        }

        if (continuesGroup && shot.from != m_groupFirst->from) {
            writeLeg(to, from, *distance, shot.azimuthDegrees() + kHalfCircle, -shot.inclinationDegrees());
        } else {
            writeLeg(from, to, *distance, shot.azimuthDegrees(), shot.inclinationDegrees());
        }
        if (!continuesGroup) {
            m_groupFirst = shot;
        }
    }

    void finish()
    {
        flushSplays();
        m_groupFirst.reset();
    }

private:
    void writeLeg(const QString& from, const QString& to, double distance, double azimuth, double clino)
    {
        m_stream << from << ' ' << to << ' ' << fixed(distance, kTapeDecimals) << ' '
                 << azimuthText(azimuth) << ' ' << fixed(clino, kAngleDecimals) << Qt::endl;
    }

    void flushSplays()
    {
        if (m_splays.isEmpty()) {
            return;
        }
        m_stream << "*flags splay" << Qt::endl;
        for (const Shot& splay : std::as_const(m_splays)) {
            writeLeg(m_names.value(splay.from.raw()), QStringLiteral(".."), *splay.distanceMeters(),
                     splay.azimuthDegrees(), splay.inclinationDegrees());
        }
        m_stream << "*flags not splay" << Qt::endl;
        m_splays.clear();
    }

    QTextStream& m_stream;
    const QHash<quint32, QString>& m_names;
    //! The first reading of the station pair being written.
    std::optional<Shot> m_groupFirst;
    //! The splays met since the group began, written after it.
    QList<Shot> m_splays;
};

void writeTripHead(QTextStream& stream,
                   int index,
                   const Trip& trip,
                   const cwPocketTopoTranslator::Input& input,
                   cwSurvexExporterUtils::CsScope& scope)
{
    const QDate date = trip.date();
    const QStringList lines = commentLines(trip.comment);
    const QString& firstLine = lines.first();

    stream << Qt::endl << "; trip #" << (index + 1) << " : " << date.toString(Qt::ISODate);
    if (!firstLine.isEmpty()) {
        stream << ' ' << firstLine;
    }
    stream << Qt::endl;
    stream << "*date " << date.toString(QStringLiteral("yyyy.MM.dd")) << Qt::endl;

    if (trip.isAutoDeclination() && input.autoDeclinationLocation.has_value()) {
        cwSurvexExporterUtils::writeDeclinationAuto(stream, *input.autoDeclinationLocation, scope);
    } else {
        const double degrees = trip.isAutoDeclination() ? input.injectedDeclinations.value(index, 0.0)
                                                        : trip.declinationDegrees();
        stream << "*declination " << fixed(degrees, kAngleDecimals) << " degrees" << Qt::endl;
    }

    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QString& line = lines.at(i);
        if (!line.isEmpty()) {
            stream << "*team \"" << QString(line).replace(QLatin1Char('"'), QLatin1Char('\''))
                   << '"' << Qt::endl;
        }
    }
}

} // namespace

namespace cwPocketTopoTranslator {

std::optional<QString> translate(const Input& input, const cwSurvexExporterUtils::CsScope& scope)
{
    const Monad::Result<Document> read = cwPocketTopoFile::readFile(input.topPath);
    if (read.hasError()) {
        return std::nullopt;
    }
    const Document document = read.value();
    const QHash<quint32, QString> names = stationNames(document);

    QString text;
    QTextStream stream(&text);
    cwSurvexExporterUtils::CsScope blockScope(scope);
    stream << "*units tape metres" << Qt::endl;
    stream << "*units compass clino degrees" << Qt::endl;
    stream << "*data normal from to tape compass clino" << Qt::endl;

    for (qsizetype tripIndex = 0; tripIndex < document.trips.size(); ++tripIndex) {
        writeTripHead(stream, int(tripIndex), document.trips.at(tripIndex), input, blockScope);
        TripWriter writer(stream, names);
        for (const Shot& shot : document.shots) {
            if (shot.tripIndex == tripIndex) {
                writer.add(shot);
            }
        }
        writer.finish();
    }

    bool fixesBegun = false;
    for (const cwPocketTopoFile::Reference& reference : document.references) {
        const QString name = names.value(reference.station.raw());
        const std::optional<double> east = reference.eastMeters();
        const std::optional<double> north = reference.northMeters();
        const std::optional<double> altitude = reference.altitudeMeters();
        if (name.isEmpty() || !east.has_value() || !north.has_value() || !altitude.has_value()) {
            continue;
        }
        if (!fixesBegun) {
            stream << Qt::endl;
            blockScope.ensure(stream, input.fixCS);
            fixesBegun = true;
        }
        stream << "*fix " << name << ' ';
        cwSurvexExporterUtils::writeCoordTriplet(stream, *east, *north, *altitude);
        stream << Qt::endl;
    }

    stream.flush();
    return text;
}

QHash<quint32, QString> stationNames(const Document& document)
{
    QList<StationId> stations;
    for (const Shot& shot : document.shots) {
        if (belongsToTrip(shot, document)) {
            stations.append(shot.from);
            stations.append(shot.to);
        }
    }
    for (const cwPocketTopoFile::Reference& reference : document.references) {
        stations.append(reference.station);
    }

    QHash<quint32, QString> names;
    QSet<QString> series;
    for (const StationId& station : std::as_const(stations)) {
        const QString name = station.name();
        if (name.isEmpty()) {
            continue;
        }
        names.insert(station.raw(), name);
        if (!station.isPlain()) {
            series.insert(name.section(kSeriesSeparator, 0, 0));
        }
    }

    for (auto it = names.begin(); it != names.end(); ++it) {
        if (StationId::fromRaw(it.key()).isPlain() && series.contains(it.value())) {
            it.value() += kRenameSuffix;
        }
    }
    return names;
}

} // namespace cwPocketTopoTranslator
