/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwCompassMakTranslator.h"
#include "cwCompassMakFile.h"
#include "cwExternalCenterlineScanner.h"

//Qt includes
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QTextStream>

//Std includes
#include <algorithm>

namespace {

// The printable ASCII range Survex can be told to read as name characters.
constexpr char16_t kFirstPrintable = u'!';
constexpr char16_t kLastPrintable = u'~';

// The punctuation Survex reads in a name by default.
constexpr QLatin1StringView kDefaultNamePunctuation("_-");

// Characters that keep a meaning in every name position whatever *set names
// says: the separator, comment, quote, root and keyword characters, and the
// ones Compass itself cannot put in a .mak name.
constexpr QLatin1StringView kUnspellableCharacters(".;\"\\*,[]");

/**
 * Adds \a name's punctuation beyond Survex's default to \a extras. False when
 * no `*set names` line lets Survex read \a name.
 */
bool collectNameCharacters(const QString& name, QString& extras)
{
    if (name.isEmpty()) {
        return false;
    }
    for (const QChar c : name) {
        const char16_t code = c.unicode();
        if (code < kFirstPrintable || code > kLastPrintable || kUnspellableCharacters.contains(c)) {
            return false;
        }
        if (c.isLetterOrNumber() || kDefaultNamePunctuation.contains(c) || extras.contains(c)) {
            continue;
        }
        extras.append(c);
    }
    return true;
}

//! The spelling \a stations give \a name, matching case-insensitively when no
//! station matches exactly, or \a name itself when none matches.
QString stationSpelling(const QSet<QString>& stations, const QString& name)
{
    if (stations.contains(name)) {
        return name;
    }
    for (const QString& station : stations) {
        if (station.compare(name, Qt::CaseInsensitive) == 0) {
            return station;
        }
    }
    return name;
}

struct Block {
    const cwCompassMakFile::DatReference* reference = nullptr;
    QString survey;
    QString path;
    //! The node's fixes on this block's stations, renamed to the station alone.
    QList<cwFixStation> nodeFixes;
};

void writeFix(QTextStream& stream, const QString& station, double easting, double northing,
              double elevation)
{
    stream << "*fix " << station << ' ';
    cwSurvexExporterUtils::writeCoordTriplet(stream, easting, northing, elevation);
    stream << Qt::endl;
}

void writeNodeFix(QTextStream& stream, const cwFixStation& fix, cwSurvexExporterUtils::CsScope& scope)
{
    scope.ensure(stream, fix.inputCS());
    writeFix(stream, fix.stationName(), fix.easting(), fix.northing(), fix.elevation());
}

} // namespace

