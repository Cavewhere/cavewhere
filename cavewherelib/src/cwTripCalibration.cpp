#include "cwTripCalibration.h"

#include "cwTrip.h"
#include "cwCave.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwFixStationModel.h"
#include "cwFixStation.h"
#include "cwCavingRegion.h"
#include "cwGeoPoint.h"
#include "cwDeclination.h"

#include "Monad/Result.h"

#include <cmath>

cwTripCalibration::cwTripCalibration(QObject *parent)
    : QObject(parent),
      m_cachedResolvedDeclination(m_data.declinationManual()),
      m_cachedAutoDeclinationAvailable(false)
{
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

void cwTripCalibration::setDeclinationManual(double value)
{
    if (m_data.declinationManual() != value) {
        m_data.setDeclinationManual(value);
        emit declinationManualChanged(value);
        emit calibrationsChanged();
        refreshResolved();
    }
}

void cwTripCalibration::setAutoDeclination(bool value)
{
    if (m_data.autoDeclination() != value) {
        m_data.setAutoDeclination(value);
        emit autoDeclinationChanged(value);
        emit calibrationsChanged();
        refreshResolved();
    }
}

void cwTripCalibration::setImportedDeclination(double value)
{
    setDeclinationManual(value);
    setAutoDeclination(false);
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

void cwTripCalibration::setData(const cwTripCalibrationData &data) {
    setCorrectedCompassBacksight(data.hasCorrectedCompassBacksight());
    setCorrectedClinoBacksight(data.hasCorrectedClinoBacksight());
    setCorrectedCompassFrontsight(data.hasCorrectedCompassFrontsight());
    setCorrectedClinoFrontsight(data.hasCorrectedClinoFrontsight());

    setTapeCalibration(data.tapeCalibration());
    setFrontCompassCalibration(data.frontCompassCalibration());
    setFrontClinoCalibration(data.frontClinoCalibration());
    setBackCompassCalibration(data.backCompassCalibration());
    setBackClinoCalibration(data.backClinoCalibration());

    // Batch the two declination fields so refreshResolved runs once at the end,
    // not twice (once per setter).
    const bool manualChanged = m_data.declinationManual() != data.declinationManual();
    const bool autoChanged = m_data.autoDeclination() != data.autoDeclination();
    if (manualChanged) {
        m_data.setDeclinationManual(data.declinationManual());
        emit declinationManualChanged(data.declinationManual());
    }
    if (autoChanged) {
        m_data.setAutoDeclination(data.autoDeclination());
        emit autoDeclinationChanged(data.autoDeclination());
    }
    if (manualChanged || autoChanged) {
        emit calibrationsChanged();
        refreshResolved();
    }

    setDistanceUnit(data.distanceUnit());

    setFrontSights(data.hasFrontSights());
    setBackSights(data.hasBackSights());
}

void cwTripCalibration::setParentTrip(cwTrip* trip)
{
    if (m_parentTrip == trip) {
        return;
    }

    // Pull any active warning off the old parent before we forget it exists.
    clearActiveDeclinationWarning();

    if (m_parentTrip) {
        disconnect(m_parentTrip, nullptr, this, nullptr);
    }

    m_parentTrip = trip;

    if (m_parentTrip) {
        connect(m_parentTrip, &cwTrip::dateChanged,
                this, &cwTripCalibration::refreshResolved);
        connect(m_parentTrip, &cwTrip::parentCaveChanged,
                this, &cwTripCalibration::rewireCaveSignals);
    }

    rewireCaveSignals();
}

void cwTripCalibration::rewireCaveSignals()
{
    cwCave* newCave = m_parentTrip ? m_parentTrip->parentCave() : nullptr;
    if (m_wiredCave == newCave) {
        refreshResolved();
        return;
    }

    if (m_wiredCave && m_wiredCave->fixStations()) {
        disconnect(m_wiredCave->fixStations(), nullptr, this, nullptr);
    }

    m_wiredCave = newCave;

    if (m_wiredCave && m_wiredCave->fixStations()) {
        auto* fixModel = m_wiredCave->fixStations();
        connect(fixModel, &QAbstractItemModel::dataChanged,
                this, &cwTripCalibration::refreshResolved);
        connect(fixModel, &QAbstractItemModel::rowsInserted,
                this, &cwTripCalibration::refreshResolved);
        connect(fixModel, &QAbstractItemModel::rowsRemoved,
                this, &cwTripCalibration::refreshResolved);
        connect(fixModel, &QAbstractItemModel::modelReset,
                this, &cwTripCalibration::refreshResolved);
    }

    refreshResolved();
}

Monad::Result<double> cwTripCalibration::resolveAuto() const
{
    if (!m_parentTrip || !m_wiredCave || !m_wiredCave->fixStations()
        || m_wiredCave->fixStations()->count() == 0) {
        return Monad::Result<double>(QStringLiteral("No fix station available"));
    }

    const cwFixStation fix = m_wiredCave->fixStations()->fixStationAt(0);
    QString sourceCS = fix.inputCS().trimmed();
    if (sourceCS.isEmpty()) {
        if (auto* region = m_wiredCave->parentRegion()) {
            sourceCS = region->geoReference()->globalCoordinateSystem().trimmed();
        }
    }
    const cwGeoPoint location(fix.easting(), fix.northing(), fix.elevation());
    return cwDeclination::compute(location, sourceCS, m_parentTrip->date());
}

void cwTripCalibration::refreshResolved()
{
    const Monad::Result<double> autoResult = resolveAuto();
    const bool newAvailable = !autoResult.hasError();
    const double newResolved = (m_data.autoDeclination() && newAvailable)
        ? autoResult.value()
        : m_data.declinationManual();

    const bool resolvedChanged = (newResolved != m_cachedResolvedDeclination);
    const bool availableChanged = (newAvailable != m_cachedAutoDeclinationAvailable);

    m_cachedResolvedDeclination = newResolved;
    m_cachedAutoDeclinationAvailable = newAvailable;

    // External inputs don't dirty stored data, so calibrationsChanged is
    // emitted only by the explicit setters, not here.
    if (resolvedChanged) {
        emit declinationChanged(newResolved);
    }
    if (availableChanged) {
        emit autoDeclinationAvailableChanged(newAvailable);
    }

    updateWarnings(autoResult);
}

void cwTripCalibration::updateWarnings(const Monad::Result<double>& autoResult)
{
    QString newWarning;

    const QDateTime tripDate = m_parentTrip ? m_parentTrip->date() : QDateTime();
    const bool autoSucceeds = !autoResult.hasError();

    if (m_data.autoDeclination() && !tripDate.isValid()) {
        newWarning = QStringLiteral(
            "Trip has no date; auto declination unavailable. "
            "Using stored manual value.");
    } else if (!m_data.autoDeclination() && autoSucceeds) {
        const double computed = autoResult.value();
        const double manual = m_data.declinationManual();
        const double diff = std::abs(manual - computed);
        if (diff >= 0.5) {
            newWarning = QStringLiteral(
                "Manual declination %1° differs from computed %2° by %3°. "
                "Verify it's still correct.")
                .arg(manual, 0, 'f', 1)
                .arg(computed, 0, 'f', 1)
                .arg(diff, 0, 'f', 1);
        }
    }

    const QString previousWarning = m_declinationWarningError.message();
    if (newWarning == previousWarning) {
        return;
    }

    if (!previousWarning.isEmpty() && !newWarning.isEmpty() && m_parentTrip) {
        // Text-only change: update the existing row in place so QML delegates
        // survive the edit and warningCount stays quiet.
        auto* errors = m_parentTrip->errorModel()->errors();
        const int row = errors->indexOf(m_declinationWarningError);
        if (row >= 0) {
            errors->setData(errors->index(row),
                            newWarning,
                            static_cast<int>(cwErrorListModel::ErrorRoles::MessageRole));
            m_declinationWarningError.setMessage(newWarning);
            emit declinationWarningChanged(newWarning);
            return;
        }
    }

    clearActiveDeclinationWarning();
    if (!newWarning.isEmpty() && m_parentTrip) {
        m_declinationWarningError = cwError(newWarning, cwError::Warning);
        m_parentTrip->errorModel()->errors()->append(m_declinationWarningError);
    }

    emit declinationWarningChanged(newWarning);
}

void cwTripCalibration::clearActiveDeclinationWarning()
{
    // remove() is a no-op when the entry isn't in the list, so a
    // default-constructed m_declinationWarningError is safe here.
    if (m_parentTrip) {
        m_parentTrip->errorModel()->errors()->remove(m_declinationWarningError);
    }
    m_declinationWarningError = cwError();
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
