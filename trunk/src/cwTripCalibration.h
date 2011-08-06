#ifndef CWTRIPCALIBRATION_H
#define CWTRIPCALIBRATION_H

//Qt includes
#include <QObject>

//Our includes
#include "cwUnits.h"

class cwTripCalibration : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool correctedCompassBacksight READ hasCorrectedCompassBacksight WRITE setCorrectedCompassBacksight NOTIFY correctedCompassBacksightChanged)
    Q_PROPERTY(bool correctedClinoBacksight READ hasCorrectedClinoBacksight WRITE setCorrectedClinoBacksight NOTIFY correctedClinoBacksightChanged)
    Q_PROPERTY(float tapeCalibration READ tapeCalibration WRITE setTapeCalibration NOTIFY tapeCalibrationChanged)
    Q_PROPERTY(float frontCompassCalibration READ frontCompassCalibration WRITE setFrontCompassCalibration NOTIFY frontCompassCalibrationChanged)
    Q_PROPERTY(float backCompassCalibration READ backCompassCalibration WRITE setBackCompassCalibration NOTIFY backCompassCalibrationChanged)
    Q_PROPERTY(float backClinoCalibration READ backClinoCalibration WRITE setBackClinoCalibration NOTIFY backClinoCalibrationChanged)
    Q_PROPERTY(float declination READ declination WRITE setDeclination NOTIFY declinationChanged)
    Q_PROPERTY(cwUnits::LengthUnit distanceUnit READ distanceUnit WRITE setDistanceUnit NOTIFY distanceUnitChanged)

public:
    explicit cwTripCalibration(QObject *parent = 0);
    cwTripCalibration(const cwTripCalibration& object);
    cwTripCalibration& operator =(const cwTripCalibration& object);

    void setCorrectedCompassBacksight(bool isCorrected);
    bool hasCorrectedCompassBacksight() const;

    void setCorrectedClinoBacksight(bool isCorrected);
    bool hasCorrectedClinoBacksight() const;

    void setTapeCalibration(float tapeCalibration);
    float tapeCalibration() const;

    void setFrontCompassCalibration(float calibration);
    float frontCompassCalibration() const;

    void setFrontClinoCalibration(float calibration);
    float frontClinoCalibration() const;

    void setBackCompassCalibration(float calibration);
    float backCompassCalibration() const;

    void setBackClinoCalibration(float calibration);
    float backClinoCalibration() const;

    void setDeclination(float declination);
    float declination() const;

    cwUnits::LengthUnit distanceUnit() const;
    void setDistanceUnit(cwUnits::LengthUnit unit);

signals:
    void correctedCompassBacksightChanged(bool corrected);
    void correctedClinoBacksightChanged(bool corrected);
    void tapeCalibrationChanged(float calibration);
    void frontCompassCalibrationChanged(float calibration);
    void frontClinoCalibrationChanged(float calibration);
    void backCompassCalibrationChanged(float calibration);
    void backClinoCalibrationChanged(float calibration);
    void declinationChanged(float declination);
    void distanceUnitChanged(cwUnits::LengthUnit unit);

public slots:

private:
    bool CorrectedCompassBacksight;
    bool CorrectedClinoBacksight;
    float TapeCalibration;
    float FrontCompassCalibration;
    float FrontClinoCalibration;
    float BackCompasssCalibration;
    float BackClinoCalibration;
    float Declination;
    cwUnits::LengthUnit DistanceUnit;

    cwTripCalibration& copy(const cwTripCalibration& object);
};

inline bool cwTripCalibration::hasCorrectedCompassBacksight() const {
    return CorrectedCompassBacksight;
}

inline bool cwTripCalibration::hasCorrectedClinoBacksight() const {
    return CorrectedClinoBacksight;
}

inline float cwTripCalibration::tapeCalibration() const {
    return TapeCalibration;
}

inline float cwTripCalibration::frontCompassCalibration() const {
    return FrontCompassCalibration;
}

inline float cwTripCalibration::frontClinoCalibration() const {
    return FrontClinoCalibration;
}

inline float cwTripCalibration::backCompassCalibration() const {
    return BackCompasssCalibration;
}

inline float cwTripCalibration::backClinoCalibration() const {
    return BackClinoCalibration;
}

inline float cwTripCalibration::declination() const {
    return Declination;
}

inline cwUnits::LengthUnit cwTripCalibration::distanceUnit() const {
    return DistanceUnit;
}

#endif // CWTRIPCALIBRATION_H
