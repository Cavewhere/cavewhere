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

//Our includes
#include "cwUnits.h"
#include "cwGlobals.h"
#include "cwStringDouble.h"

class CAVEWHERE_LIB_EXPORT cwTripCalibration : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool correctedCompassBacksight READ hasCorrectedCompassBacksight WRITE setCorrectedCompassBacksight NOTIFY correctedCompassBacksightChanged)
    Q_PROPERTY(bool correctedClinoBacksight READ hasCorrectedClinoBacksight WRITE setCorrectedClinoBacksight NOTIFY correctedClinoBacksightChanged)
    Q_PROPERTY(bool correctedCompassFrontsight READ hasCorrectedCompassFrontsight WRITE setCorrectedCompassFrontsight NOTIFY correctedCompassFrontsightChanged)
    Q_PROPERTY(bool correctedClinoFrontsight READ hasCorrectedClinoFrontsight WRITE setCorrectedClinoFrontsight NOTIFY correctedClinoFrontsightChanged)
    Q_PROPERTY(cwStringDouble tapeCalibration READ tapeCalibration WRITE setTapeCalibration NOTIFY tapeCalibrationChanged)
    Q_PROPERTY(cwStringDouble frontCompassCalibration READ frontCompassCalibration WRITE setFrontCompassCalibration NOTIFY frontCompassCalibrationChanged)
    Q_PROPERTY(cwStringDouble frontClinoCalibration READ frontClinoCalibration WRITE setFrontClinoCalibration NOTIFY frontClinoCalibrationChanged)
    Q_PROPERTY(cwStringDouble backCompassCalibration READ backCompassCalibration WRITE setBackCompassCalibration NOTIFY backCompassCalibrationChanged)
    Q_PROPERTY(cwStringDouble backClinoCalibration READ backClinoCalibration WRITE setBackClinoCalibration NOTIFY backClinoCalibrationChanged)
    Q_PROPERTY(cwStringDouble declination READ declination WRITE setDeclination NOTIFY declinationChanged)
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

    void setCorrectedCompassFrontsight(bool correctedClinoFrontsight);
    bool hasCorrectedCompassFrontsight() const;

    void setCorrectedClinoFrontsight(bool correctedClinoFrontsight);
    bool hasCorrectedClinoFrontsight() const;

    void setTapeCalibration(cwStringDouble tapeCalibration);
    cwStringDouble tapeCalibration() const;

    void setFrontCompassCalibration(cwStringDouble calibration);
    cwStringDouble frontCompassCalibration() const;

    void setFrontClinoCalibration(cwStringDouble calibration);
    cwStringDouble frontClinoCalibration() const;

    void setBackCompassCalibration(cwStringDouble calibration);
    cwStringDouble backCompassCalibration() const;

    void setBackClinoCalibration(cwStringDouble calibration);
    cwStringDouble backClinoCalibration() const;

    void setDeclination(cwStringDouble declination);
    cwStringDouble declination() const;

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
    void correctedCompassFrontsightChanged();
    void correctedClinoFrontsightChanged();
    void tapeCalibrationChanged(cwStringDouble calibration);
    void frontCompassCalibrationChanged(cwStringDouble calibration);
    void frontClinoCalibrationChanged(cwStringDouble calibration);
    void backCompassCalibrationChanged(cwStringDouble calibration);
    void backClinoCalibrationChanged(cwStringDouble calibration);
    void declinationChanged(cwStringDouble declination);
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
    bool CorrectedCompassFrontsight; //!<
    bool CorrectedClinoFrontsight; //!<
    cwStringDouble TapeCalibration;
    cwStringDouble FrontCompassCalibration;
    cwStringDouble FrontClinoCalibration;
    cwStringDouble BackCompasssCalibration;
    cwStringDouble BackClinoCalibration;
    cwStringDouble Declination;
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

/**
* @brief cwTripCalibration::correctedCompassFrontsight
* @return
*/
inline bool cwTripCalibration::hasCorrectedCompassFrontsight() const {
    return CorrectedCompassFrontsight;
}


inline cwStringDouble cwTripCalibration::tapeCalibration() const {
    return TapeCalibration;
}

inline cwStringDouble cwTripCalibration::frontCompassCalibration() const {
    return FrontCompassCalibration;
}

inline cwStringDouble cwTripCalibration::frontClinoCalibration() const {
    return FrontClinoCalibration;
}

inline cwStringDouble cwTripCalibration::backCompassCalibration() const {
    return BackCompasssCalibration;
}

inline cwStringDouble cwTripCalibration::backClinoCalibration() const {
    return BackClinoCalibration;
}

inline cwStringDouble cwTripCalibration::declination() const {
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
* @brief class::correctedClinoFrontsight
* @return
*/
inline bool cwTripCalibration::hasCorrectedClinoFrontsight() const {
    return CorrectedClinoFrontsight;
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
