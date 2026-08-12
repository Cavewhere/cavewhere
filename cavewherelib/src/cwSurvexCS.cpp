/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexCS.h"
#include "cwCoordinateTransform.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

//Std includes
#include <algorithm>

namespace {

//Cavern's own ceiling on an EPSG or ESRI code.
constexpr int kAuthorityCodeLimit = 1000000;

const auto kOsgbPrefix = QLatin1String("OSGB:");

//What cavern assumes when the name it is given doesn't exist as written
//(fopen_portable with EXT_PRJ), and what makes the file an ESRI .prj too.
const auto kSidecarSuffix = QLatin1String(".prj");

//! `*cs` argument reading the system from \a fileName. Survex takes the name
//! after an @; a name with a space needs the whole token quoted, which it
//! accepts as "@name.prj".
QString sidecarReference(const QString& fileName)
{
    const bool hasSpace = std::any_of(fileName.cbegin(), fileName.cend(),
                                      [](QChar c) { return c.isSpace(); });
    return hasSpace ? QStringLiteral("CUSTOM \"@%1\"").arg(fileName)
                    : QStringLiteral("CUSTOM @%1").arg(fileName);
}

//An OSGB square is 100 km on a side, and the national grid's origin sits 14
//squares east and 20 squares north of the letter grid's own origin.
constexpr int kOsgbSquareMeters = 100000;
constexpr int kOsgbOriginEastingSquares = 14;
constexpr int kOsgbOriginNorthingSquares = 20;
constexpr int kOsgbSquaresPerLetter = 5;

//! The PROJ string for an OSGB letter pair, or nothing when the pair isn't one.
//! The letters name a 100 km square and shift the false origin onto its
//! south-west corner, so OSGB:SD is a different system from OSGB:SV — which is
//! the pair that lands on the national grid itself, EPSG:27700.
QString osgbProjCS(const QString& square)
{
    if (square.size() != 2) {
        return QString();
    }

    const QChar first = square.at(0).toUpper();
    const QChar second = square.at(1).toUpper();
    if (!QStringLiteral("HNOST").contains(first)
        || second < QLatin1Char('A') || second > QLatin1Char('Z')
        || second == QLatin1Char('I')) {
        return QString();
    }

    //The grid skips I, so every letter past it counts one lower.
    const auto letterIndex = [](QChar letter) {
        const int index = letter.unicode() - 'A';
        return letter > QLatin1Char('I') ? index - 1 : index;
    };
    const int firstIndex = letterIndex(first);
    const int secondIndex = letterIndex(second);
    const int x = (firstIndex % kOsgbSquaresPerLetter) * kOsgbSquaresPerLetter
                  + secondIndex % kOsgbSquaresPerLetter;
    const int y = (firstIndex / kOsgbSquaresPerLetter) * kOsgbSquaresPerLetter
                  + secondIndex / kOsgbSquaresPerLetter;

    return QStringLiteral("+proj=tmerc +lat_0=49 +lon_0=-2 +k=0.9996012717 "
                          "+x_0=%1 +y_0=%2 +ellps=airy +datum=OSGB36 +units=m +no_defs")
        .arg((kOsgbOriginEastingSquares - x) * kOsgbSquareMeters)
        .arg((y - kOsgbOriginNorthingSquares) * kOsgbSquareMeters);
}

//! The spellings that stand for one fixed system each. The strings are copied
//! from cavern so a file round-trips through both programs onto the same
//! system. LOCAL belongs here too: it is a spelling survex defines, and the
//! system it names is none — deliberately ungeoreferenced, which is a different
//! answer from a system we failed to map.
const QHash<QString, QString>& keywordSystems()
{
    static const QHash<QString, QString> systems = {
        { QStringLiteral("LOCAL"), QString() },
        { QStringLiteral("S-MERC"), QStringLiteral("EPSG:3857") },
        { QStringLiteral("LONG-LAT"), cwCoordinateTransform::Wgs84 },
        { QStringLiteral("EUR79Z30"),
          QStringLiteral("+proj=utm +zone=30 +ellps=intl "
                         "+towgs84=-86,-98,-119,0,0,0,0 +no_defs") },
        { QStringLiteral("IJTSK"),
          QStringLiteral("+proj=krovak +ellps=bessel "
                         "+towgs84=570.8285,85.6769,462.842,4.9984,1.5867,5.2611,3.5623 "
                         "+no_defs") },
        { QStringLiteral("IJTSK03"),
          QStringLiteral("+proj=krovak +ellps=bessel "
                         "+towgs84=485.021,169.465,483.839,7.786342,4.397554,4.102655,0 "
                         "+no_defs") },
        { QStringLiteral("JTSK"),
          QStringLiteral("+proj=krovak +czech +ellps=bessel "
                         "+towgs84=570.8285,85.6769,462.842,4.9984,1.5867,5.2611,3.5623 "
                         "+no_defs") },
        { QStringLiteral("JTSK03"),
          QStringLiteral("+proj=krovak +czech +ellps=bessel "
                         "+towgs84=485.021,169.465,483.839,7.786342,4.397554,4.102655,0 "
                         "+no_defs") }
    };
    return systems;
}

} // namespace

