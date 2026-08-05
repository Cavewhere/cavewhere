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
#include "cwTripData.h"
#include "cwUnits.h"
#include "cwDistanceReading.h"
#include "cwClinoReading.h"
#include "cwCompassReading.h"
#include "cwFixStation.h"
#include "CaveWhereLibExport.h"
class cwTrip;
class cwSurveyChunk;
class cwTripCalibration;
class cwTeam;

//Qt includes
class QTextStream;


class CAVEWHERE_LIB_EXPORT cwSurvexExporterTripTask : public cwExporterTask
{
    Q_OBJECT

public:
    explicit cwSurvexExporterTripTask(QObject *parent = 0);

    void setData(const cwTripData& trip);

    //! The fix stations of the cave this trip belongs to. A trip exported on
    //! its own writes no *fix — fixes belong to the cave — but it still reads
    //! them for the one location `*declination auto` needs and for the *cs out
    //! that location requires.
    void setCaveFixStations(const QList<cwFixStation>& fixStations);

    //! \a autoDeclinationInScope says an enclosing block already carries
    //! `*declination auto`, so a trip with auto on writes no declination line
    //! and inherits it.
    void writeTrip(QTextStream& stream,
                   cwTrip* trip,
                   bool autoDeclinationInScope = false);


signals:

protected:
    virtual void runTask();

public slots:

private:
    cwTripData Trip;
    QList<cwFixStation> CaveFixStations;
    inline static const int TextPadding = -11;

    void writeChunk(QTextStream& stream, bool hasFrontSight, bool hasBackSight, cwSurveyChunk* chunk);
    void writeCalibrations(QTextStream& stream,
                           cwTripCalibration* calibrations,
                           bool autoDeclinationInScope);
    void writeLengthUnits(QTextStream& stream, cwUnits::LengthUnit unit);
    void writeShotData(QTextStream& stream, const cwTrip* trip);
    void writeLRUDData(QTextStream& stream, const cwTrip *trip);
    void writeTeamData(QTextStream& stream, const cwTeam *trip);
    void writeDate(QTextStream& stream, QDate date);

    QString toSupportedLength(const cwTripCalibration* calibration, const cwDistanceReading& reading) const;
    QString compassToString(const cwCompassReading &reading) const;
    QString clinoToString(const cwClinoReading& reading) const;
};

#endif // CWSURVEXEXPORTERTRIPTASK_H
