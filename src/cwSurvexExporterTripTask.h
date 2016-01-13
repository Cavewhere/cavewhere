/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXEXPORTERTRIPTASK_H
#define CWSURVEXEXPORTERTRIPTASK_H

//Our includes
#include "cwExporterTask.h"
#include "cwUnits.h"
#include "cwReadingStates.h"
class cwTrip;
class cwSurveyChunk;
class cwTripCalibration;
class cwTeam;

//Qt includes
class QTextStream;


class cwSurvexExporterTripTask : public cwExporterTask
{
    Q_OBJECT

public:
    explicit cwSurvexExporterTripTask(QObject *parent = 0);

    void setData(const cwTrip& trip);

    void writeTrip(QTextStream& stream, cwTrip* trip);


signals:

protected:
    virtual void runTask();

public slots:

private:
    cwTrip* Trip;
    static const int TextPadding;

    void writeChunk(QTextStream& stream, bool hasFrontSight, bool hasBackSight, cwSurveyChunk* chunk);
    void writeCalibrations(QTextStream& stream, cwTripCalibration* calibrations);
    void writeCalibration(QTextStream& stream, QString type, double value, double scale = 1.0);
    void writeLengthUnits(QTextStream& stream, cwUnits::LengthUnit unit);
    void writeShotData(QTextStream& stream, cwTrip* trip);
    void writeLRUDData(QTextStream& stream, cwTrip* trip);
    void writeTeamData(QTextStream& stream, cwTeam *trip);
    void writeDate(QTextStream& stream, QDate date);

    QString toSupportedLength(double length, cwDistanceStates::State) const;
    QString compassToString(double compass, cwCompassStates::State) const;
    QString clinoToString(double clino, cwClinoStates::State) const;
};

#endif // CWSURVEXEXPORTERTRIPTASK_H
