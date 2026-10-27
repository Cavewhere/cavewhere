/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwExternalStationHarvest.h"
#include "cwCavernRunner.h"
#include "cwCoordinateTransform.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineScanner.h"
#include "cwLinePlotErrorCodes.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurvex3DFileReader.h"
#include "cwSurvexExporterUtils.h"

//Qt includes
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

Monad::Result<cwExternalStationHarvest::Stations> harvestError(const QString& message,
                                                                LinePlotErrorCode code)
{
    return Monad::Result<cwExternalStationHarvest::Stations>(message, static_cast<int>(code));
}

/**
 * Strips the harvest's own throwaway driver out of cavern's log. That path is
 * deleted before anyone reads the message and differs on every run, so leaving
 * it in would make an otherwise identical failure look like a new one.
 *
 * Only the "<file>:<line>:<column>: " location prefix goes, not the whole line:
 * cavern blames the driver for anything it hits opening the entry itself
 * ("harvest.svx:1: error: Couldn't open file ..."), so dropping those lines
 * outright left the caller with an echoed source line and a caret and no
 * reason. A line that still names the driver after the prefix comes off has
 * nothing worth keeping, and is dropped as before so no run-varying path can
 * reach the message.
 */
QString withoutDriverNoise(const QString& logText, const QString& workingDirectory)
{
    const QString nativeWorkingDirectory = QDir::toNativeSeparators(workingDirectory);

    const auto namesDriver = [&](const QString& text) {
        return text.contains(workingDirectory) || text.contains(nativeWorkingDirectory);
    };

    //"<path>:<line>:<column>: ", with the line and column both optional —
    //cavern omits them for a diagnostic it cannot place.
    static const QRegularExpression locationPrefix(
        QStringLiteral(R"(^\s*\S+?\.svx(?::\d+)*:\s*)"));

    QStringList kept;
    const QStringList lines = logText.split(QLatin1Char('\n'));
    kept.reserve(lines.size());
    for(const QString& line : lines) {
        if(!namesDriver(line)) {
            kept.append(line);
            continue;
        }

        //"In file included from <driver>:1:" introduces the diagnosis rather
        //than carrying it, so there is nothing to salvage.
        if(line.trimmed().startsWith(QStringLiteral("In file included from"))) {
            continue;
        }

        QString withoutLocation = line;
        withoutLocation.remove(locationPrefix);
        if(namesDriver(withoutLocation) || withoutLocation.trimmed().isEmpty()) {
            continue;
        }
        kept.append(withoutLocation);
    }
    return kept.join(QLatin1Char('\n'));
}

} // namespace

