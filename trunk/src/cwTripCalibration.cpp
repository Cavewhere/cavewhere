#include "cwTripCalibration.h"

cwTripCalibration::cwTripCalibration(QObject *parent) :
    QObject(parent)
{
    CorrectedClinoBacksight = false;
    CorrectedCompassBacksight = false;
    TapeCalibration = 0.0f;
    FrontCompassCalibration = 0.0f;
    FrontClinoCalibration = 0.0f;
    BackCompasssCalibration = 0.0f;
    BackClinoCalibration = 0.0f;
    Declination = 0.0f;
    DistanceUnit = cwUnits::Meters;
}

/**
  Copy constructor!
  */
cwTripCalibration::cwTripCalibration(const cwTripCalibration& object) :
    QObject(NULL)
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
    TapeCalibration = object.TapeCalibration;
    FrontCompassCalibration = object.FrontCompassCalibration;
    FrontClinoCalibration = object.FrontClinoCalibration;
    BackCompasssCalibration = object.BackCompasssCalibration;
    BackClinoCalibration = object.BackClinoCalibration;
    Declination = object.Declination;
    DistanceUnit = object.DistanceUnit;

    return *this;
}

void cwTripCalibration::setCorrectedCompassBacksight(bool isCorrected) {
    if(isCorrected != CorrectedCompassBacksight) {
        CorrectedCompassBacksight = isCorrected;
        emit correctedCompassBacksightChanged(isCorrected);
    }
}

void cwTripCalibration::setCorrectedClinoBacksight(bool isCorrected) {
    if(isCorrected != CorrectedClinoBacksight) {
        CorrectedClinoBacksight = isCorrected;
        emit correctedClinoBacksightChanged(isCorrected);
    }
}

void cwTripCalibration::setTapeCalibration(double tapeCalibration) {
    if(tapeCalibration != TapeCalibration) {
        TapeCalibration = tapeCalibration;
        emit tapeCalibrationChanged(TapeCalibration);
    }
}

void cwTripCalibration::setFrontCompassCalibration(double calibration) {
    if(FrontCompassCalibration != calibration) {
        FrontCompassCalibration = calibration;
        emit frontCompassCalibrationChanged(FrontCompassCalibration);
    }
}

void cwTripCalibration::setFrontClinoCalibration(double calibration) {
    if(FrontClinoCalibration != calibration) {
        FrontClinoCalibration = calibration;
        emit frontClinoCalibrationChanged(FrontClinoCalibration);
    }
}

void cwTripCalibration::setBackCompassCalibration(double calibration) {
    if(BackCompasssCalibration != calibration) {
        BackCompasssCalibration = calibration;
        emit backCompassCalibrationChanged(BackCompasssCalibration);
    }
}

void cwTripCalibration::setBackClinoCalibration(double calibration) {
    if(BackClinoCalibration != calibration) {
        BackClinoCalibration = calibration;
        emit backClinoCalibrationChanged(BackClinoCalibration);
    }
}

void cwTripCalibration::setDeclination(double declination) {
    if(Declination != declination) {
        Declination = declination;
        emit declinationChanged(Declination);
    }
}

void cwTripCalibration::setDistanceUnit(cwUnits::LengthUnit unit) {
    if(DistanceUnit != unit) {
        DistanceUnit = unit;
        emit distanceUnitChanged(unit);
    }
}

/**
    Gets supported units that the trip calibration supports
*/
QStringList cwTripCalibration::supportedUnits() const {
    QStringList list;
    list.append("m");
    list.append("ft");
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
        return cwUnits::Unitless;
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

