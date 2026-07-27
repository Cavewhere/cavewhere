/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSurvexExporterRegion.h"
#include "cwCavernNaming.h"
#include "cwLinePlotErrorCodes.h"
#include "cwStationHandle.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterUtils.h"

#include <QFile>
#include <QTextStream>

namespace {

// The cave that owns a handle's container: the cave itself for a NativeCave
// handle, or the cave holding the trip for a Trip handle. Region equates cross
// cave boundaries, so a handle names a container in *some* cave; return null
// when none matches so the caller drops the tie.
const cwCaveData* owningCave(const cwStationHandle& handle, const cwCavingRegionData& region)
{
    switch (handle.scope()) {
    case cwStationHandle::NativeCave:
        for (const cwCaveData& cave : region.caves) {
            if (cave.id == handle.containerId()) {
                return &cave;
            }
        }
        return nullptr;
    case cwStationHandle::Trip:
        for (const cwCaveData& cave : region.caves) {
            for (const cwTripData& trip : cave.trips) {
                if (trip.id == handle.containerId()) {
                    return &cave;
                }
            }
        }
        return nullptr;
    }
    return nullptr;
}

// Render a region equate handle fully qualified — the owning cave's own "*begin"
// label prepended to the cave-relative operand the cave exporter already knows
// how to build — so a bare "*equate" at region scope resolves it across both
// sibling cave blocks. Empty when the handle names no cave in the region.
// The label must be the one writeCave emitted, not a second derivation of it:
// an operand naming a scope the file never opened resolves to nothing.
QString qualifiedOperand(const cwStationHandle& handle,
                         const cwCavingRegionData& region,
                         const QHash<QUuid, QString>& caveLabels,
                         const QHash<QUuid, QHash<QUuid, QString>>& tripLabelsByCave)
{
    const cwCaveData* cave = owningCave(handle, region);
    if (cave == nullptr) {
        return QString();
    }
    const QString relative = cwSurvexExporterCaveTask::equateOperand(
        handle, *cave, tripLabelsByCave.value(cave->id));
    if (relative.isEmpty()) {
        return QString();
    }
    const QString caveLabel = caveLabels.value(cave->id);
    if (caveLabel.isEmpty()) {
        return QString();
    }
    return caveLabel + QLatin1Char('.') + relative;
}

// Emit each cross-cave tie as a fully-qualified "*equate" at region scope,
// after every cave's "*begin" sibling has been declared and closed. Cavern
// merges the tied stations to one coordinate; splitLookupByCave then routes
// each qualified name back to its cave, landing the tie as a position-alias in
// both.
void writeRegionEquates(QTextStream& stream, const cwCavingRegionData& region,
                        const QHash<QUuid, QString>& caveLabels)
{
    // Pooled once per cave rather than per operand: a label is unique among a
    // cave's trips, so answering one handle costs the whole cave either way.
    QHash<QUuid, QHash<QUuid, QString>> tripLabelsByCave;
    tripLabelsByCave.reserve(region.caves.size());
    for (const cwCaveData& cave : std::as_const(region.caves)) {
        tripLabelsByCave.insert(cave.id,
                                cwCavernNaming::scopeLabels(
                                    cwCavernNaming::scopeEntries(cave.trips)));
    }

    cwSurvexExporterCaveTask::writeEquates(
        stream, region.equates,
        [&region, &caveLabels, &tripLabelsByCave](const cwStationHandle& handle) {
            return qualifiedOperand(handle, region, caveLabels, tripLabelsByCave);
        });
}

} // namespace

Monad::ResultBase
cwSurvexExporterRegion::exportRegion(const cwCavingRegionData& region,
                                     const QString& outputPath,
                                     const Options& options)
{
    if (region.caves.isEmpty()) {
        return Monad::ResultBase(QStringLiteral("No caves to do loop closure"),
                                 static_cast<int>(LinePlotErrorCode::ExportFailed));
    }

    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        return Monad::ResultBase(QStringLiteral("Open file %1").arg(outputPath),
                                 static_cast<int>(LinePlotErrorCode::ExportFailed));
    }

    QTextStream stream(&outputFile);

    stream << "*begin  ;All the caves" << Qt::endl;

    const QString outputCS = cwSurvexExporterUtils::resolveOutputCS(region);
    if (!outputCS.isEmpty()) {
        stream << "*cs out " << outputCS << Qt::endl;
    }

    // Cave labels are decided here and nowhere else: uniqueness is a property of
    // the whole region, and the "*begin" lines and the equate operands below
    // both have to name the same scopes.
    Options labeledOptions = options;
    labeledOptions.caveLabels = cwCavernNaming::scopeLabels(
        cwCavernNaming::scopeEntries(region.caves));

    cwSurvexExporterCaveTask caveExporter;
    caveExporter.setExportOptions(labeledOptions);

    for (int i = 0; i < region.caves.size(); i++) {
        const cwCaveData& cave = region.caves.at(i);
        const bool good = caveExporter.writeCave(stream, cave, outputCS);
        stream << Qt::endl;

        if (!good) {
            stream.flush();
            outputFile.close();
            QString message = caveExporter.errors().join(QStringLiteral("; "));
            if (message.isEmpty()) {
                message = QStringLiteral("Failed to write cave %1").arg(cave.name);
            }
            return Monad::ResultBase(message, static_cast<int>(LinePlotErrorCode::ExportFailed));
        }
    }

    // Cross-cave ties emit here, after every cave's "*begin" sibling is declared
    // and closed, so both fully-qualified operands are in scope.
    writeRegionEquates(stream, region, labeledOptions.caveLabels);

    stream << "*end" << Qt::endl;
    stream.flush();
    outputFile.close();

    return Monad::ResultBase();
}
