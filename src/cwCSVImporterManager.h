#ifndef CWCSVIMPORTERMANAGER_H
#define CWCSVIMPORTERMANAGER_H

//Qt includes
#include <QObject>
#include <QList>

//Our includes
class cwColumnNameModel;
class cwCSVImporterTask;
class cwErrorModel;
class cwCSVLineModel;
#include "cwCSVImporterSettings.h"
#include "cwGlobals.h"
#include "cwCave.h"

/**
 * @brief The cwCSVImporterManager class
 *
 * The csv import manager, manages the cwCSVImporterTask.
 */
class CAVEWHERE_LIB_EXPORT cwCSVImporterManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged)
    Q_PROPERTY(int skipHeaderLines READ skipHeaderLines WRITE setSkipHeaderLines NOTIFY skipHeaderLinesChanged)
    Q_PROPERTY(QString seperator READ seperator WRITE setSeperator NOTIFY seperatorChanged)
    Q_PROPERTY(bool useFromStationForLRUD READ useFromStationForLRUD WRITE setUseFromStationForLRUD NOTIFY useFromStationForLRUDChanged)
    Q_PROPERTY(cwUnits::LengthUnit distanceUnit READ distanceUnit WRITE setDistanceUnit NOTIFY distanceUnitChanged)
    Q_PROPERTY(bool newTripOnEmptyLines READ newTripOnEmptyLines WRITE setNewTripOnEmptyLines NOTIFY newTripOnEmptyLinesChanged)
    Q_PROPERTY(cwColumnNameModel* availableColumnsModel READ availableColumnsModel CONSTANT)
    Q_PROPERTY(cwColumnNameModel* columnsModel READ columnsModel CONSTANT)
    Q_PROPERTY(cwErrorModel* errorModel READ errorModel CONSTANT)
    Q_PROPERTY(cwCSVLineModel* lineModel READ lineModel CONSTANT)
    Q_PROPERTY(int skipColumnId READ skipColumnId CONSTANT)
    Q_PROPERTY(int previewLines READ previewLines WRITE setPreviewLines NOTIFY previewLinesChanged)
    Q_PROPERTY(QString previewText READ previewText NOTIFY previewTextChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY lineCountChanged)

public:
    cwCSVImporterManager(QObject* parent = nullptr);
    virtual ~cwCSVImporterManager();

    QString filename() const;
    void setFilename(const QString& filename);

    int skipHeaderLines() const;
    void setSkipHeaderLines(int skipHeaderLines);

    QString seperator() const;
    void setSeperator(QString seperator);

    bool useFromStationForLRUD() const;
    void setUseFromStationForLRUD(bool useFromStationForLRUD);

    cwUnits::LengthUnit distanceUnit() const;
    void setDistanceUnit(cwUnits::LengthUnit distanceUnit);

    bool newTripOnEmptyLines() const;
    void setNewTripOnEmptyLines(bool newTripOnEmptyLines);

    int skipColumnId() const;

    int previewLines() const;
    void setPreviewLines(int previewLines);

    QString previewText() const;

    int lineCount() const;

    cwColumnNameModel* availableColumnsModel() const;
    cwColumnNameModel* columnsModel() const;
    cwErrorModel* errorModel() const;
    cwCSVLineModel* lineModel() const;

    void waitToFinish();

    QList<cwCave> caves() const;

signals:
    void distanceUnitChanged();
    void filenameChanged();
    void skipHeaderLinesChanged();
    void seperatorChanged();
    void useFromStationForLRUDChanged();
    void newTripOnEmptyLinesChanged();
    void previewLinesChanged();
    void previewTextChanged();
    void lineCountChanged();

private:
    cwCSVImporterSettings Settings;

    cwColumnNameModel* AvailableColumns; //!<
    cwColumnNameModel* ColumnsModel; //!<
    cwErrorModel* ErrorModel; //!<
    cwCSVLineModel* LineModel; //!<
    QString Text; //!<
    int LineCount = 0;

    cwCSVImporterTask* Task;

private slots:
    void startParsing();
    void updateErrorModel();
    void updateLineModel();
};

/**
* Returns the filename of the CSV that will be parsed
*/
inline QString cwCSVImporterManager::filename() const {
    return Settings.filename();
}

/**
* Return the number of lines that are skipped to ignore header lines
*/
inline bool cwCSVImporterManager::useFromStationForLRUD() const {
    return Settings.useFromStationForLRUD();
}

/**
* Returns the number of lines that will be skipped during parsing
*/
inline int cwCSVImporterManager::skipHeaderLines() const {
    return Settings.skipHeaderLines();
}

/**
* Returns the seperator that the parser uses.
*
* By default this is ","
*/
inline QString cwCSVImporterManager::seperator() const {
    return Settings.seperator();
}

/**
* Returns the distance unit the parser uses
*/
inline cwUnits::LengthUnit cwCSVImporterManager::distanceUnit() const {
    return Settings.distanceUnit();
}

/**
* Returns the model that holds all the available columns
*/
inline cwColumnNameModel* cwCSVImporterManager::availableColumnsModel() const {
    return AvailableColumns;
}

/**
*   Returns the model that holds the columns that are used to parse the data
*/
inline cwColumnNameModel* cwCSVImporterManager::columnsModel() const {
    return ColumnsModel;
}

/**
* Returns true if new lines will cause a new trip. False if all data is put into one
* trip.
*/
inline bool cwCSVImporterManager::newTripOnEmptyLines() const {
    return Settings.newTripOnEmptyLines();
}

/**
 * Returns the model that holds all the errors from the last CSV read
 */
inline cwErrorModel* cwCSVImporterManager::errorModel() const {
    return ErrorModel;
}

/**
* Stores all the lines in the loaded CSV file
*/
inline cwCSVLineModel* cwCSVImporterManager::lineModel() const {
    return LineModel;
}

/**
* Returns the number of preview lines that are stored
*/
inline int cwCSVImporterManager::previewLines() const {
    return Settings.previewLines();
}

/**
* Returns the text that was read from disk. The text contains only the number of lines found
* in previewLines.
*/
inline QString cwCSVImporterManager::previewText() const {
    return Text;
}

/**
* Returns the number of lines read in the csv file
*/
inline int cwCSVImporterManager::lineCount() const {
    return LineCount;
}

#endif // CWCSVIMPORTERMANAGER_H
