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
}

/**
  Copy constructor!
  */
cwTripCalibration::cwTripCalibration(const cwTripCalibration& object) :
    QObject(NULL)
{
    CorrectedCompassBacksight = object.CorrectedCompassBacksight;
    CorrectedClinoBacksight = object.CorrectedClinoBacksight;
    TapeCalibration = object.TapeCalibration;
    FrontCompassCalibration = object.FrontCompassCalibration;
    FrontClinoCalibration = object.FrontClinoCalibration;
    BackCompasssCalibration = object.BackCompasssCalibration;
    BackClinoCalibration = object.BackClinoCalibration;
    Declination = object.Declination;
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

void cwTripCalibration::setTapeCalibration(float tapeCalibration) {
    if(tapeCalibration != TapeCalibration) {
        TapeCalibration = tapeCalibration;
        emit tapeCalibrationChanged(TapeCalibration);
    }
}

void cwTripCalibration::setFrontCompassCalibration(float calibration) {
    if(FrontCompassCalibration != calibration) {
        FrontCompassCalibration = calibration;
        emit frontCompassCalibrationChanged(FrontCompassCalibration);
    }
}

void cwTripCalibration::setFrontClinoCalibration(float calibration) {
    if(FrontClinoCalibration != calibration) {
        FrontClinoCalibration = calibration;
        emit frontClinoCalibrationChanged(FrontClinoCalibration);
    }
}

void cwTripCalibration::setBackCompassCalibration(float calibration) {
    if(BackCompasssCalibration != calibration) {
        BackCompasssCalibration = calibration;
        emit backCompassCalibrationChanged(BackCompasssCalibration);
    }
}

void cwTripCalibration::setBackClinoCalibration(float calibration) {
    if(BackClinoCalibration != calibration) {
        BackClinoCalibration = calibration;
        emit backClinoCalibrationChanged(BackClinoCalibration);
    }
}

void cwTripCalibration::setDeclination(float declination) {
    if(Declination != declination) {
        Declination = declination;
        emit declinationChanged(Declination);
    }
}

