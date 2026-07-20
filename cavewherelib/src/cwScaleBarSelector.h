/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCALEBARSELECTOR_H
#define CWSCALEBARSELECTOR_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
#include "cwUnits.h"

//Std includes
#include <vector>

//! The round scale length a scale bar labels: a round \a value in the magnitude
//! \a unit its size calls for (\a valid is false when nothing fits). A QML value
//! type so the on-screen view bar can read the C++ producer's choice directly.
struct cwScaleBarSelection
{
    Q_GADGET
    QML_VALUE_TYPE(scaleBarSelection)
    Q_PROPERTY(bool valid MEMBER valid)
    Q_PROPERTY(double value MEMBER value)
    Q_PROPERTY(cwUnits::LengthUnit unit MEMBER unit)

public:
    bool valid = false;
    double value = 0.0;
    cwUnits::LengthUnit unit = cwUnits::Meters;
};

//! The single source of the "nice number" rule both scale bars round to. The
//! on-screen view bar (ScaleBar.qml) and the printed export bar
//! (cwScaleBarItem, #470) each apply their own fit policy, but they round to the
//! same candidate lengths so a change to the rule can no longer drift between
//! the QML and C++ halves (A1).
class CAVEWHERE_LIB_EXPORT cwScaleBarSelector : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScaleBarSelector)
    QML_SINGLETON

public:
    //! One round scale candidate: the world length in \a meters, that length as a
    //! round \a value in \a unit.
    struct Candidate {
        double meters;
        double value;
        cwUnits::LengthUnit unit;
    };

    //! The ascending round scale lengths ({1,2,5}×10ⁿ) for \a system, each in the
    //! unit its own magnitude calls for (cwUnits::magnitudeUnit — m/ft below one
    //! large unit, km/mi at or above). Cached per system and shared by both scale
    //! bars, so the nice-number rule has a single owner.
    static const std::vector<Candidate>& niceCandidates(cwUnits::UnitSystem system);

    //! Pick the on-screen view bar's per-cell round increment: the candidate
    //! whose \a cells cells come closest to filling the target pixel span
    //! (centred in [\a minTotalWidth, \a maxTotalWidth]). Always returns a real
    //! candidate's increment and unit — the producer, never QML, owns the
    //! "nothing fits" outcome; \a valid reports whether the choice fit the band.
    Q_INVOKABLE cwScaleBarSelection selectViewScale(double pixelsPerMeter,
                                                    double minTotalWidth,
                                                    double maxTotalWidth,
                                                    int cells,
                                                    cwUnits::UnitSystem system) const;
};

#endif // CWSCALEBARSELECTOR_H
