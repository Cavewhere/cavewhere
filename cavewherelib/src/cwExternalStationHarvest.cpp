/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwExternalStationHarvest.h"
#include "cwCavernRunner.h"
#include "cwLinePlotErrorCodes.h"
#include "cwStationPositionLookup.h"
#include "cwSurvex3DFileReader.h"

//Qt includes
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

Monad::Result<QStringList> harvestError(const QString& message, LinePlotErrorCode code)
{
    return Monad::Result<QStringList>(message, static_cast<int>(code));
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

Monad::Result<QStringList> cwExternalStationHarvest::harvest(const QString& entryFile)
{
    const QString absoluteEntry = QFileInfo(entryFile).absoluteFilePath();

    if(!QFileInfo::exists(absoluteEntry)) {
        return harvestError(QStringLiteral("External centerline entry file does not exist: %1")
                                .arg(absoluteEntry),
                            LinePlotErrorCode::ExportFailed);
    }

    //Survex *include has no escape for an embedded double quote or newline, so
    //such a path would break the driver or smuggle in a second *include token.
    //The same refusal cwSurvexExporterCaveTask::writeExternalInclude makes.
    if(absoluteEntry.contains(QLatin1Char('"')) || absoluteEntry.contains(QLatin1Char('\n'))) {
        return harvestError(QStringLiteral("External centerline path contains characters that "
                                           "Survex *include cannot quote (\" or newline): %1")
                                .arg(absoluteEntry),
                            LinePlotErrorCode::ExportFailed);
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

        //Exactly one line: no *fix, no *cs and no *begin. A *fix or a *cs would
        //defeat netskel's implicit origin fix, and a *begin would add a naming
        //level the region solve doesn't have.
        QTextStream stream(&driver);
        stream << "*include \"" << absoluteEntry << "\"" << Qt::endl;
    }

    const auto cavernResult =
        cwCavernRunner::run(driverPath, directory.filePath(QStringLiteral("harvest.3d")));
    if(cavernResult.hasError()) {
        return Monad::Result<QStringList>(
            withoutDriverNoise(cavernResult.errorMessage(), workingDirectory.path()),
            cavernResult.errorCode());
    }

    cwSurvex3DFileReader reader;
    const cwStationPositionLookup lookup =
        reader.readStationPositions(cavernResult.value().output3dPath);

    //cwStationPositionLookup canonicalizes on insert and keeps its stations in
    //a QMap, so its keys are already canonical and sorted.
    return QStringList(lookup.positions().keys());
}
