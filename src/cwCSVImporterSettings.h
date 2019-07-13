#ifndef CWCSVIMPORTERSETTINGS_H
#define CWCSVIMPORTERSETTINGS_H

//Qt includes
#include <QSharedDataPointer>

//Our includes
#include "cwColumnNameModel.h"
#include "cwUnits.h"


class cwCSVImporterSettingsData;

class cwCSVImporterSettings
{
public:
    cwCSVImporterSettings();
    cwCSVImporterSettings(const cwCSVImporterSettings &);
    cwCSVImporterSettings &operator=(const cwCSVImporterSettings &);
    ~cwCSVImporterSettings();

    QString filename() const;
    void setFilename(const QString& filename);

    int skipHeaderLines() const;
    void setSkipHeaderLines(int skipLines);

    QString seperator() const;
    void setSeperator(const QString& seperator);

    cwUnits::LengthUnit distanceUnit() const;
    void setDistanceUnit(cwUnits::LengthUnit units);

    QList<cwColumnName> columns() const;
    void setColumns(const QList<cwColumnName>& columns);

    bool useFromStationForLRUD() const;
    void setUseFromStationForLRUD(bool useFromStationForLRUD);

    bool newTripOnEmptyLines() const;
    void setNewTripOnEmptyLines(bool newTripOnEmptyLines);

    int previewLines() const;
    void setPreviewLines(int previewLines);

    QThread* outputThread() const;
    void setOutputThread(QThread* thread);

private:
    QSharedDataPointer<cwCSVImporterSettingsData> data;
};

#endif // CWCSVIMPORTERSETTINGS_H
