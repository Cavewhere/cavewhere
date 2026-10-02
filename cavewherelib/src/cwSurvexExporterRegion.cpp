/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSurvexExporterRegion.h"
#include "cwLinePlotErrorCodes.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterUtils.h"
#include "cwSurvexCS.h"

#include <QFile>
#include <QTextStream>

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

    //One writer for the whole file: the region's *cs out and every fix row's own
    //*cs land in it, and each system that needs a sidecar takes its own file.
    //Only the working-frame file gets sidecars — the bundled cavern is its only
    //reader, and it is the only survex that reads an @ reference.
    const auto sidecarPolicy =
        options.outputCSPolicy == cwSurvexExporterUtils::OutputCSPolicy::WorkingFrame
            ? cwSurvexCS::SidecarPolicy::BundledCavern
            : cwSurvexCS::SidecarPolicy::OfficialSyntax;
    cwSurvexCS::SidecarWriter sidecars(outputPath, sidecarPolicy);

    const QString outputCS =
        cwSurvexExporterUtils::resolveOutputCS(region,
                                               region.geoReference.localCoordinateSystem,
                                               options.outputCSPolicy);
    if (!outputCS.isEmpty()) {
        cwSurvexCS::writeCsLine(stream, sidecars, outputCS, true);
    }

    // Labels are assigned here and nowhere else: a label is unique only among
    // its siblings, so the whole tree is needed to assign one, and the "*begin"
    // lines and the equate operands below both have to name the same ones.
    const cwSurvexExporterCaveTask::DriverTree tree(region.caves, cwScopeLabels(region),
                                                    options.excludedExternalOwners);

    cwSurvexExporterCaveTask nodeExporter;
    nodeExporter.setExportOptions(options);
    nodeExporter.setSidecarWriter(&sidecars);

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
    // every fully-qualified operand is in scope. Cavern merges the tied
    // stations to one coordinate; the decode then routes each qualified name
    // back to its node.
    cwSurvexExporterCaveTask::writeEquates(stream, region.equates, tree);

    stream << "*end" << Qt::endl;
    stream.flush();
    outputFile.close();

    //Last, so no sidecar outlives a file that failed halfway.
    const QString sidecarError = sidecars.write();
    if (!sidecarError.isEmpty()) {
        return Monad::ResultBase(sidecarError, static_cast<int>(LinePlotErrorCode::ExportFailed));
    }

    return Monad::ResultBase();
}
