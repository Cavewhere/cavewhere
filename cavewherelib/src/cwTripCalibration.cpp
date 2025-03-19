#include "cwTripCalibration.h"

cwTripCalibration::cwTripCalibration(QObject *parent)
    : QObject(parent)
{
}

cwTripCalibration::cwTripCalibration(const cwTripCalibration &other)
    : QObject(nullptr),
    m_data(other.m_data)
{
}

cwTripCalibration& cwTripCalibration::operator=(const cwTripCalibration &other)
{
    if (this != &other) {
        m_data = other.m_data;
    }
    return *this;
}

void cwTripCalibration::setCorrectedCompassBacksight(bool value)
{
    if (m_data.hasCorrectedCompassBacksight() != value) {
        m_data.setCorrectedCompassBacksight(value);
        emit correctedCompassBacksightChanged(value);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setCorrectedClinoBacksight(bool value)
{
    if (m_data.hasCorrectedClinoBacksight() != value) {
        m_data.setCorrectedClinoBacksight(value);
        emit correctedClinoBacksightChanged(value);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setCorrectedCompassFrontsight(bool value)
{
    if (m_data.hasCorrectedCompassFrontsight() != value) {
        m_data.setCorrectedCompassFrontsight(value);
        emit correctedCompassFrontsightChanged();
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setCorrectedClinoFrontsight(bool value)
{
    if (m_data.hasCorrectedClinoFrontsight() != value) {
        m_data.setCorrectedClinoFrontsight(value);
        emit correctedClinoFrontsightChanged();
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setTapeCalibration(double value)
{
    if (m_data.tapeCalibration() != value) {
        m_data.setTapeCalibration(value);
        emit tapeCalibrationChanged(value);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setFrontCompassCalibration(double value)
{
    if (m_data.frontCompassCalibration() != value) {
        m_data.setFrontCompassCalibration(value);
        emit frontCompassCalibrationChanged(value);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setFrontClinoCalibration(double value)
{
    if (m_data.frontClinoCalibration() != value) {
        m_data.setFrontClinoCalibration(value);
        emit frontClinoCalibrationChanged(value);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setBackCompassCalibration(double value)
{
    if (m_data.backCompassCalibration() != value) {
        m_data.setBackCompassCalibration(value);
        emit backCompassCalibrationChanged(value);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setBackClinoCalibration(double value)
{
    if (m_data.backClinoCalibration() != value) {
        m_data.setBackClinoCalibration(value);
        emit backClinoCalibrationChanged(value);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setDeclination(double value)
{
    if (m_data.declination() != value) {
        m_data.setDeclination(value);
        emit declinationChanged(value);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setDistanceUnit(cwUnits::LengthUnit value)
{
    if (m_data.distanceUnit() != value) {
        m_data.setDistanceUnit(value);
        emit distanceUnitChanged(value);
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setFrontSights(bool value)
{
    if (m_data.hasFrontSights() != value) {
        m_data.setFrontSights(value);
        emit frontSightsChanged();
        emit calibrationsChanged();
    }
}

void cwTripCalibration::setBackSights(bool value)
{
    if (m_data.hasBackSights() != value) {
        m_data.setBackSights(value);
        emit backSightsChanged();
        emit calibrationsChanged();
    }
}

QStringList cwTripCalibration::supportedUnits() const
{
    QStringList list;
    list.append("m");
    list.append("ft");
    return list;
}

int cwTripCalibration::mapToLengthUnit(int supportedUnitIndex)
{
    switch (supportedUnitIndex) {
    case 0:
        return cwUnits::Meters;
    case 1:
        return cwUnits::Feet;
    default:
        return cwUnits::LengthUnitless;
    }
}

int cwTripCalibration::mapToSupportUnit(int lengthUnit)
{
    switch (lengthUnit) {
    case cwUnits::Meters:
        return 0;
    case cwUnits::Feet:
        return 1;
    default:
        return -1;
    }
}
