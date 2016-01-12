/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOMPASSEXPORTCAVETASK_H
#define CWCOMPASSEXPORTCAVETASK_H

//Our includes
#include "cwCaveExporterTask.h"
#include "cwUnits.h"
#include "cwStation.h"
#include "cwReadingStates.h"
#include "cwGlobals.h"
class cwCave;
class cwTrip;
class cwTripCalibration;
class cwSurveyChunk;
class cwShot;

class CAVEWHERE_LIB_EXPORT cwCompassExportCaveTask : public cwCaveExporterTask
{
    Q_OBJECT
public:
    explicit cwCompassExportCaveTask(QObject *parent = 0);

    bool writeCave(QTextStream& stream, cwCave* cave);

signals:

public slots:

private:
    enum StationLRUDField {
        Left,
        Right,
        Up,
        Down
    };

    enum ShotField {
        Compass,
        Clino,
        BackCompass,
        BackClino
    };


    static const char* CompassNewLine;

    void writeTrip(QTextStream& stream, cwTrip* trip);
    void writeHeader(QTextStream& stream, cwTrip* trip);
    void writeDataFormat(QTextStream& stream, cwTripCalibration* calibrations);
    void writeDeclination(QTextStream& stream, cwTripCalibration* calibrations);
    void writeCorrections(QTextStream& stream, cwTripCalibration* calibrations);
    void writeData(QTextStream& stream, QString fieldName, int fieldLength, QString data);
    void writeChunk(QTextStream& stream, cwSurveyChunk* chunk);

    double convertField(cwStation station, StationLRUDField field, cwUnits::LengthUnit unit);
    double convertField(cwTripCalibration *trip, cwShot shot, ShotField field);
    QString formatDouble(double value);

    bool convertFromDownUp(cwClinoStates::State clinoReading, double* value);
    QString surveyTeam(cwTrip* trip);

    void writeInvalidTripData(QTextStream& stream, cwTrip *trip);
    void writeShot(QTextStream &stream, cwTripCalibration *calibrations, const cwStation &fromStation, const cwStation &toStation, cwShot shot, bool LRUDShotOnly);

};

#endif // CWCOMPASSEXPORTCAVETASK_H
