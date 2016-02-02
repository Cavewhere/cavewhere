/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCHIPDATAEXPORTCAVETASK_H
#define CWCHIPDATAEXPORTCAVETASK_H

//Our includes
#include "cwCaveExporterTask.h"
#include "cwUnits.h"
#include "cwStation.h"
#include "cwReadingStates.h"
class cwCave;
class cwTrip;
class cwTripCalibration;
class cwSurveyChunk;
class cwShot;

class cwChipdataExportCaveTask : public cwCaveExporterTask
{
    Q_OBJECT
public:
    explicit cwChipdataExportCaveTask(QObject *parent = 0);

    bool writeCave(QTextStream& stream, cwCave* cave);

signals:

public slots:

private:
    static const QString ChipdataNewLine;

    void writeTrip(QTextStream& stream, cwTrip* trip, QString caveName = QString());
    void writeHeader(QTextStream& stream, cwTrip* trip, QString caveName = QString());
    void writeDataFormat(QTextStream& stream, cwTrip* trip);
    void writeChunk(QTextStream& stream, cwSurveyChunk* chunk, bool feetAndInches);
    void writeShot(QTextStream &stream, cwTripCalibration *calibrations, bool feetAndInches, const cwStation &fromStation, const cwStation &toStation, cwShot shot);
    void writeLrudMeasurement(QTextStream &stream, cwDistanceStates::State state, double measurement, cwUnits::LengthUnit fromUnit, cwUnits::LengthUnit toUnit);
    static QString formatNumber(double number, int maxPrecision, int columnWidth);

    static cwUnits::LengthUnit outputDistanceUnit(cwTrip* trip);
    static bool isFeetAndInches(cwTrip* trip);

    template<typename P>
    static bool containsShot(cwTrip* trip, P predicate);
};

#endif // CWCHIPDATAEXPORTCAVETASK_H