namespace {

/**
 * The system the driver puts in force around \a absoluteEntry, or empty. Only
 * a Survex closure that fixes a station gets one: cavern refuses a fix under a
 * *cs with no *cs out (error 437), and reads a bare fix in the *cs out the file
 * names. That is the file's own *cs out, else the system of its first fix that
 * names one, made projected since cavern outputs only in a projected system.
 * The harvest reads names, not positions, so any such system serves.
 *
 * A .mak or .wpj sets its own output when none is in force, and a closure that
 * fixes nothing keeps cavern's implicit origin fix, which any *cs defeats.
 */
QString outputSystemFor(const QString& absoluteEntry)
{
    if(cwExternalCenterlineScanner::formatFor(absoluteEntry)
       != cwExternalCenterlineScanner::Format::Survex) {
        return QString();
    }

    const auto scan = cwExternalCenterlineScanner::scan(absoluteEntry);
    if(scan.hasError() || scan.value().fixes.isEmpty()) {
        return QString();
    }
    if(!scan.value().outputCoordinateSystem.isEmpty()) {
        return scan.value().outputCoordinateSystem;
    }

    for(const cwExternalCenterlineScanner::ScannedFix& scanned : scan.value().fixes) {
        if(!scanned.hasCoordinateSystem()) {
            continue;
        }
        cwAttachedFix fix;
        fix.coordinate = scanned.coordinate;
        fix.coordinateSystem = scanned.coordinateSystem;
        fix.placementSystem = scanned.placementSystem;
        const QString system = fix.solveSystem();
        const auto placement = fix.placementIn(system);
        if(!placement.has_value()) {
            continue;
        }
        if(!cwCoordinateTransform::isGeographic(placement->cs)) {
            return system;
        }
        const QString projected =
            cwCoordinateTransform::deriveProjectedOutputCS(placement->cs, placement->point);
        if(!projected.isEmpty()) {
            return projected;
        }
    }
    return QString();
}

Monad::Result<QStringList> namesOf(const Monad::Result<cwExternalStationHarvest::Stations>& result)
{
    if(result.hasError()) {
        return Monad::Result<QStringList>(result.errorMessage(), result.errorCode());
    }
    return result.value().names;
}

Monad::Result<cwExternalStationHarvest::Stations> run(const QString& entryFile,
                                                      const QStringList& seedStations,
                                                      const std::function<bool()>& isCanceled)
{
    const QString absoluteEntry = QFileInfo(entryFile).absoluteFilePath();

    if(!QFileInfo::exists(absoluteEntry)) {
        return harvestError(QStringLiteral("External centerline entry file does not exist: %1")
                                .arg(absoluteEntry),
                            LinePlotErrorCode::ValidationFailed);
    }

    if(!cwSurvexExporterUtils::isQuotableIncludePath(absoluteEntry)) {
        return harvestError(QStringLiteral("External centerline path contains characters that "
                                           "Survex *include cannot quote (\", newline, carriage "
                                           "return or Ctrl-Z): %1")
                                .arg(absoluteEntry),
                            LinePlotErrorCode::ValidationFailed);
    }

    QTemporaryDir workingDirectory;
    if(!workingDirectory.isValid()) {
        return harvestError(QStringLiteral("Couldn't create a working directory to read station "
                                           "names from %1: %2")
                                .arg(absoluteEntry, workingDirectory.errorString()),
                            LinePlotErrorCode::ExportFailed);
    }

    const QDir directory(workingDirectory.path());
    const QString driverPath = directory.filePath(QStringLiteral("harvest.svx"));

    {
        QFile driver(driverPath);
        if(!driver.open(QFile::WriteOnly | QFile::Text)) {
            return harvestError(QStringLiteral("Couldn't write %1: %2")
                                    .arg(driverPath, driver.errorString()),
                                LinePlotErrorCode::ExportFailed);
        }

        //No *cs unless the closure fixes a station that needs one (see
        //outputSystemFor), and no named *begin: a *cs would defeat netskel's
        //implicit origin fix, and a named *begin would add a naming level the
        //region solve doesn't have. The system comes before the seeds, which
        //cavern refuses once a *cs follows them. A *fix takes the place of the
        //implicit fix only for the components the caller seeds. The seeds are
        //spelled as cavern reported them, so they keep their case — Compass
        //names are case-sensitive — inside an unnamed block that keeps *case
        //from reaching the include.
        QTextStream stream(&driver);
        const QString outputSystem = outputSystemFor(absoluteEntry);
        if(!outputSystem.isEmpty()) {
            cwSurvexExporterUtils::writeFileOutputSystem(stream, outputSystem);
        }
        if(!seedStations.isEmpty()) {
            stream << "*begin" << Qt::endl << "*case preserve" << Qt::endl;
            for(const QString& station : seedStations) {
                stream << "*fix " << station << " 0 0 0" << Qt::endl;
            }
            stream << "*end" << Qt::endl;
        }
        stream << "*include \"" << absoluteEntry << "\"" << Qt::endl;

        //A driver that only partly reached the disk is still valid Survex — an
        //empty or truncated one just includes nothing — so without this the
        //user's entry file gets blamed for the "No survey data" a full disk
        //caused.
        if(stream.status() != QTextStream::Ok || !driver.flush()) {
            return harvestError(QStringLiteral("Couldn't write %1: %2")
                                    .arg(driverPath, driver.errorString()),
                                LinePlotErrorCode::ExportFailed);
        }
    }

    const auto cavernResult = cwCavernRunner::run(
        driverPath, directory.filePath(QStringLiteral("harvest.3d")), isCanceled);
    if(cavernResult.hasError()) {
        return Monad::Result<cwExternalStationHarvest::Stations>(
            withoutDriverNoise(cavernResult.errorMessage(), workingDirectory.path()),
            cavernResult.errorCode());
    }

    cwSurvex3DFileReader reader;
    const cwSurvex3DFileReader::NetworkAndLookup read =
        reader.readNetworkAndLookup(cavernResult.value().output3dPath);
    const cwStationPositionLookup& lookup = read.lookup;

    //cwSurvex3DFileReader only warns when it can't read a .3d and hands back an
    //empty lookup, which would leave this function reporting success with no
    //names — the one answer it must never give, since a caller can't tell it
    //apart from a file that declares nothing. Cavern never writes an empty .3d:
    //a run with no survey data is fatalerror 43 (netskel.c:93), so an empty
    //lookup after a successful run means the read failed.
    if(lookup.positions().isEmpty()) {
        return harvestError(QStringLiteral("Couldn't read the station names Survex solved for %1")
                                .arg(absoluteEntry),
                            LinePlotErrorCode::ParseFailed);
    }

    //Spellings, not keys: the region solve wraps this file in the cave's and
    //trip's *begin blocks and derives each station's key from wrapper +
    //spelling, so the harvest keeps the spelling cavern wrote.
    cwExternalStationHarvest::Stations stations;
    stations.names = read.spellings.values();
    stations.names.sort();

    QStringList fixedSpellings;
    fixedSpellings.reserve(read.fixedStations.size());
    for(const QString& fixed : read.fixedStations) {
        fixedSpellings.append(read.spellings.value(fixed));
    }
    // A seed's *fix is the driver's, not the file's.
    QSet<QString> seedSpellings;
    for(const QString& seed : seedStations) {
        seedSpellings.insert(cwStation::spellingIn(seed, fixedSpellings));
    }
    for(const QString& fixed : std::as_const(fixedSpellings)) {
        if(!seedSpellings.contains(fixed)) {
            stations.fixedNames.append(fixed);
        }
    }
    return stations;
}

} // namespace

Monad::Result<QStringList> cwExternalStationHarvest::harvest(const QString& entryFile)
{
    return namesOf(run(entryFile, {}, {}));
}

Monad::Result<cwExternalStationHarvest::Stations>
cwExternalStationHarvest::harvestWithFixes(const QString& entryFile,
                                           const std::function<bool()>& isCanceled)
{
    return run(entryFile, {}, isCanceled);
}

Monad::Result<QStringList> cwExternalStationHarvest::harvestComponents(const QString& entryFile,
                                                                       const QStringList& seedStations,
                                                                       const std::function<bool()>& isCanceled)
{
    return namesOf(run(entryFile, seedStations, isCanceled));
}
