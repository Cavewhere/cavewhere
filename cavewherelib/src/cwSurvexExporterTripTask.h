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
#include "cwFixStation.h"
#include "CaveWhereLibExport.h"

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
    //! and inherits it. \a gridConvergence is subtracted from a literal
    //! declination — see cwSurvexExporterUtils::writeDeclinationCalibration.
    //! It stays 0 for a trip exported on its own, which writes no `*cs out`
    //! unless it uses auto, so there is no grid for the literal to converge to.
    void writeTrip(QTextStream& stream,
                   const cwTripData& trip,
                   bool autoDeclinationInScope = false,
                   double gridConvergence = 0.0);

protected:
    void runTask() override;

private:
    cwTripData Trip;
    QList<cwFixStation> CaveFixStations;
};

#endif // CWSURVEXEXPORTERTRIPTASK_H
