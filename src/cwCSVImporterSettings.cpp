#include "cwCSVImporterSettings.h"
#include "cwUnits.h"

class cwCSVImporterSettingsData : public QSharedData
{
public:
    QString Filename;
    int SkipHeaderLines = 1;
    QString Seperator = ",";
    cwUnits::LengthUnit DistanceUnit = cwUnits::Meters;
    QList<cwColumnName> Columns;
    bool UseFromStationForLRUD = true;
    bool NewTripOnEmptyLines = false;
    int PreviewLines = 20;
    QThread* OutputThread = nullptr;
};

cwCSVImporterSettings::cwCSVImporterSettings() : data(new cwCSVImporterSettingsData)
{

}

cwCSVImporterSettings::cwCSVImporterSettings(const cwCSVImporterSettings &rhs) : data(rhs.data)
{

}

cwCSVImporterSettings &cwCSVImporterSettings::operator=(const cwCSVImporterSettings &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwCSVImporterSettings::~cwCSVImporterSettings()
{

}

QString cwCSVImporterSettings::filename() const
{
    return data->Filename;
}

void cwCSVImporterSettings::setFilename(const QString &filename)
{
    data->Filename = filename;
}

int cwCSVImporterSettings::skipHeaderLines() const
{
    return data->SkipHeaderLines;
}

void cwCSVImporterSettings::setSkipHeaderLines(int skipLines)
{
    data->SkipHeaderLines = skipLines;
}

QString cwCSVImporterSettings::seperator() const
{
    return data->Seperator;
}

void cwCSVImporterSettings::setSeperator(const QString &seperator)
{
    data->Seperator = seperator;
}

cwUnits::LengthUnit cwCSVImporterSettings::distanceUnit() const
{
    return data->DistanceUnit;
}

void cwCSVImporterSettings::setDistanceUnit(cwUnits::LengthUnit units)
{
    data->DistanceUnit = units;
}

QList<cwColumnName> cwCSVImporterSettings::columns() const
{
    return data->Columns;
}

void cwCSVImporterSettings::setColumns(const QList<cwColumnName> &columns)
{
    data->Columns = columns;
}

bool cwCSVImporterSettings::useFromStationForLRUD() const
{
    return data->UseFromStationForLRUD;
}

void cwCSVImporterSettings::setUseFromStationForLRUD(bool useFromStationForLRUD)
{
    data->UseFromStationForLRUD = useFromStationForLRUD;
}

bool cwCSVImporterSettings::newTripOnEmptyLines() const
{
    return data->NewTripOnEmptyLines;
}

void cwCSVImporterSettings::setNewTripOnEmptyLines(bool newTripOnEmptyLines)
{
    data->NewTripOnEmptyLines = newTripOnEmptyLines;
}

int cwCSVImporterSettings::previewLines() const
{
    return data->PreviewLines;
}

void cwCSVImporterSettings::setPreviewLines(int previewLines)
{
    data->PreviewLines = previewLines;
}

QThread *cwCSVImporterSettings::outputThread() const
{
    return data->OutputThread;
}

void cwCSVImporterSettings::setOutputThread(QThread *thread)
{
     data->OutputThread = thread;
}
