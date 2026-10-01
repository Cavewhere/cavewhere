/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSurvexExporterRegion.h"
#include "cwLinePlotErrorCodes.h"
#include "cwStationHandle.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterUtils.h"

#include <QFile>
#include <QTextStream>

namespace {

// Emit each region equate as a fully-qualified "*equate" at region scope,
// after every node's "*begin" block has been declared and closed. Cavern
// merges the tied stations to one coordinate; the decode then routes each
// qualified name back to its node.
void writeRegionEquates(QTextStream& stream, const cwCavingRegionData& region,
                        const cwSurvexExporterCaveTask::DriverTree& tree)
{
    cwSurvexExporterCaveTask::writeEquates(
        stream, region.equates,
        [&tree](const cwStationHandle& handle) { return tree.operand(handle); });
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

    // Labels are assigned here and nowhere else: a label is unique only among
    // its siblings, so the whole tree is needed to assign one, and the "*begin"
    // lines and the equate operands below both have to name the same ones.
    const cwSurvexExporterCaveTask::DriverTree tree(region.caves, cwScopeLabels(region),
                                                    options.excludedExternalOwners);

    cwSurvexExporterCaveTask nodeExporter;
    nodeExporter.setExportOptions(options);

    for (const cwCaveData& node : region.caves) {
        if (!nodeExporter.writeNode(stream, node, tree, outputCS)) {
            stream.flush();
            outputFile.close();
            QString message = nodeExporter.errors().join(QStringLiteral("; "));
            if (message.isEmpty()) {
                message = QStringLiteral("Failed to write cave %1").arg(node.name);
            }
            return Monad::ResultBase(message, static_cast<int>(LinePlotErrorCode::ExportFailed));
        }
    }

    // Region equates emit here, after every block is declared and closed, so
    // every fully-qualified operand is in scope.
    writeRegionEquates(stream, region, tree);

    stream << "*end" << Qt::endl;
    stream.flush();
    outputFile.close();

    return Monad::ResultBase();
}
