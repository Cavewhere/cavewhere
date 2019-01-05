#ifndef CWCSVIMPORTERMANAGER_H
#define CWCSVIMPORTERMANAGER_H

//Qt includes
#include <QObject>
#include <QList>

//Our includes
class cwColumnNameModel;
class cwCSVImporterTask;
class cwErrorModel;
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

    cwColumnNameModel* availableColumnsModel() const;
    cwColumnNameModel* columnsModel() const;
    cwErrorModel* errorModel() const;

    void waitToFinish();

    QList<cwCave> caves() const;

signals:
    void distanceUnitChanged();
    void filenameChanged();
    void skipHeaderLinesChanged();
    void seperatorChanged();
    void useFromStationForLRUDChanged();
    void newTripOnEmptyLinesChanged();

private:
    cwCSVImporterSettings Settings;

    cwColumnNameModel* AvailableColumns; //!<
    cwColumnNameModel* ColumnsModel; //!<
    cwErrorModel* ErrorModel; //!<

    cwCSVImporterTask* Task;

private slots:
    void startParsing();
    void updateErrorModel();
};

/**
* @brief cwCSVImporterManager::filename
* @return
*/
inline QString cwCSVImporterManager::filename() const {
    return Settings.filename();
}

/**
*
*/
inline bool cwCSVImporterManager::useFromStationForLRUD() const {
    return Settings.useFromStationForLRUD();
}

/**
*
*/
inline int cwCSVImporterManager::skipHeaderLines() const {
    return Settings.skipHeaderLines();
}

/**
*
*/
inline QString cwCSVImporterManager::seperator() const {
    return Settings.seperator();
}

/**
* @brief cwCSVImporterManager::distanceUnit
* @return
*/
inline cwUnits::LengthUnit cwCSVImporterManager::distanceUnit() const {
    return Settings.distanceUnit();
}

/**
*
*/
inline cwColumnNameModel* cwCSVImporterManager::availableColumnsModel() const {
    return AvailableColumns;
}

/**
*
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

inline cwErrorModel* cwCSVImporterManager::errorModel() const {
    return ErrorModel;
}

#endif // CWCSVIMPORTERMANAGER_H