namespace cwSurvexCS {

std::optional<Parsed> fromSurvexCS(const QString& csArgument)
{
    const QString text = csArgument.trimmed();
    if (text.isEmpty()) {
        return {};
    }

    //A quoted PROJ string is the only spelling whose payload keeps its case,
    //and read_string stops at the closing quote — survex has no escape syntax
    //inside one.
    static const QRegularExpression customExp(
        QStringLiteral("^CUSTOM\\s+\"([^\"]*)\"$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch custom = customExp.match(text);
    if (custom.hasMatch()) {
        const QString payload = custom.captured(1).trimmed();
        //An @ names a file that holds the system, so the system is in there
        //rather than on this line; taking the name for one would georeference
        //the cave on a string spelled like a file name.
        if (payload.startsWith(QLatin1Char('@'))) {
            return {};
        }
        return Parsed{ payload };
    }

    const QString upper = text.toUpper();

    static const QRegularExpression utmExp(QStringLiteral("^UTM(\\d+)([NS]?)$"));
    const QRegularExpressionMatch utm = utmExp.match(upper);
    if (utm.hasMatch()) {
        //A zone with no hemisphere letter is north, as cavern reads it. A zone
        //outside the grid comes back empty, which names nothing rather than an
        //EPSG code that would place the cave somewhere arbitrary.
        const bool north = utm.captured(2) != QLatin1String("S");
        const QString epsg = cwCoordinateTransform::utmZoneToEpsg(utm.captured(1).toInt(), north);
        if (epsg.isEmpty()) {
            return {};
        }
        return Parsed{ epsg };
    }

    static const QRegularExpression authorityExp(QStringLiteral("^(EPSG|ESRI):(\\d+)$"));
    const QRegularExpressionMatch authority = authorityExp.match(upper);
    if (authority.hasMatch()) {
        const int code = authority.captured(2).toInt();
        if (code < kAuthorityCodeLimit) {
            return Parsed{ QStringLiteral("%1:%2").arg(authority.captured(1)).arg(code) };
        }
        return {};
    }

    if (upper.startsWith(kOsgbPrefix)) {
        const QString projCS = osgbProjCS(upper.mid(kOsgbPrefix.size()));
        if (projCS.isEmpty()) {
            return {};
        }
        return Parsed{ projCS };
    }

    //Survex writes the pair latitude first here, and refuses the spelling
    //rather than telling PROJ about the order. CaveWhere transposes instead,
    //which places the cave where the file says it is.
    if (upper == QLatin1String("LAT-LONG")) {
        return Parsed{ cwCoordinateTransform::Wgs84, true };
    }

    const auto& systems = keywordSystems();
    const auto keyword = systems.constFind(upper);
    if (keyword != systems.constEnd()) {
        return Parsed{ keyword.value() };
    }

    return {};
}

SidecarWriter::SidecarWriter(const QString& survexPath)
{
    setSurvexFile(survexPath);
}

void SidecarWriter::setSurvexFile(const QString& survexPath)
{
    //The names already handed out belong to the old stem and the old directory,
    //so a retargeted writer starts over rather than pointing a new file's *cs
    //lines at sidecars named after the last one.
    m_fileNameByCS.clear();

    if (survexPath.isEmpty()) {
        m_directory.clear();
        m_stem.clear();
        return;
    }

    const QFileInfo info(survexPath);
    m_directory = info.absolutePath();
    m_stem = info.completeBaseName();
    //Survex quotes a name with a space, and has nothing to quote one with a
    //quote in it.
    m_stem.replace(QLatin1Char('"'), QLatin1Char('_'));
}

QString SidecarWriter::reserve(const QString& cs)
{
    if (m_stem.isEmpty()) {
        return QString();
    }

    const auto taken = m_fileNameByCS.constFind(cs);
    if (taken != m_fileNameByCS.constEnd()) {
        return taken.value();
    }

    //The first sidecar takes the .svx's own stem, so the pair reads as a pair —
    //and, for a single-system file, doubles as the ESRI .prj for the same data.
    const int index = m_fileNameByCS.size() + 1;
    const QString fileName = index == 1
        ? m_stem + kSidecarSuffix
        : m_stem + QLatin1Char('-') + QString::number(index) + kSidecarSuffix;
    m_fileNameByCS.insert(cs, fileName);
    return fileName;
}

QString SidecarWriter::write() const
{
    const QDir directory(m_directory);
    QString firstError;
    const auto failed = [&firstError](const QSaveFile& file) {
        if (firstError.isEmpty()) {
            firstError = QStringLiteral("Write coordinate system file %1: %2")
                             .arg(file.fileName(), file.errorString());
        }
    };

    for (auto it = m_fileNameByCS.constBegin(); it != m_fileNameByCS.constEnd(); ++it) {
        QSaveFile file(directory.filePath(it.value()));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            failed(file);
            continue;
        }

        {
            QTextStream stream(&file);
            //One line, no BOM: cavern joins the lines it reads with a space and
            //skips a BOM, but PROJ rejects a description that starts with one.
            stream << it.key() << Qt::endl;
        }

        if (!file.commit()) {
            failed(file);
        }
    }
    return firstError;
}

QString toSurvexCS(const QString& cs, SidecarWriter& sidecars)
{
    const QString trimmed = cs.trimmed();
    const auto isBareName = [](const QString& text) {
        if (text.isEmpty()) {
            return true;
        }
        return std::all_of(text.cbegin(), text.cend(), [](QChar c) {
            return c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_')
                || c == QLatin1Char(':') || c == QLatin1Char('.');
        });
    };

    if (isBareName(trimmed)) {
        return trimmed;
    }

    //Everything but a quote survives inline, which covers a PROJ string. A
    //system carrying one goes to a file — unless nothing said where files go,
    //and then the inline spelling at least fails loudly in cavern, which reads
    //it as far as that quote and says so.
    if (trimmed.contains(QLatin1Char('"'))) {
        const QString sidecar = sidecars.reserve(trimmed);
        if (!sidecar.isEmpty()) {
            return sidecarReference(sidecar);
        }
    }

    return QStringLiteral("CUSTOM \"%1\"").arg(trimmed);
}

void writeCsLine(QTextStream& stream, SidecarWriter& sidecars, const QString& cs, bool isOutput)
{
    stream << (isOutput ? "*cs out " : "*cs ") << toSurvexCS(cs, sidecars) << Qt::endl;
}

} // namespace cwSurvexCS
