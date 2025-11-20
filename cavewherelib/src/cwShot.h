// cwShot.h
#ifndef CSHOT_H
#define CSHOT_H

/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include <QSharedDataPointer>
#include <QList>
#include <QString>
#include <QUuid>

#include "cwGlobals.h"
#include "cwShotMeasurement.h"
#include "cwDistanceReading.h"
#include "cwCompassReading.h"
#include "cwClinoReading.h"

class CAVEWHERE_LIB_EXPORT cwShot {
public:
    /**
     * Default ctor: seeds with one front/back measurement (legacy behavior)
     */
    cwShot();

    /**
     * Legacy ctor: initializes front and backsight readings
     */
    cwShot(const QString& distance,
           const QString& compass,
           const QString& backCompass,
           const QString& clino,
           const QString& backClino);

    cwShot(const cwShot&) = default;
    cwShot& operator=(const cwShot&) = default;

    QUuid id() const { return m_id; }
    void setId(const QUuid id) { m_id = id; }

    // Legacy API (for backward compatibility)
    cwDistanceReading distance() const;
    void setDistance(const cwDistanceReading& reading);

    cwCompassReading compass() const;
    void setCompass(const cwCompassReading& reading);

    cwCompassReading backCompass() const;
    void setBackCompass(const cwCompassReading& reading);

    cwClinoReading clino() const;
    void setClino(const cwClinoReading& reading);

    cwClinoReading backClino() const;
    void setBackClino(const cwClinoReading& reading);

    // Multi-measurement API
    void addMeasurement(const cwShotMeasurement& measurement);
    void removeMeasurementAt(int index);
    const cwShotMeasurement& measurementAt(int index) const;
    int measurementCount() const;

    // Include/exclude in distance sums
    bool isDistanceIncluded() const;
    void setDistanceIncluded(bool include);

    // Validity check (delegates to first measurement)
    bool isValid() const;

private:
    QUuid m_id;
    QList<cwShotMeasurement> measurements;
    bool IncludeDistance = true;
};

inline int cwShot::measurementCount() const {
    return measurements.size();
}

#endif // CSHOT_H