namespace cwCompassMakTranslator {

std::optional<QString> translate(const Input& input, const cwSurvexExporterUtils::CsScope& scope)
{
    const std::optional<QString> makText = cwExternalCenterlineScanner::readSurveyText(input.makPath);
    if (!makText.has_value()) {
        return std::nullopt;
    }
    const cwCompassMakFile::Project project = cwCompassMakFile::parse(*makText);

    QString extraNameCharacters;
    QList<Block> blocks;
    // Every station each survey holds so far, as cavern's prefix tree would.
    QHash<QString, QSet<QString>> surveyStations;
    // The surveys read so far, in order; a link searches them most recent first.
    QStringList readSurveys;
    QStringList equates;

    for (const cwCompassMakFile::DatReference& reference : project.references) {
        const QString path =
            cwExternalCenterlineScanner::resolveCompassReference(input.makPath, reference.file);
        // Cavern warns and moves on from a missing DAT, fixing and linking
        // nothing on its line; the scan has already warned.
        if (path.isEmpty()) {
            continue;
        }
        const QString survey = cwCompassMakFile::datSurveyName(reference.file);
        if (!cwSurvexExporterUtils::isQuotableIncludePath(path)
            || !collectNameCharacters(survey, extraNameCharacters)) {
            return std::nullopt;
        }

        QSet<QString>& stations = surveyStations[survey];
        if (!cwCompassMakFile::isMakFile(path)) {
            if (const auto datText = cwExternalCenterlineScanner::readSurveyText(path)) {
                stations.unite(cwExternalCenterlineScanner::compassDatStationNames(*datText));
            }
        }
        for (const cwCompassMakFile::Fix& fix : reference.fixes) {
            // A bracket cavern can't read is an error it reports itself, from
            // the .mak as it is.
            if (!fix.meters.has_value() || !collectNameCharacters(fix.station, extraNameCharacters)) {
                return std::nullopt;
            }
            stations.insert(fix.station);
        }

        // A link station joins this DAT to the most recent earlier one that
        // has a station of that name; one no earlier DAT has links nothing.
        for (const QString& link : reference.linkStations) {
            if (!stations.contains(link)) {
                continue;
            }
            for (qsizetype i = readSurveys.size() - 1; i >= 0; --i) {
                const QString& earlier = readSurveys.at(i);
                if (!surveyStations.value(earlier).contains(link)) {
                    continue;
                }
                if (earlier != survey) {
                    if (!collectNameCharacters(link, extraNameCharacters)) {
                        return std::nullopt;
                    }
                    equates.append(QStringLiteral("*equate %1.%3 %2.%3").arg(earlier, survey, link));
                }
                break;
            }
        }

        readSurveys.append(survey);
        blocks.append({&reference, survey, path, {}});
    }

    // The node's fixes go into the block whose survey they name.
    QList<cwFixStation> unplacedNodeFixes;
    for (const cwFixStation& fix : input.nodeFixes) {
        const QString name = fix.stationName().trimmed();
        const auto block = std::find_if(blocks.begin(), blocks.end(), [&name](const Block& block) {
            return name.startsWith(block.survey + QLatin1Char('.'), Qt::CaseInsensitive);
        });
        if (block == blocks.end()) {
            unplacedNodeFixes.append(fix);
            continue;
        }
        cwFixStation renamed = fix;
        renamed.setStationName(stationSpelling(surveyStations.value(block->survey),
                                               name.mid(block->survey.size() + 1)));
        if (!collectNameCharacters(renamed.stationName(), extraNameCharacters)) {
            return std::nullopt;
        }
        block->nodeFixes.append(renamed);
    }

    const bool georeferenced = !input.outputCS.isEmpty();

    QString text;
    QTextStream stream(&text);
    cwSurvexExporterUtils::CsScope wrapperScope(scope);
    stream << "*begin ; " << QFileInfo(input.makPath).fileName() << ", read as Survex"
           << Qt::endl;
    stream << "*case preserve" << Qt::endl;
    if (!extraNameCharacters.isEmpty()) {
        stream << "*set names " << kDefaultNamePunctuation << extraNameCharacters << Qt::endl;
    }
    for (const cwFixStation& fix : std::as_const(unplacedNodeFixes)) {
        writeNodeFix(stream, fix, wrapperScope);
    }

    for (const Block& block : std::as_const(blocks)) {
        const cwCompassMakFile::DatReference& reference = *block.reference;
        cwSurvexExporterUtils::CsScope blockScope(wrapperScope);
        stream << "*begin " << block.survey << Qt::endl;

        // Cavern computes the declination at the base location whenever a
        // datum reads it, and a Compass DAT's own DECLINATION then gives way.
        const QString baseSystem = reference.baseLocationCoordinateSystem();
        if (georeferenced && !baseSystem.isEmpty()) {
            blockScope.ensure(stream, baseSystem);
            stream << "*declination auto ";
            cwSurvexExporterUtils::writeCoordTriplet(stream, reference.baseLocation->easting,
                                                     reference.baseLocation->northing,
                                                     reference.baseLocation->elevation);
            stream << Qt::endl;
        }

        stream << "*include \"" << block.path << "\"" << Qt::endl;

        // Cavern reads a fix whose line names no datum and zone in the output
        // system as it is.
        const QString fixSystem = reference.coordinateSystem().isEmpty()
            ? input.outputCS
            : reference.coordinateSystem();
        for (const cwCompassMakFile::Fix& fix : reference.fixes) {
            if (georeferenced) {
                blockScope.ensure(stream, fixSystem);
            }
            const std::array<double, 3>& meters = *fix.meters;
            writeFix(stream, fix.station, meters.at(0), meters.at(1), meters.at(2));
            stream << "*entrance " << fix.station << Qt::endl;
        }
        for (const cwFixStation& fix : block.nodeFixes) {
            writeNodeFix(stream, fix, blockScope);
        }

        stream << "*end " << block.survey << Qt::endl;
    }

    for (const QString& equate : std::as_const(equates)) {
        stream << equate << Qt::endl;
    }
    stream << "*end" << Qt::endl;
    stream.flush();
    return text;
}

} // namespace cwCompassMakTranslator
