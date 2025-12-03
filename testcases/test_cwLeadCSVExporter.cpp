//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwLeadCSVExporter.h"
#include "cwLeadModel.h"
#include "cwRootData.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwUnits.h"
#include "TestHelper.h"

//Qt includes
#include <QFile>
#include <QUrl>
#include <QUuid>
#include <QLocale>

//Std includes
#include <cmath>

TEST_CASE("cwLeadCSVExporter should export leads to CSV", "[cwLeadCSVExporter]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto datasetFile = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");
    project->loadFile(datasetFile);
    project->waitLoadToFinish();
    rootData->futureManagerModel()->waitForFinished();

    REQUIRE(project->cavingRegion()->caveCount() == 1);
    auto cave = project->cavingRegion()->cave(0);

    cwLeadModel leadModel;
    leadModel.setRegionTreeModel(rootData->regionTreeModel());
    leadModel.setCave(cave);

    const int rowCount = leadModel.rowCount();
    REQUIRE(rowCount > 0);

    cwLeadCSVExporter exporter;
    exporter.setModel(&leadModel);
    exporter.setLeadModel(&leadModel);
    exporter.setFutureManagerToken(rootData->futureManagerModel()->token());

    const QString exportPath = prependTempFolder(QString("lead-csv-export-%1.csv")
                                                 .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));

    exporter.exportToFile(QUrl::fromLocalFile(exportPath));
    rootData->futureManagerModel()->waitForFinished();

    QFile file(exportPath);
    REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text));

    const QStringList lines = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    REQUIRE(lines.size() == rowCount + 1);

    const QString referenceStation = leadModel.referanceStation();
    const QString distanceHeader = referenceStation.isEmpty()
            ? QStringLiteral("Distance to Reference Station (m)")
            : QStringLiteral("Distance to %1 (m)").arg(referenceStation);
    const QString expectedHeader = QStringLiteral("Completed,Nearest Station,Trip,Size Width,Size Height,Size Units,%1,Description")
            .arg(distanceHeader);
    CHECK(lines.first() == expectedHeader);

    auto escapeCsv = [](const QString& value) {
        QString escaped = value;
        escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        if(escaped.contains(',') || escaped.contains('"') || escaped.contains('\n')) {
            escaped.prepend('"');
            escaped.append('"');
        }
        return escaped;
    };

    auto numberString = [](double value) {
        if(!std::isfinite(value)) {
            return QString();
        }
        return QLocale::c().toString(value, 'g', 12);
    };

    for(int row = 0; row < rowCount; ++row) {
        QModelIndex index = leadModel.index(row, 0);
        const bool completed = leadModel.data(index, cwLeadModel::LeadCompleted).toBool();
        const QString nearestStation = leadModel.data(index, cwLeadModel::LeadNearestStation).toString();
        const QString tripName = leadModel.data(index, cwLeadModel::LeadTrip).toString();
        const QSizeF size = leadModel.data(index, cwLeadModel::LeadSize).toSizeF();
        const QString sizeUnits = cwUnits::unitName(static_cast<cwUnits::LengthUnit>(
                                                       leadModel.data(index, cwLeadModel::LeadUnits).toInt()));
        const double distance = leadModel.data(index, cwLeadModel::LeadDistanceToReferanceStation).toDouble();
        const QString description = leadModel.data(index, cwLeadModel::LeadDesciption).toString();

        QStringList expectedColumns;
        expectedColumns << (completed ? QStringLiteral("Yes") : QStringLiteral("No"));
        expectedColumns << escapeCsv(nearestStation);
        expectedColumns << escapeCsv(tripName);
        expectedColumns << numberString(size.width());
        expectedColumns << numberString(size.height());
        expectedColumns << escapeCsv(sizeUnits);
        expectedColumns << numberString(distance);
        expectedColumns << escapeCsv(description);

        CHECK(lines.at(row + 1) == expectedColumns.join(','));
    }
}
