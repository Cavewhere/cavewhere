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

    stream << "*end" << Qt::endl;
    stream.flush();
    outputFile.close();

    return Monad::ResultBase();
}
