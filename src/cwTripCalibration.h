/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRIPCALIBRATION_H
#define CWTRIPCALIBRATION_H

//Qt includes
#include <QObject>
#include <QStringList>
#include "cwGlobals.h"

//Our includes
#include "cwUnits.h"

class CAVEWHERE_LIB_EXPORT cwTripCalibration : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool correctedCompassBacksight READ hasCorrectedCompassBacksight WRITE setCorrectedCompassBacksight NOTIFY correctedCompassBacksightChanged)
    Q_PROPERTY(bool correctedClinoBacksight READ hasCorrectedClinoBacksight WRITE setCorrectedClinoBacksight NOTIFY correctedClinoBacksightChanged)
    Q_PROPERTY(double tapeCalibration READ tapeCalibration WRITE setTapeCalibration NOTIFY tapeCalibrationChanged)
    Q_PROPERTY(double frontCompassCalibration READ frontCompassCalibration WRITE setFrontCompassCalibration NOTIFY frontCompassCalibrationChanged)
    Q_PROPERTY(double frontClinoCalibration READ frontClinoCalibration WRITE setFrontClinoCalibration NOTIFY frontClinoCalibrationChanged)
    Q_PROPERTY(double backCompassCalibration READ backCompassCalibration WRITE setBackCompassCalibration NOTIFY backCompassCalibrationChanged)
    Q_PROPERTY(double backClinoCalibration READ backClinoCalibration WRITE setBackClinoCalibration NOTIFY backClinoCalibrationChanged)
    Q_PROPERTY(double declination READ declination WRITE setDeclination NOTIFY declinationChanged)
    Q_PROPERTY(cwUnits::LengthUnit distanceUnit READ distanceUnit WRITE setDistanceUnit NOTIFY distanceUnitChanged)
    Q_PROPERTY(QStringList supportedDistanceUnits READ supportedDistanceUnits NOTIFY supportedUnitsChanged)
    Q_PROPERTY(bool frontSights READ hasFrontSights WRITE setFrontSights NOTIFY frontSightsChanged)
    Q_PROPERTY(bool backSights READ hasBackSights WRITE setBackSights NOTIFY backSightsChanged)
    Q_PROPERTY(cwUnits::AngleUnit frontCompassUnit READ frontCompassUnit WRITE setFrontCompassUnit NOTIFY frontCompassUnitChanged)
    Q_PROPERTY(cwUnits::AngleUnit frontClinoUnit READ frontClinoUnit WRITE setFrontClinoUnit NOTIFY frontClinoUnitChanged)
    Q_PROPERTY(cwUnits::AngleUnit backCompassUnit READ backCompassUnit WRITE setBackCompassUnit NOTIFY backCompassUnitChanged)
    Q_PROPERTY(cwUnits::AngleUnit backClinoUnit READ backClinoUnit WRITE setBackClinoUnit NOTIFY backClinoUnitChanged)
    Q_PROPERTY(cwUnits::AngleUnit angleUnit READ angleUnit WRITE setAngleUnit NOTIFY angleUnitChanged)

public:
    explicit cwTripCalibration(QObject *parent = 0);
    cwTripCalibration(const cwTripCalibration& object);
    cwTripCalibration& operator =(const cwTripCalibration& object);

    void setCorrectedCompassBacksight(bool isCorrected);
    bool hasCorrectedCompassBacksight() const;

    void setCorrectedClinoBacksight(bool isCorrected);
    bool hasCorrectedClinoBacksight() const;

    void setTapeCalibration(double tapeCalibration);
    double tapeCalibration() const;

    void setFrontCompassCalibration(double calibration);
    double frontCompassCalibration() const;

    void setFrontClinoCalibration(double calibration);
    double frontClinoCalibration() const;

    void setBackCompassCalibration(double calibration);
    double backCompassCalibration() const;

    void setBackClinoCalibration(double calibration);
    double backClinoCalibration() const;

    void setDeclination(double declination);
    double declination() const;

    cwUnits::LengthUnit distanceUnit() const;
    void setDistanceUnit(cwUnits::LengthUnit unit);

    bool hasBackSights() const;
    void setBackSights(bool hasBackSights);

    bool hasFrontSights() const;
    void setFrontSights(bool hasFrontSights);

    cwUnits::AngleUnit frontCompassUnit() const;
    void setFrontCompassUnit(cwUnits::AngleUnit frontCompassUnit);

    cwUnits::AngleUnit frontClinoUnit() const;
    void setFrontClinoUnit(cwUnits::AngleUnit frontClinoUnit);

    cwUnits::AngleUnit backCompassUnit() const;
    void setBackCompassUnit(cwUnits::AngleUnit backCompassUnit);

    cwUnits::AngleUnit backClinoUnit() const;
    void setBackClinoUnit(cwUnits::AngleUnit backClinoUnit);

    cwUnits::AngleUnit angleUnit() const;
    void setAngleUnit(cwUnits::AngleUnit angleUnit);

    QStringList supportedDistanceUnits() const;

    Q_INVOKABLE int mapToLengthUnit(int supportedUnitIndex);
    Q_INVOKABLE int mapToSupportUnit(int lengthUnit);

