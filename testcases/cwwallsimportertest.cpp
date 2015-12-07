#include "catch.hpp"
#include "cwWallsImporter.h"
#include "wallsunits.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

typedef UnitizedDouble<Length> ULength;
typedef UnitizedDouble<Angle> UAngle;
typedef const Unit<Length>* LengthUnit;
typedef const Unit<Angle>* AngleUnit;

TEST_CASE( "importCalibrations", "[cwWallsImporter]" ) {
    WallsUnits units;
    cwTrip trip;

    LengthUnit dUnit = Length::feet();
    ULength incd(5, Length::meters());
    UAngle inca(42, Angle::gradians());
    UAngle incab(0.253, Angle::radians());
    UAngle incv(36, Angle::percentGrade());
    UAngle incvb(25, Angle::degrees());
    UAngle decl(24, Angle::gradians());

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
    CHECK( trip.calibrations()->frontCompassCalibration() == inca.get(Angle::degrees()) );
    CHECK( trip.calibrations()->backCompassCalibration() == incab.get(Angle::degrees()) );
    CHECK( trip.calibrations()->frontClinoCalibration() == incv.get(Angle::degrees()) );
    CHECK( trip.calibrations()->backClinoCalibration() == incvb.get(Angle::degrees()) );
    CHECK( trip.calibrations()->declination() == decl.get(Angle::degrees()) );

    incd = ULength(4, Length::feet());
    dUnit = Length::meters();
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
