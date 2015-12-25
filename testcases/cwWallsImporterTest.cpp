#include "catch.hpp"
#include "cwWallsImporter.h"
#include "wallsunits.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

typedef UnitizedDouble<Length> ULength;
typedef UnitizedDouble<Angle> UAngle;

using namespace dewalls;

TEST_CASE( "importCalibrations", "[cwWallsImporter]" ) {
    WallsUnits units;
    cwTrip trip;

    Length::Unit dUnit = Length::Feet;
    ULength incd(5, Length::Meters);
    UAngle inca(42, Angle::Gradians);
    UAngle incab(0.253, Angle::Radians);
    UAngle incv(36, Angle::PercentGrade);
    UAngle incvb(25, Angle::Degrees);
    UAngle decl(24, Angle::Gradians);

    units.setDUnit(dUnit);
    units.setIncd(incd);
    units.setInca(inca);
    units.setIncab(incab);
    units.setIncv(incv);
    units.setIncvb(incvb);
    units.setTypeabCorrected(true);
    units.setTypevbCorrected(true);
    units.setDecl(decl);

    cwWallsImporter::importCalibrations(units, trip);

    CHECK( trip.calibrations()->hasCorrectedCompassBacksight() );
    CHECK( trip.calibrations()->hasCorrectedClinoBacksight() );
    CHECK( trip.calibrations()->distanceUnit() == cwUnits::LengthUnit::Feet );
    CHECK( trip.calibrations()->tapeCalibration() == incd.get(dUnit) );
    CHECK( trip.calibrations()->frontCompassCalibration() == inca.get(Angle::Degrees) );
    CHECK( trip.calibrations()->backCompassCalibration() == incab.get(Angle::Degrees) );
    CHECK( trip.calibrations()->frontClinoCalibration() == incv.get(Angle::Degrees) );
    CHECK( trip.calibrations()->backClinoCalibration() == incvb.get(Angle::Degrees) );
    CHECK( trip.calibrations()->declination() == decl.get(Angle::Degrees) );

    incd = ULength(4, Length::Feet);
    dUnit = Length::Meters;
    units.setDUnit(dUnit);
    units.setIncd(incd);
    cwWallsImporter::importCalibrations(units, trip);
    CHECK( trip.calibrations()->distanceUnit() == cwUnits::Meters );
    CHECK( trip.calibrations()->tapeCalibration() == incd.get(dUnit) );

    units.setTypeabCorrected(false);
    units.setTypevbCorrected(true);
    cwWallsImporter::importCalibrations(units, trip);
    CHECK( !trip.calibrations()->hasCorrectedCompassBacksight() );
    CHECK( trip.calibrations()->hasCorrectedClinoBacksight() );

    units.setTypeabCorrected(false);
    units.setTypevbCorrected(false);
    cwWallsImporter::importCalibrations(units, trip);
    CHECK( !trip.calibrations()->hasCorrectedCompassBacksight() );
    CHECK( !trip.calibrations()->hasCorrectedClinoBacksight() );

    units.setTypeabCorrected(true);
    units.setTypevbCorrected(false);
    cwWallsImporter::importCalibrations(units, trip);
    CHECK( trip.calibrations()->hasCorrectedCompassBacksight() );
    CHECK( !trip.calibrations()->hasCorrectedClinoBacksight() );
}
