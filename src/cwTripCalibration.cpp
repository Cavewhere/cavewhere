/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTripCalibration.h"

cwTripCalibration::cwTripCalibration(QObject *parent) :
    QObject(parent)
{
    CorrectedClinoBacksight = false;
    CorrectedCompassBacksight = false;
    CorrectedClinoFrontsight = false;
    CorrectedCompassFrontsight = false;
    TapeCalibration = 0.0f;
    FrontCompassCalibration = 0.0f;
    FrontClinoCalibration = 0.0f;
    BackCompasssCalibration = 0.0f;
    BackClinoCalibration = 0.0f;
    Declination = 0.0f;
    DistanceUnit = cwUnits::Meters;
    FrontSights = true;
    BackSights = true;
    FrontClinoUnit = cwUnits::Degrees;
    BackClinoUnit = cwUnits::Degrees;
    FrontCompassUnit = cwUnits::Degrees;
    BackCompassUnit = cwUnits::Degrees;
    AngleUnit = cwUnits::Degrees;
}

/**
  Copy constructor!
  */
cwTripCalibration::cwTripCalibration(const cwTripCalibration& object) :
    QObject(nullptr)
{
    copy(object);
}

/**
  Alignment opterator
  */
cwTripCalibration& cwTripCalibration::operator =(const cwTripCalibration& object) {
    return copy(object);
}

cwTripCalibration& cwTripCalibration::copy(const cwTripCalibration& object) {
    if(&object == this) {
        return *this;
    }

    CorrectedCompassBacksight = object.CorrectedCompassBacksight;
    CorrectedClinoBacksight = object.CorrectedClinoBacksight;
    CorrectedCompassFrontsight = object.CorrectedCompassFrontsight;
    CorrectedClinoFrontsight = object.CorrectedClinoFrontsight;
    TapeCalibration = object.TapeCalibration;
    FrontCompassCalibration = object.FrontCompassCalibration;
    FrontClinoCalibration = object.FrontClinoCalibration;
    BackCompasssCalibration = object.BackCompasssCalibration;
    BackClinoCalibration = object.BackClinoCalibration;
    Declination = object.Declination;
    DistanceUnit = object.DistanceUnit;
    FrontSights = object.FrontSights;
    BackSights = object.BackSights;
    FrontClinoUnit = object.FrontClinoUnit;
    BackClinoUnit = object.BackClinoUnit;
    FrontCompassUnit = object.FrontCompassUnit;
    BackCompassUnit = object.BackCompassUnit;
    AngleUnit = object.AngleUnit;

    return *this;
}

