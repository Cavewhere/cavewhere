#ifndef CWSURVEXEXPORTERUTILS_H
#define CWSURVEXEXPORTERUTILS_H

#include <QtGlobal>
#include <QString>

#include "cwClinoReading.h"
#include "cwCompassReading.h"
#include "cwDistanceReading.h"
#include "cwStation.h"
#include "cwShot.h"

namespace cwSurvexExporterUtils {

//Survex's parser rejects scientific notation (e.g. "1.1e+02"). A reading's
//stored string can be in that form (older imports formatted LRUD with
//QString::number(value, 'g', 2)), so rewrite just those as a plain decimal.
//Two decimals matches the source granularity of such values (Compass stores
//hundredths); any string without an exponent is returned untouched.
inline QString toSurvexNumber(const QString& value)
{
    constexpr int SurvexDecimalPlaces = 2;

    if(!value.contains(QLatin1Char('e'), Qt::CaseInsensitive)) {
        return value;
    }

    bool ok = false;
    const double number = value.toDouble(&ok);
    if(!ok) {
        return value;
    }

    return QString::number(number, 'f', SurvexDecimalPlaces);
}

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

//True when a shot only carries passage dimensions: a valid zero-length
//distance with no compass or clino (front or back). Compass records LRUD on
//dead-end stations this way, e.g. "ALT13LRUD ALT13 0 - - - -". Survex rejects
//such a leg (omitted compass on a non-plumbed shot), so it's exported as an
//*equate of the two stations instead.
inline bool isLrudOnlyShot(const cwShot& shot)
{
    const cwDistanceReading distance = shot.distance();
    const bool distanceIsZero = distance.state() == cwDistanceReading::State::Valid
                                && qFuzzyIsNull(distance.toDouble());
    return distanceIsZero
        && shot.compass().state()     == cwCompassReading::State::Empty
        && shot.backCompass().state() == cwCompassReading::State::Empty
        && shot.clino().state()       == cwClinoReading::State::Empty
        && shot.backClino().state()   == cwClinoReading::State::Empty;
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
