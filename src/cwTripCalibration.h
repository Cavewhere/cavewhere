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
    Q_PROPERTY(bool correctedCompassFrontsight READ hasCorrectedCompassFrontsight WRITE setCorrectedCompassFrontsight NOTIFY correctedCompassFrontsightChanged)
    Q_PROPERTY(bool correctedClinoFrontsight READ hasCorrectedClinoFrontsight WRITE setCorrectedClinoFrontsight NOTIFY correctedClinoFrontsightChanged)
    Q_PROPERTY(double tapeCalibration READ tapeCalibration WRITE setTapeCalibration NOTIFY tapeCalibrationChanged)
    Q_PROPERTY(double frontCompassCalibration READ frontCompassCalibration WRITE setFrontCompassCalibration NOTIFY frontCompassCalibrationChanged)
    Q_PROPERTY(double frontClinoCalibration READ frontClinoCalibration WRITE setFrontClinoCalibration NOTIFY frontClinoCalibrationChanged)
    Q_PROPERTY(double backCompassCalibration READ backCompassCalibration WRITE setBackCompassCalibration NOTIFY backCompassCalibrationChanged)
    Q_PROPERTY(double backClinoCalibration READ backClinoCalibration WRITE setBackClinoCalibration NOTIFY backClinoCalibrationChanged)
    Q_PROPERTY(double declination READ declination WRITE setDeclination NOTIFY declinationChanged)
    Q_PROPERTY(cwUnits::LengthUnit distanceUnit READ distanceUnit WRITE setDistanceUnit NOTIFY distanceUnitChanged)
    Q_PROPERTY(QStringList supportedUnits READ supportedUnits NOTIFY supportedUnitsChanged)
    Q_PROPERTY(bool frontSights READ hasFrontSights WRITE setFrontSights NOTIFY frontSightsChanged)
    Q_PROPERTY(bool backSights READ hasBackSights WRITE setBackSights NOTIFY backSightsChanged)

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


    QStringList supportedUnits() const;

    Q_INVOKABLE int mapToLengthUnit(int supportedUnitIndex);
    Q_INVOKABLE int mapToSupportUnit(int lengthUnit);

signals:
    void correctedCompassBacksightChanged(bool corrected);
    void correctedClinoBacksightChanged(bool corrected);
    void correctedCompassFrontsightChanged();
    void correctedClinoFrontsightChanged();
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

public slots:

private:
    bool CorrectedCompassBacksight;
    bool CorrectedClinoBacksight;
    bool CorrectedCompassFrontsight; //!<
    bool CorrectedClinoFrontsight; //!<
    double TapeCalibration;
    double FrontCompassCalibration;
    double FrontClinoCalibration;
    double BackCompasssCalibration;
    double BackClinoCalibration;
    double Declination;
    cwUnits::LengthUnit DistanceUnit;
    bool BackSights; //!<
    bool FrontSights; //!<

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
* @brief class::correctedClinoFrontsight
* @return
*/
inline bool cwTripCalibration::hasCorrectedClinoFrontsight() const {
    return CorrectedClinoFrontsight;
}



#endif // CWTRIPCALIBRATION_H
