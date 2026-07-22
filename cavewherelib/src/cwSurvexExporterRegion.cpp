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

// Render a region equate handle fully qualified — "cave_<hex>." prepended to
// the cave-relative operand the cave exporter already knows how to build — so a
// bare "*equate" at region scope resolves it across both sibling cave blocks.
// Empty when the handle names no cave in the region.
QString qualifiedOperand(const cwStationHandle& handle, const cwCavingRegionData& region)
{
    const cwCaveData* cave = owningCave(handle, region);
    if (cave == nullptr) {
        return QString();
    }
    const QString relative = cwSurvexExporterCaveTask::equateOperand(handle, *cave);
    if (relative.isEmpty()) {
        return QString();
    }
    return cwCavernNaming::caveScopePrefix(cave->id) + relative;
}

// Emit each cross-cave tie as a fully-qualified "*equate" at region scope,
// after every "*begin cave_<hex>" sibling has been declared and closed. Cavern
// merges the tied stations to one coordinate; splitLookupByCave then routes
// each qualified name back to its cave, landing the tie as a position-alias in
// both.
void writeRegionEquates(QTextStream& stream, const cwCavingRegionData& region)
{
    cwSurvexExporterCaveTask::writeEquates(
        stream, region.equates,
        [&region](const cwStationHandle& handle) {
            return qualifiedOperand(handle, region);
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

    cwSurvexExporterCaveTask caveExporter;
    caveExporter.setExportOptions(options);

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

    // Cross-cave ties emit here, after every "*begin cave_<hex>" sibling is
    // declared and closed, so both fully-qualified operands are in scope.
    writeRegionEquates(stream, region);

    stream << "*end" << Qt::endl;
    stream.flush();
    outputFile.close();

    return Monad::ResultBase();
}
