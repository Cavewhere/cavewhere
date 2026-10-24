// cwShot.cpp
#include "cwShot.h"

// --- PrivateData ---

cwShot::cwShot() :
    m_id(QUuid::createUuid())
{

}


cwShot::cwShot(const QString& distance,
               const QString& compass,
               const QString& backCompass,
               const QString& clino,
               const QString& backClino) :
    m_id(QUuid::createUuid())
{
    // Front measurement
    cwShotMeasurement front;
    front.distance = cwDistanceReading(distance);
    front.compass = cwCompassReading(compass);
    front.clino = cwClinoReading(clino);
    front.direction = cwShotMeasurement::Direction::Front;
    m_measurements.append(front);

    // Back measurement
    cwShotMeasurement back;
    back.distance = cwDistanceReading(distance);
    back.compass = cwCompassReading(backCompass);
    back.clino = cwClinoReading(backClino);
    back.direction = cwShotMeasurement::Direction::Back;
    m_measurements.append(back);
}

// --- Legacy API implementations ---

cwDistanceReading cwShot::distance() const {
    return measurementCount() > 0
               ? m_measurements[0].distance
               : cwDistanceReading();
}

void cwShot::setDistance(const cwDistanceReading& r) {
    if (measurementCount() == 0) {
        m_measurements.append(cwShotMeasurement());
    }
    m_measurements[0].distance = r;
}

cwCompassReading cwShot::compass() const {
    return measurementCount() > 0
               ? m_measurements[0].compass
               : cwCompassReading();
}

void cwShot::setCompass(const cwCompassReading& r) {
    if (measurementCount() == 0) {
        m_measurements.append(cwShotMeasurement());
    }
    m_measurements[0].compass = r;
}

cwCompassReading cwShot::backCompass() const {
    return measurementCount() > 1
               ? m_measurements[1].compass
               : cwCompassReading();
}

void cwShot::setBackCompass(const cwCompassReading& r) {
    if (measurementCount() < 2) {
        cwShotMeasurement back;
        back.direction = cwShotMeasurement::Direction::Back;
        m_measurements.append(back);
    }
    m_measurements[1].compass = r;
}

cwClinoReading cwShot::clino() const {
    return measurementCount() > 0
               ? m_measurements[0].clino
               : cwClinoReading();
}

void cwShot::setClino(const cwClinoReading& r) {
    if (measurementCount() == 0) {
        m_measurements.append(cwShotMeasurement());
    }
    m_measurements[0].clino = r;
}

cwClinoReading cwShot::backClino() const {
    return measurementCount() > 1
               ? m_measurements[1].clino
               : cwClinoReading();
}

void cwShot::setBackClino(const cwClinoReading& r) {
    if (measurementCount() < 2) {
        cwShotMeasurement back;
        back.direction = cwShotMeasurement::Direction::Back;
        m_measurements.append(back);
    }
    m_measurements[1].clino = r;
}

// --- Multi-measurement API ---

void cwShot::addMeasurement(const cwShotMeasurement& m) {
    m_measurements.append(m);
}

void cwShot::removeMeasurementAt(int index) {
    m_measurements.removeAt(index);
}

const cwShotMeasurement& cwShot::measurementAt(int index) const {
    return m_measurements.at(index);
}

// --- Include/exclude & validity ---

bool cwShot::isDistanceIncluded() const {
    return m_includeDistance;
}

void cwShot::setDistanceIncluded(bool include) {
    m_includeDistance = include;
}

bool cwShot::isValid() const {
    return measurementCount() > 0
           && m_measurements[0].distance.state() == cwDistanceReading::State::Valid;
}

QSet<int> cwShot::ignoredWarnings() const
{
    return m_ignoredWarnings;
}

void cwShot::setIgnoredWarnings(const QSet<int>& ids)
{
    m_ignoredWarnings = ids;
}

bool cwShot::isWarningIgnored(cwErrorTypeId id) const
{
    return m_ignoredWarnings.contains(static_cast<int>(id));
}
