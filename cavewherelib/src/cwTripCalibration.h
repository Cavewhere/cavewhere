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
#include <QQmlEngine>
#include "cwGlobals.h"

//Our includes
#include "cwUnits.h"

class cwTripCalibrationData {
public:
    cwTripCalibrationData();
    cwTripCalibrationData(const cwTripCalibrationData &other);
    cwTripCalibrationData& operator=(const cwTripCalibrationData &other);

    // Inline getters and setters for each calibration property:
    inline bool hasCorrectedCompassBacksight() const { return d->m_CorrectedCompassBacksight; }
    inline void setCorrectedCompassBacksight(bool value) { d->m_CorrectedCompassBacksight = value; }

    inline bool hasCorrectedClinoBacksight() const { return d->m_CorrectedClinoBacksight; }
    inline void setCorrectedClinoBacksight(bool value) { d->m_CorrectedClinoBacksight = value; }

    inline bool hasCorrectedCompassFrontsight() const { return d->m_CorrectedCompassFrontsight; }
    inline void setCorrectedCompassFrontsight(bool value) { d->m_CorrectedCompassFrontsight = value; }

    inline bool hasCorrectedClinoFrontsight() const { return d->m_CorrectedClinoFrontsight; }
    inline void setCorrectedClinoFrontsight(bool value) { d->m_CorrectedClinoFrontsight = value; }

    inline double tapeCalibration() const { return d->m_TapeCalibration; }
    inline void setTapeCalibration(double value) { d->m_TapeCalibration = value; }

    inline double frontCompassCalibration() const { return d->m_FrontCompassCalibration; }
    inline void setFrontCompassCalibration(double value) { d->m_FrontCompassCalibration = value; }

    inline double frontClinoCalibration() const { return d->m_FrontClinoCalibration; }
    inline void setFrontClinoCalibration(double value) { d->m_FrontClinoCalibration = value; }

    inline double backCompassCalibration() const { return d->m_BackCompassCalibration; }
    inline void setBackCompassCalibration(double value) { d->m_BackCompassCalibration = value; }

    inline double backClinoCalibration() const { return d->m_BackClinoCalibration; }
    inline void setBackClinoCalibration(double value) { d->m_BackClinoCalibration = value; }

    inline double declination() const { return d->m_Declination; }
    inline void setDeclination(double value) { d->m_Declination = value; }

    inline cwUnits::LengthUnit distanceUnit() const { return d->m_DistanceUnit; }
    inline void setDistanceUnit(cwUnits::LengthUnit value) { d->m_DistanceUnit = value; }

    inline bool hasFrontSights() const { return d->m_FrontSights; }
    inline void setFrontSights(bool value) { d->m_FrontSights = value; }

    inline bool hasBackSights() const { return d->m_BackSights; }
    inline void setBackSights(bool value) { d->m_BackSights = value; }

private:
    // The PrivateData class encapsulates the actual data and inherits QSharedData
    class PrivateData : public QSharedData {
    public:
        PrivateData()
            : m_CorrectedCompassBacksight(false)
            , m_CorrectedClinoBacksight(false)
            , m_CorrectedCompassFrontsight(false)
            , m_CorrectedClinoFrontsight(false)
            , m_TapeCalibration(0.0)
            , m_FrontCompassCalibration(0.0)
            , m_FrontClinoCalibration(0.0)
            , m_BackCompassCalibration(0.0)
            , m_BackClinoCalibration(0.0)
            , m_Declination(0.0)
            , m_DistanceUnit(cwUnits::Meters)
            , m_FrontSights(true)
            , m_BackSights(true)
        {}