signals:
    void correctedCompassBacksightChanged(bool corrected);
    void correctedClinoBacksightChanged(bool corrected);
    void tapeCalibrationChanged(double calibration);
    void frontCompassCalibrationChanged(double calibration);
    void frontClinoCalibrationChanged(double calibration);
    void backCompassCalibrationChanged(double calibration);
    void backClinoCalibrationChanged(double calibration);
    void declinationChanged(double declination);
    void distanceUnitChanged(cwUnits::LengthUnit unit);
    void supportedUnitsChanged();
    void frontSightsChanged();
    void backSightsChanged();
    void calibrationsChanged(); //Emited when ever any of calibrations have changed
    void frontCompassUnitChanged();
    void frontClinoUnitChanged();
    void backCompassUnitChanged();
    void backClinoUnitChanged();
    void angleUnitChanged();

public slots:

private:
    bool CorrectedCompassBacksight;
    bool CorrectedClinoBacksight;
    double TapeCalibration;
    double FrontCompassCalibration;
    double FrontClinoCalibration;
    double BackCompasssCalibration;
    double BackClinoCalibration;
    double Declination;
    cwUnits::LengthUnit DistanceUnit;
    bool BackSights; //!<
    bool FrontSights; //!<
    cwUnits::AngleUnit FrontCompassUnit; //!<
    cwUnits::AngleUnit FrontClinoUnit; //!<
    cwUnits::AngleUnit BackCompassUnit; //!<
    cwUnits::AngleUnit BackClinoUnit; //!<
    cwUnits::AngleUnit AngleUnit; //!<

    cwTripCalibration& copy(const cwTripCalibration& object);
};

inline bool cwTripCalibration::hasCorrectedCompassBacksight() const {
    return CorrectedCompassBacksight;
}

inline bool cwTripCalibration::hasCorrectedClinoBacksight() const {
    return CorrectedClinoBacksight;
}

inline double cwTripCalibration::tapeCalibration() const {
    return TapeCalibration;
}

inline double cwTripCalibration::frontCompassCalibration() const {
    return FrontCompassCalibration;
}

inline double cwTripCalibration::frontClinoCalibration() const {
    return FrontClinoCalibration;
}

inline double cwTripCalibration::backCompassCalibration() const {
    return BackCompasssCalibration;
}

inline double cwTripCalibration::backClinoCalibration() const {
    return BackClinoCalibration;
}

inline double cwTripCalibration::declination() const {
    return Declination;
}

inline cwUnits::LengthUnit cwTripCalibration::distanceUnit() const {
    return DistanceUnit;
}

/**
    Gets frontSights, this returns true if the trip is using front sights
*/
inline bool cwTripCalibration::hasFrontSights() const {
    return FrontSights;
}

/**
Gets backSights
*/
inline bool cwTripCalibration::hasBackSights() const {
    return BackSights;
}

/**
Gets the frontsite compass unit
*/
inline cwUnits::AngleUnit cwTripCalibration::frontCompassUnit() const {
    return FrontCompassUnit;
}

/**
Gets the frontsite clino unit
*/
inline cwUnits::AngleUnit cwTripCalibration::frontClinoUnit() const {
    return FrontClinoUnit;
}

/**
* Gets the backsite compasses unit
*/
inline cwUnits::AngleUnit cwTripCalibration::backCompassUnit() const {
    return BackCompassUnit;
}

/**
* @brief cwTripCalibration::backClinoUnit
* @return
*/
inline cwUnits::AngleUnit cwTripCalibration::backClinoUnit() const {
    return BackClinoUnit;
}

/**
* @brief cwTripCalibration::angleUnit
* @return
*/
inline cwUnits::AngleUnit cwTripCalibration::angleUnit() const {
    return AngleUnit;
}

#endif // CWTRIPCALIBRATION_H
