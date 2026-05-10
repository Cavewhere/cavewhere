#ifndef CWSURVEXEXPORTERUTILS_H
#define CWSURVEXEXPORTERUTILS_H

#include <QtGlobal>
#include <QString>

#include "cwClinoReading.h"
#include "cwDistanceReading.h"
#include "cwStation.h"

namespace cwSurvexExporterUtils {
inline QString verticalClinoText(const cwClinoReading& reading)
{
    if(reading.state() != cwClinoReading::State::Valid) {
        return QString();
    }

    bool ok = false;
    const double value = reading.toDouble(&ok);
    if(!ok) {
        return QString();
    }

    if(qAbs(qAbs(value) - 90.0) >= 0.001) {
        return QString();
    }

    return value >= 0.0 ? QStringLiteral("UP") : QStringLiteral("DOWN");
}

inline bool stationHasLrudData(const cwStation& station)
{
    return station.left().state()  == cwDistanceReading::State::Valid
        || station.right().state() == cwDistanceReading::State::Valid
        || station.up().state()    == cwDistanceReading::State::Valid
        || station.down().state()  == cwDistanceReading::State::Valid;
}
} // namespace cwSurvexExporterUtils

#endif // CWSURVEXEXPORTERUTILS_H