        PrivateData(const PrivateData &other)
            : QSharedData(other)
            , m_CorrectedCompassBacksight(other.m_CorrectedCompassBacksight)
            , m_CorrectedClinoBacksight(other.m_CorrectedClinoBacksight)
            , m_CorrectedCompassFrontsight(other.m_CorrectedCompassFrontsight)
            , m_CorrectedClinoFrontsight(other.m_CorrectedClinoFrontsight)
            , m_TapeCalibration(other.m_TapeCalibration)
            , m_FrontCompassCalibration(other.m_FrontCompassCalibration)
            , m_FrontClinoCalibration(other.m_FrontClinoCalibration)
            , m_BackCompassCalibration(other.m_BackCompassCalibration)
            , m_BackClinoCalibration(other.m_BackClinoCalibration)
            , m_Declination(other.m_Declination)
            , m_DistanceUnit(other.m_DistanceUnit)
            , m_FrontSights(other.m_FrontSights)
            , m_BackSights(other.m_BackSights)
        {}

        bool m_CorrectedCompassBacksight;
        bool m_CorrectedClinoBacksight;
        bool m_CorrectedCompassFrontsight;
        bool m_CorrectedClinoFrontsight;
        double m_TapeCalibration;
        double m_FrontCompassCalibration;
        double m_FrontClinoCalibration;
        double m_BackCompassCalibration;
        double m_BackClinoCalibration;
        double m_Declination;
        cwUnits::LengthUnit m_DistanceUnit;
        bool m_FrontSights;
        bool m_BackSights;
    };

    QSharedDataPointer<PrivateData> d;
};

// Inline implementations for constructors and assignment
inline cwTripCalibrationData::cwTripCalibrationData()
    : d(new PrivateData)
{
}

inline cwTripCalibrationData::cwTripCalibrationData(const cwTripCalibrationData &other)
    : d(other.d)
{
}

inline cwTripCalibrationData& cwTripCalibrationData::operator=(const cwTripCalibrationData &other)
{
    if (this != &other) {
        d = other.d;
    }
    return *this;
}


#include <QObject>
#include <QStringList>
#include "cwUnits.h"

class CAVEWHERE_LIB_EXPORT cwTripCalibration : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TripCalibration)

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
    explicit cwTripCalibration(QObject *parent = nullptr);
    cwTripCalibration(const cwTripCalibration &other);
    cwTripCalibration& operator=(const cwTripCalibration &other);

    // Getters that delegate to the internal data instance:
    bool hasCorrectedCompassBacksight() const { return m_data.hasCorrectedCompassBacksight(); }
    bool hasCorrectedClinoBacksight() const { return m_data.hasCorrectedClinoBacksight(); }
    bool hasCorrectedCompassFrontsight() const { return m_data.hasCorrectedCompassFrontsight(); }
    bool hasCorrectedClinoFrontsight() const { return m_data.hasCorrectedClinoFrontsight(); }
    double tapeCalibration() const { return m_data.tapeCalibration(); }
    double frontCompassCalibration() const { return m_data.frontCompassCalibration(); }
    double frontClinoCalibration() const { return m_data.frontClinoCalibration(); }
    double backCompassCalibration() const { return m_data.backCompassCalibration(); }
    double backClinoCalibration() const { return m_data.backClinoCalibration(); }
    double declination() const { return m_data.declination(); }
    cwUnits::LengthUnit distanceUnit() const { return m_data.distanceUnit(); }
    bool hasFrontSights() const { return m_data.hasFrontSights(); }
    bool hasBackSights() const { return m_data.hasBackSights(); }

    // Setters that update the internal data and emit signals if changed:
    void setCorrectedCompassBacksight(bool value);
    void setCorrectedClinoBacksight(bool value);
    void setCorrectedCompassFrontsight(bool value);
    void setCorrectedClinoFrontsight(bool value);
    void setTapeCalibration(double value);
    void setFrontCompassCalibration(double value);
    void setFrontClinoCalibration(double value);
    void setBackCompassCalibration(double value);
    void setBackClinoCalibration(double value);
    void setDeclination(double value);
    void setDistanceUnit(cwUnits::LengthUnit value);
    void setFrontSights(bool value);
    void setBackSights(bool value);

    QStringList supportedUnits() const;

    // New getter that returns the entire data structure:
    cwTripCalibrationData data() const { return m_data; }

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
    void calibrationsChanged();

public slots:

private:
    // The instance of the public data class
    cwTripCalibrationData m_data;
};



#endif // CWTRIPCALIBRATION_H
