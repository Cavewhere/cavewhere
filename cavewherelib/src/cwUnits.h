/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWUNITCOVERTER_H
#define CWUNITCOVERTER_H


//Qt includes
#include <QMetaType>
#include <QObject>
#include <QQmlEngine>
#include <array>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwUnits : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Units)
    QML_SINGLETON

public:
    // !!NOTICE!! Changing the enum effects SAVE / LOAD and the cwUnit Code !!NOTICE!!
    enum LengthUnit {
        Inches = 0,       //!< Inches
        Feet = 1,       //!< Feet
        Yards = 2,       //!< Yards
        Meters = 3,        //!< Meters
        Millimeters = 4,       //!< Millimeters
        Centimeters = 5,       //!< Centimeters
        Kilometers = 6,       //!< Kilometers
        LengthUnitless = 7,  //!< Invalid units or unit less
        Miles = 8
    };

    // !!NOTICE!! Changing the enum effects SAVE / LOAD and the cwUnit Code !!NOTICE!!
    enum ImageResolutionUnit {
        DotsPerInch = 0,
        DotsPerCentimeter = 1,
        DotsPerMeter = 2
    };

    // The project-wide display-unit choice. Every concrete unit shown to the
    // user is derived from this via the helpers below, so there is no per-unit
    // settings matrix. // !!NOTICE!! persisted to SAVE / LOAD !!NOTICE!!
    enum UnitSystem {
        Metric = 0,
        Imperial = 1
    };

    //! How a scale bar picks its unit system, independent of the project's own
    //! setting. FollowProject tracks the project's UnitSystem live;
    //! ForceMetric/ForceImperial pin one bar to a system regardless of the
    //! project. Shared by the on-screen view bar (ScaleBar.qml) and the per-layer
    //! export bar (cwCaptureViewport, where the value is persisted, #470).
    //! Values match the Metric/Imperial combobox index (0/1/2 with FollowProject).
    enum ScaleBarUnitMode {
        FollowProject = 0,
        ForceMetric = 1,
        ForceImperial = 2
    };

    Q_ENUM(LengthUnit)
    Q_ENUM(ImageResolutionUnit)
    Q_ENUM(UnitSystem)
    Q_ENUM(ScaleBarUnitMode)

    static constexpr double PointsPerInch = 72.0;
    static constexpr double SvgCssDpi = 96.0;

    static constexpr double convert(double value,
                                    cwUnits::LengthUnit from,
                                    cwUnits::LengthUnit to);
    static QStringList lengthUnitNames();
    static QString unitName(cwUnits::LengthUnit unit);
    static cwUnits::LengthUnit toLengthUnit(QString unitString);

    //! A length in \a meters, converted to \a unit and rendered at \a decimals
    //! places with the unit suffix (e.g. "196.85 ft"). With \a signedValue a
    //! positive value gets an explicit '+'; a value that rounds to zero carries
    //! no sign (a rounded -0 renders a clean "0.00 <unit>").
    static QString formatLength(double meters, cwUnits::LengthUnit unit,
                                bool signedValue = false, int decimals = 2);

    //! The unit new survey shots default to for \a system (metres / feet).
    static constexpr cwUnits::LengthUnit surveyUnit(cwUnits::UnitSystem system);
    //! The unit cave depth is displayed in for \a system (metres / feet).
    static constexpr cwUnits::LengthUnit depthUnit(cwUnits::UnitSystem system);
    //! The base (short-distance) unit for \a system (metres / feet) — the small
    //! half of the small/large pair magnitudeUnit() chooses between.
    static constexpr cwUnits::LengthUnit smallLengthUnit(cwUnits::UnitSystem system);
    //! The large-distance unit for \a system (kilometres / miles), used once a
    //! length reaches one large unit — see magnitudeUnit().
    static constexpr cwUnits::LengthUnit largeLengthUnit(cwUnits::UnitSystem system);
    //! Pick the sensible display unit for \a meters in \a system: the small unit
    //! (m / ft) below one large unit, the large unit (km / mi) at or above it.
    //! Drives the cave-length stat and both scale bars.
    static constexpr cwUnits::LengthUnit magnitudeUnit(double meters, cwUnits::UnitSystem system);
    //! The display name of a unit system ("Metric" / "Imperial").
    static QString unitName(cwUnits::UnitSystem system);
    //! The unit-system names in enum order — a combobox model.
    static QStringList unitSystemNames();

    // --- QML-facing display helpers ---------------------------------------
    // The static methods above are the single source of truth for unit
    // derivation and conversion; these thin instance forwarders exist only so
    // QML (the view scale bar, the cave length/depth stats) can resolve a
    // display unit live from a project's UnitSystem. cwUnits is a QML_SINGLETON,
    // so call them off the type directly, e.g. Units.lengthDisplayUnit(meters, system).
    Q_INVOKABLE cwUnits::LengthUnit lengthDisplayUnit(double meters, cwUnits::UnitSystem system) const {
        return magnitudeUnit(meters, system);
    }
    Q_INVOKABLE cwUnits::LengthUnit depthDisplayUnit(cwUnits::UnitSystem system) const {
        return depthUnit(system);
    }
    Q_INVOKABLE double convertLength(double value, cwUnits::LengthUnit from, cwUnits::LengthUnit to) const {
        return convert(value, from, to);
    }
    Q_INVOKABLE QString lengthUnitName(cwUnits::LengthUnit unit) const {
        return unitName(unit);
    }
    //! The display name of a unit system ("Metric" / "Imperial") — feeds the
    //! Metric/Imperial comboboxes (settings, project, scale-bar controls).
    Q_INVOKABLE QString unitSystemName(cwUnits::UnitSystem system) const {
        return unitName(system);
    }

    static constexpr double convert(double value,
                                    cwUnits::ImageResolutionUnit from,
                                    cwUnits::ImageResolutionUnit to);
    static QStringList imageResolutionUnitNames();
    static QString unitName(cwUnits::ImageResolutionUnit unit);
    static cwUnits::ImageResolutionUnit toImageResolutionUnit(QString unitString);

