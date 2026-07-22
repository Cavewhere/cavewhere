/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterCaveTask.h"
#include "cwCavernNaming.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurvexExporterUtils.h"
#include "cwTrip.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStringList>

namespace {

// All driver-emitted *include paths are absolute with forward slashes
// (Survex accepts forward slashes on every platform) and double-quoted
// so sanitised cave/trip dir names with spaces survive. See
// plans/EXTERNAL_FILE_PHASE1.html §10.
QString driverIncludePathFor(const QString& attachmentDir, const QString& entryFile)
{
    const QString joined = QDir(attachmentDir).filePath(entryFile);
    return QFileInfo(joined).absoluteFilePath();
}

// Render one equate handle as a station name relative to the enclosing
// "*begin <cave>" block, so a bare "*equate" at cave scope resolves it.
// A NativeCave station sits directly at cave scope — the native trip's
// "*begin" is anonymous, adding no naming level — so the operand is the
// bare tail. A Trip station carries its trip's scope prefix
// (trip_<hex>. for an externally-attached trip, "<stationPrefix>." for a
// native-prefixed trip), matching how the trip loop names those stations.
// Returns an empty string when a handle names a container that is not this
// cave — a Trip handle whose trip is absent, or a NativeCave handle whose
// containerId is another cave — so the caller can drop the malformed equate
// rather than emit a name that silently mis-ties to a like-named local
// station (the same membership check cwCave::validate performs).
QString equateOperand(const cwStationHandle& handle, const cwCaveData& cave)
{
    switch (handle.scope()) {
    case cwStationHandle::NativeCave:
        if (handle.containerId() != cave.id) {
            return QString();
        }
        return handle.tail();
    case cwStationHandle::Trip:
        for (const cwTripData& trip : cave.trips) {
            if (trip.id == handle.containerId()) {
                return cwTrip::scopePrefix(trip) + handle.tail();
            }
        }
        return QString();
    }
    return QString();
}

} // namespace

cwSurvexExporterCaveTask::cwSurvexExporterCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
    TripExporter = new cwSurvexExporterTripTask(this);
    TripExporter->setParentSurvexExporter(this);

//    connect(TripExporter, SIGNAL(progressed(int)), SIGNAL(progressed(int)));
}

void cwSurvexExporterCaveTask::setExportOptions(const cwSurvexExporterRegion::Options& options)
{
    ExportOptions = options;
}

/**
  \brief Writes the cave data to the stream
  */
bool cwSurvexExporterCaveTask::writeCave(QTextStream& stream, const cwCaveData& cave, const QString& globalCS) {

    // checkData rejects caves with zero trips; a cave-level external
    // attachment is *defined* by having zero native trips, so the
    // externalCenterline branch runs before the gate. Trip-level
    // attachments still go through the gate because their cave has at
    // least one trip (the attached one).
    if (cave.externalCenterline.isEmpty() && !checkData(cave)) {
        if(isRunning()) {
            stop();
        }
        return false;
    }

    QString caveName = QString(cave.name).remove(" ");

    stream << "*begin " << caveName << " ;" << cave.name << Qt::endl << Qt::endl;

    // Cave-level external attachment: skip fix stations, calibrations,
    // and the trip loop. The included file carries its own *cs, *fix,
    // and *begin/*end structure; emitting our own would either silently
    // shadow theirs (no-op) or fight them (cavern error). Per master
    // plan §6, native + external are not mixed inside the same cave
    // body.
    if (!cave.externalCenterline.isEmpty()) {
        if (!writeExternalInclude(stream, cave.id,
                                  ExportOptions.caveAttachmentDirs,
                                  cave.externalCenterline.entryFile(),
                                  cave.name)) {
            return false;
        }
        stream << "*end " << caveName << " ; End of " << cave.name << Qt::endl;
        return true;
    }

    writeFixStations(stream, cave, globalCS);

    const auto declinationContext = cwSurvexExporterUtils::makeDeclinationContext(
        cave.fixStations, globalCS);

    //Haven't done anything
    TotalProgress = 0;

    //Go throug all the trips and save them
    for(int i = 0; i < cave.trips.size(); i++) {
        const cwTripData& tripData = cave.trips.at(i);

        // Trip-level external attachment: wrap the *include in
        // *begin trip_<uuid> / *end trip_<uuid> so the included file's
        // own *begin / *end stay isolated from this cave's namespace
        // (master plan §6, "Native vs. external trip wrapping is
        // asymmetric — on purpose"). Stations from the included file
        // resolve to cave_<uuid>.trip_<uuid>.<file-tail>.
        if (!tripData.externalCenterline.isEmpty()) {
            const QString tripLabel = cwCavernNaming::tripName(tripData.id);
            stream << "*begin " << tripLabel << " ; " << tripData.name << Qt::endl;
            // CaveWhere-resolved declination for files that carry none of
            // their own. Emitted before *include so it scopes over the
            // included legs; when the file owns declination the map has no
            // entry (see cwSurvexExporterRegion::Options).
            const auto declinationIt =
                ExportOptions.tripInjectedDeclinations.constFind(tripData.id);
            if (declinationIt != ExportOptions.tripInjectedDeclinations.constEnd()) {
                cwSurvexExporterUtils::writeCalibration(stream,
                                                        QStringLiteral("DECLINATION"),
                                                        declinationIt.value());
            }
            if (!writeExternalInclude(stream, tripData.id,
                                      ExportOptions.tripAttachmentDirs,
                                      tripData.externalCenterline.entryFile(),
                                      tripData.name)) {
                return false;
            }
            stream << "*end " << tripLabel << Qt::endl << Qt::endl;
            continue;
        }

        auto trip = std::make_unique<cwTrip>();
        trip->setData(tripData);
        TripExporter->writeTrip(stream, trip.get(), declinationContext);
        TotalProgress += trip->numberOfStations();
        stream << Qt::endl;
    }

    // Cave-level ties emit here, after every native station and every
    // *begin trip_<hex> sibling is declared, so both operands are in scope
    // (master plan §C4). Cavern merges the tied stations to one position;
    // the decode side then keys both cave-local names to that shared point.
    writeEquates(stream, cave);

    stream << "*end " << caveName << " ; End of " << cave.name << Qt::endl;

    return true;
}

