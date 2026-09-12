/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXEXPORTER_H
#define CWSURVEXEXPORTER_H

//Our includes
#include "cwTripData.h"
#include "cwSurveyChunkData.h"
#include "cwTeamData.h"
#include "cwTripCalibration.h"
#include "cwUnits.h"
#include "cwDistanceReading.h"
#include "cwClinoReading.h"
#include "cwCompassReading.h"
#include "cwShotMeasurement.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QDate>
#include <QString>
#include <QStringList>
class QTextStream;

/**
 * \brief The one place a survex trip block is written.
 *
 * Pure compute over the value snapshots, so it is safe to call from any
 * thread. cwSurvexExporterTripTask and cwSurvexExporterCaveTask are runnable
 * wrappers around it.
 */
class CAVEWHERE_LIB_EXPORT cwSurvexExporter
{
public:
    cwSurvexExporter() = delete;

    //! Writes one trip block. Appends any per-shot problems to \a errors and
    //! keeps writing; the caller decides whether errors abort the export.
    //!
    //! \a autoDeclinationInScope says an enclosing block already carries
    //! `*declination auto`, so a trip with auto on writes no declination line
    //! and inherits it. \a gridConvergence is subtracted from a literal
    //! declination — see cwSurvexExporterUtils::writeDeclinationCalibration.
    static void writeTrip(QTextStream& stream,
                          const cwTripData& trip,
                          QStringList& errors,
                          bool autoDeclinationInScope = false,
                          double gridConvergence = 0.0);

    //! The number of stations in every chunk of \a trip
    static int stationCount(const cwTripData& trip);

private:
    static void writeCalibrations(QTextStream& stream,
                                  const cwTripCalibrationData& calibrations,
                                  bool autoDeclinationInScope,
                                  double gridConvergence);
    static void writeLengthUnits(QTextStream& stream, cwUnits::LengthUnit unit);
    static void writeShotData(QTextStream& stream, const cwTripData& trip, QStringList& errors);
    static void writeChunk(QTextStream& stream,
                           bool hasFrontSights,
                           bool hasBackSights,
                           const cwTripCalibrationData& calibration,
                           const cwSurveyChunkData& chunk,
                           QStringList& errors);
    static void writeSplayData(QTextStream& stream,
                               const cwTripData& trip,
                               const QString& normalDataLine,
                               bool tripHasBackSights,
                               QStringList& errors);
    static QString splayLine(const cwTripCalibrationData& calibration,
                             const QString& stationName,
                             const cwShotMeasurement& splay,
                             QStringList& errors);
    static void writeLRUDData(QTextStream& stream, const cwTripData& trip);
    static void writeTeamData(QTextStream& stream, const cwTeamData& team);
    static void writeDate(QTextStream& stream, QDate date);

    static QString toSupportedLength(const cwTripCalibrationData& calibration,
                                     const cwDistanceReading& reading);
    static QString compassToString(const cwCompassReading& reading);
    static QString clinoToString(const cwClinoReading& reading);
};

#endif // CWSURVEXEXPORTER_H