private:
    inline static constexpr std::array<double, Miles + 1> LengthUnitsToMeters = {
        0.0254, //Inches
        0.3048, //Feet
        0.9144, //Yard
        1.0, //Meter
        0.001, //millimeter
        0.01, //cm
        1000.0, //km
        0.0, //Unitless
        1609.340 //Miles
    };

    inline static constexpr std::array<double, DotsPerMeter + 1> ResolutionUnitToDotPerMeters = {
        39.3700787, //Dots per inch
        100.0, //Dots per centimeter
        1.0, //Dots per meter
    };

    static constexpr double convert(double value, double fromFactor, double toFactor);

};

/**
 * @brief cwUnits::convert
 *
 * Converts the value to a different unit using the fromFactor and toFactor
 *
 * @param value
 * @param fromFactor - The factor that puts the a common unit
 * @param toFactor - The factor that puts the value into a common unit
 */
inline constexpr double cwUnits::convert(double value, double fromFactor, double toFactor)
{
    return value * fromFactor / toFactor;
}

inline constexpr double cwUnits::convert(double value,
                                         cwUnits::LengthUnit from,
                                         cwUnits::LengthUnit to)
{
    if (from == LengthUnitless || to == LengthUnitless) {
        return value;
    }
    if (to < 0 || to > Miles) {
        return value;
    }
    if (from < 0 || from > Miles) {
        return value;
    }
    return convert(value, LengthUnitsToMeters[from], LengthUnitsToMeters[to]);
}

inline constexpr cwUnits::LengthUnit cwUnits::smallLengthUnit(cwUnits::UnitSystem system)
{
    return system == Imperial ? Feet : Meters;
}

inline constexpr cwUnits::LengthUnit cwUnits::surveyUnit(cwUnits::UnitSystem system)
{
    return smallLengthUnit(system);
}

inline constexpr cwUnits::LengthUnit cwUnits::depthUnit(cwUnits::UnitSystem system)
{
    return smallLengthUnit(system);
}

inline constexpr cwUnits::LengthUnit cwUnits::largeLengthUnit(cwUnits::UnitSystem system)
{
    return system == Imperial ? Miles : Kilometers;
}

inline constexpr cwUnits::LengthUnit cwUnits::magnitudeUnit(double meters, cwUnits::UnitSystem system)
{
    // The crossover is exactly one large unit, so a value never renders below
    // 1.0 of the large unit (e.g. 999 m stays "999 m", 1000 m becomes "1 km").
    const cwUnits::LengthUnit large = largeLengthUnit(system);
    return qAbs(meters) >= LengthUnitsToMeters[large] ? large : smallLengthUnit(system);
}

inline constexpr double cwUnits::convert(double value,
                                         cwUnits::ImageResolutionUnit from,
                                         cwUnits::ImageResolutionUnit to)
{
    if (to < 0 || to > DotsPerMeter) {
        return value;
    }
    if (from < 0 || from > DotsPerMeter) {
        return value;
    }
    return convert(value, ResolutionUnitToDotPerMeters[from], ResolutionUnitToDotPerMeters[to]);
}


#endif // CWUNITCOVERTER_H