/**
 * Emit a bare `*equate` line per cave-local tie. Operands render
 * cave-relative (see equateOperand): a NativeCave handle as its tail, a
 * Trip handle as "<scopePrefix><tail>". No `*export`/`*infer` is needed —
 * a cross-scope `*equate` inside the cave block resolves both sibling
 * scopes on its own. Structurally-invalid equates, and any whose Trip
 * handle names a trip absent from this cave, are dropped silently; station
 * *existence* is not checked here (deferred to the equate UX, master plan
 * §1.2), matching the fix-station path which also trusts the snapshot.
 */
void cwSurvexExporterCaveTask::writeEquates(QTextStream& stream, const cwCaveData& cave) const
{
    for (const cwEquate& equate : cave.equates) {
        if (!equate.isValid()) {
            continue;
        }

        const QList<cwStationHandle> handles = equate.stations();
        QStringList operands;
        operands.reserve(handles.size());
        for (const cwStationHandle& handle : handles) {
            const QString operand = equateOperand(handle, cave);
            if (operand.isEmpty()) {
                operands.clear();
                break;
            }
            operands.append(operand);
        }

        if (operands.size() < 2) {
            continue;
        }

        stream << "*equate " << operands.join(QLatin1Char(' ')) << Qt::endl;
    }
}

/**
 * Emit the *cs / *fix block for the cave. Validates the snapshot's
 * fixStations against the actual station names; rejected fixes are dropped
 * silently here (the user-facing exporter — cwSurvexExporterRule — runs
 * the same validation at snapshot time and surfaces errors on the cave).
 * Falls back to `*fix <firstStation> 0 0 0` when no valid fix exists so
 * un-fixed caves still resolve in cavern.
 */
void cwSurvexExporterCaveTask::writeFixStations(QTextStream &stream, const cwCaveData &cave, const QString& globalCS)
{
    QSet<QString> stationNamesLower;
    QString firstValidStation;
    for (const cwTripData& trip : cave.trips) {
        for (const cwSurveyChunkData& chunk : trip.chunks) {
            for (const cwStation& station : chunk.stations) {
                if (station.isValid()) {
                    stationNamesLower.insert(cwStation::canonicalKey(station.name()));
                    if (firstValidStation.isEmpty()) {
                        firstValidStation = station.name();
                    }
                }
            }
        }
    }

    QStringList errors;
    const QList<cwFixStation> validFixes = cwSurvexExporterUtils::validateFixStations(
        cave.fixStations, stationNamesLower, errors);
    for (const QString& message : std::as_const(errors)) {
        Errors.append(message);
    }

    cwSurvexExporterUtils::writeFixStations(stream, validFixes, firstValidStation, globalCS);
}

bool cwSurvexExporterCaveTask::writeExternalInclude(QTextStream& stream,
                                                    const QUuid& ownerId,
                                                    const QHash<QUuid, QString>& attachmentDirs,
                                                    const QString& entryFile,
                                                    const QString& ownerLabel)
{
    const auto it = attachmentDirs.constFind(ownerId);
    if (it == attachmentDirs.constEnd()) {
        Errors.append(QStringLiteral(
            "External centerline for '%1' has no resolved attachment directory "
            "(reconcile did not run before the driver export).").arg(ownerLabel));
        return false;
    }
    if (entryFile.isEmpty()) {
        Errors.append(QStringLiteral(
            "External centerline for '%1' has an empty entry file.").arg(ownerLabel));
        return false;
    }

    const QString absolutePath = driverIncludePathFor(it.value(), entryFile);

    // Survex *include syntax has no escape for an embedded double-quote
    // or newline, so a hostile entry file / attachment dir name would
    // either produce a syntactically broken driver or sneak in a second
    // *include token. Refuse before we emit; the user surfaces this as
    // an export error, not a confusing cavern parse failure.
    if (absolutePath.contains(QLatin1Char('"')) || absolutePath.contains(QLatin1Char('\n'))) {
        Errors.append(QStringLiteral(
            "External centerline path for '%1' contains characters that "
            "Survex *include cannot quote (\" or newline): %2")
            .arg(ownerLabel, absolutePath));
        return false;
    }

    stream << "*include \"" << absolutePath << "\"" << Qt::endl;
    return true;
}