void cwTripCalibration::setCorrectedCompassBacksight(bool isCorrected) {
    if(isCorrected != CorrectedCompassBacksight) {
        CorrectedCompassBacksight = isCorrected;
        emit correctedCompassBacksightChanged(isCorrected);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setCorrectedClinoBacksight(bool isCorrected) {
    if(isCorrected != CorrectedClinoBacksight) {
        CorrectedClinoBacksight = isCorrected;
        emit correctedClinoBacksightChanged(isCorrected);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setTapeCalibration(double tapeCalibration) {
    if(tapeCalibration != TapeCalibration) {
        TapeCalibration = tapeCalibration;
        emit tapeCalibrationChanged(TapeCalibration);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setFrontCompassCalibration(double calibration) {
    if(FrontCompassCalibration != calibration) {
        FrontCompassCalibration = calibration;
        emit frontCompassCalibrationChanged(FrontCompassCalibration);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setFrontClinoCalibration(double calibration) {
    if(FrontClinoCalibration != calibration) {
        FrontClinoCalibration = calibration;
        emit frontClinoCalibrationChanged(FrontClinoCalibration);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setBackCompassCalibration(double calibration) {
    if(BackCompasssCalibration != calibration) {
        BackCompasssCalibration = calibration;
        emit backCompassCalibrationChanged(BackCompasssCalibration);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setBackClinoCalibration(double calibration) {
    if(BackClinoCalibration != calibration) {
        BackClinoCalibration = calibration;
        emit backClinoCalibrationChanged(BackClinoCalibration);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setDeclination(double declination) {
    if(Declination != declination) {
        Declination = declination;
        emit declinationChanged(Declination);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setDistanceUnit(cwUnits::LengthUnit unit) {
    if(DistanceUnit != unit) {
        DistanceUnit = unit;
        emit distanceUnitChanged(unit);
        emit calibrationsChanged();
    }
}

/**
    Gets supported units that the trip calibration supports
*/
QStringList cwTripCalibration::supportedDistanceUnits() const {
    QStringList list;
    list.append("in");
    list.append("ft");
    list.append("yd");
    list.append("m");
    list.append("km");
    list.append("mi");
    return list;
}

/**
  This converts the supportedUnitIndex into cwUnit::LengthUnit
  */
int cwTripCalibration::mapToLengthUnit(int supportedUnitIndex)
{
    switch(supportedUnitIndex) {
    case 0:
        return cwUnits::Meters;
    case 1:
        return cwUnits::Feet;
    default:
        return cwUnits::LengthUnitless;
    }
}

/**
  This converts the length units into support length unit
  */
int cwTripCalibration::mapToSupportUnit(int lengthUnit)
{
    switch(lengthUnit) {
    case cwUnits::Meters:
        return 0;
    case cwUnits::Feet:
        return 1;
    default:
        return -1;
    }

}

/**
Sets frontSights
*/
void cwTripCalibration::setFrontSights(bool frontSights) {
    if(FrontSights != frontSights) {
        FrontSights = frontSights;
        emit frontSightsChanged();
        emit calibrationsChanged();
    }
}


/**
Sets backSights
*/
void cwTripCalibration::setBackSights(bool backSights) {
    if(BackSights != backSights) {
        BackSights = backSights;
        emit backSightsChanged();
        emit calibrationsChanged();
    }
}

/**
* @brief cwTripCalibration::setCorrectedCompassFrontsight
* @param correctedCompassFrontsight
*/
void cwTripCalibration::setCorrectedCompassFrontsight(bool correctedCompassFrontsight) {
    if(CorrectedCompassFrontsight != correctedCompassFrontsight) {
        CorrectedCompassFrontsight = correctedCompassFrontsight;
        emit correctedCompassFrontsightChanged();
        emit calibrationsChanged();
    }
}

/**
* @brief class::setCorrectedClinoFrontsight
* @param correctedClinoFrontsight
*/
void cwTripCalibration::setCorrectedClinoFrontsight(bool correctedClinoFrontsight) {
    if(CorrectedClinoFrontsight != correctedClinoFrontsight) {
        CorrectedClinoFrontsight = correctedClinoFrontsight;
        emit correctedClinoFrontsightChanged();
        emit calibrationsChanged();
    }
}

/**
* Sets the frontsite compass unit
*/
void cwTripCalibration::setFrontCompassUnit(cwUnits::AngleUnit frontCompassUnit) {
    if(FrontCompassUnit != frontCompassUnit) {
        FrontCompassUnit = frontCompassUnit;
        emit frontCompassUnitChanged();
    }
}

/**
* Sets the frontsite clino unit
*/
void cwTripCalibration::setFrontClinoUnit(cwUnits::AngleUnit frontClinoUnit) {
    if(FrontClinoUnit != frontClinoUnit) {
        FrontClinoUnit = frontClinoUnit;
        emit frontClinoUnitChanged();
    }
}

/**
* Sets the backsite compass unit
*/
void cwTripCalibration::setBackCompassUnit(cwUnits::AngleUnit backCompassUnit) {
    if(BackCompassUnit != backCompassUnit) {
        BackCompassUnit = backCompassUnit;
        emit backCompassUnitChanged();
    }
}

/**
* Sets the backsite compass
*/
void cwTripCalibration::setBackClinoUnit(cwUnits::AngleUnit backClinoUnit) {
    if(BackClinoUnit != backClinoUnit) {
        BackClinoUnit = backClinoUnit;
        emit backClinoUnitChanged();
    }
}

/**
* @brief cwTripCalibration::setAngleUnit
* @param angleUnit
*/
void cwTripCalibration::setAngleUnit(cwUnits::AngleUnit angleUnit) {
    if(AngleUnit != angleUnit) {
        AngleUnit = angleUnit;
        emit angleUnitChanged();
    }
}
