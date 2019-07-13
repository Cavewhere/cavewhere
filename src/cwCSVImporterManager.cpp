//Our includes
#include "cwCSVImporterManager.h"
#include "cwCSVImporterTask.h"
#include "cwColumnNameModel.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwCSVLineModel.h"

cwCSVImporterManager::cwCSVImporterManager(QObject* parent) :
    QObject(parent),
    AvailableColumns(new cwColumnNameModel(this)),
    ColumnsModel(new cwColumnNameModel(this)),
    ErrorModel(new cwErrorModel(this)),
    LineModel(new cwCSVLineModel(this)),
    Task(new cwCSVImporterTask)
{
    QList<cwColumnName> availableColumns {
        {"Compass Backsight", cwCSVImporterTask::CompassBackSight},
        {"Clino Backsight", cwCSVImporterTask::ClinoBackSight},
        {"Left", cwCSVImporterTask::Left},
        {"Right", cwCSVImporterTask::Right},
        {"Up", cwCSVImporterTask::Up},
        {"Down", cwCSVImporterTask::Down},
        {"Skip", cwCSVImporterTask::Skip}
    };

    QList<cwColumnName> columns {
        {"From", cwCSVImporterTask::FromStation},
        {"To", cwCSVImporterTask::ToStation},
        {"Length", cwCSVImporterTask::Length},
        {"Compass", cwCSVImporterTask::CompassFrontSight},
        {"Clino", cwCSVImporterTask::ClinoFrontSight}
    };

    AvailableColumns->append(availableColumns);
    ColumnsModel->append(columns);

    LineModel->setColumnModel(ColumnsModel);

    //This allows caves to be moved
    Settings.setOutputThread(thread());

    //For restarts
    connect(Task, &cwTask::shouldRerun, this, &cwCSVImporterManager::startParsing);
    connect(Task, &cwTask::finished, this, &cwCSVImporterManager::updateModel);
    connect(ColumnsModel, &QAbstractItemModel::modelReset, this, &cwCSVImporterManager::startParsing);
    connect(ColumnsModel, &QAbstractItemModel::rowsRemoved, this, &cwCSVImporterManager::startParsing);
    connect(ColumnsModel, &QAbstractItemModel::rowsInserted, this, &cwCSVImporterManager::startParsing);
    connect(ColumnsModel, &QAbstractItemModel::rowsMoved, this, &cwCSVImporterManager::startParsing);
}

cwCSVImporterManager::~cwCSVImporterManager()
{
    Task->stop();
    Task->waitToFinish();
    Task->deleteLater();
}

/**
* @brief cwCSVImporterManager::setSkipHeaderLines
* @param skipHeaderLines
*/
void cwCSVImporterManager::setSkipHeaderLines(int skipHeaderLines) {
    if(this->skipHeaderLines() != skipHeaderLines) {
        Settings.setSkipHeaderLines(skipHeaderLines);
        emit skipHeaderLinesChanged();
        startParsing();
    }
}

/**
* @brief cwCSVImporterManager::setSeperator
* @param seperator
*/
void cwCSVImporterManager::setSeperator(QString seperator) {
    if(this->seperator() != seperator) {
        Settings.setSeperator(seperator);
        emit seperatorChanged();
        startParsing();
    }
}

/**
* @brief class::setUseFromStationForLRUD
* @param useFromStationForLRUD
*/
void cwCSVImporterManager::setUseFromStationForLRUD(bool useFromStationForLRUD) {
    if(this->useFromStationForLRUD() != useFromStationForLRUD) {
        Settings.setUseFromStationForLRUD(useFromStationForLRUD);
        emit useFromStationForLRUDChanged();
        startParsing();
    }
}

/**
* @brief cwCSVImporterManager::setFilename
* @param filename
*/
void cwCSVImporterManager::setFilename(const QString& filename) {
    QString localFilename = cwGlobals::convertFromURL(filename);
    if(this->filename() != localFilename) {
        Settings.setFilename(localFilename);
        emit filenameChanged();
    }
    //Start parsing even if it's the same file, this reloads the file
    startParsing();
}

/**
* @brief cwCSVImporterManager::setVariable
* @param distanceUnit
*/
void cwCSVImporterManager::setDistanceUnit(cwUnits::LengthUnit distanceUnit) {
    if(this->distanceUnit() != distanceUnit) {
        Settings.setDistanceUnit(distanceUnit);
        emit distanceUnitChanged();
        startParsing();
    }
}

/**
 * Waits for the parser to finish
 */
void cwCSVImporterManager::waitToFinish()
{
    Task->waitToFinish();
}

/**
 * Returns all the caves that were parsed. Caves will be avaliable when the underlying task
 * finishes
 */
QList<cwCave*> cwCSVImporterManager::caves() const
{
    if(Task->isReady()) {
        return Task->output().caves;
    }
    return QList<cwCave*>();
}

/**
 * Delete's all the old caves that or owned by the manager
 */
void cwCSVImporterManager::deleteOldCaves()
{
    for(auto cave : LastCaves) {
        if(cave->parent() == this) {
            cave->deleteLater();
        }
    }
}

/**
 * Starts parsing csv data
 */
void cwCSVImporterManager::startParsing()
{
    if(Task->isReady()) {
        Settings.setColumns(ColumnsModel->toList());
        Task->setSettings(Settings);
        Task->start();
    } else {
        Task->restart();
    }
}

/**
 * Updates the line model with new data
 */
void cwCSVImporterManager::updateModel()
{
    if(Task->isReady()) {
        ErrorModel->errors()->clear();
        ErrorModel->errors()->append(Task->output().errors);

        LineModel->setLines(Task->output().lines);

        Text = Task->output().text;
        emit previewTextChanged();

        LineCount = Task->output().lineCount;
        emit lineCountChanged();

        deleteOldCaves();
        LastCaves = Task->output().caves;
        for(auto cave : LastCaves) {
            cave->setParent(this);
        }
    }
}

/**
* Sets true to treat empty lines as a new trip. Otherwise, all shots will be added to
* a single trip.
*/
void cwCSVImporterManager::setNewTripOnEmptyLines(bool newTripOnEmptyLines) {
    if(this->newTripOnEmptyLines() != newTripOnEmptyLines) {
        Settings.setNewTripOnEmptyLines(newTripOnEmptyLines);
        emit newTripOnEmptyLinesChanged();
        startParsing();
    }
}

/**
* Returns the id for the skip column.
*/
 int cwCSVImporterManager::skipColumnId() const {
    return cwCSVImporterTask::Skip;
}

/**
* Sets the number of preview lines for the CSV parser
*
* If the file is longer than preview lines, those lines won't be added. Setting preview lines
* to max int size will give you all the lines in the file. By default preview lines in 20
*/
 void cwCSVImporterManager::setPreviewLines(int previewLines) {
     if(this->previewLines() != previewLines) {
         Settings.setPreviewLines(previewLines);
         emit previewLinesChanged();
         startParsing();
     }
 }
