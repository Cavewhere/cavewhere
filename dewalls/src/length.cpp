#include "length.h"
#include "defaultunit.h"

namespace dewalls {

Length::Length():
    UnitType<Length>("length")
{
    auto meters 		= new DefaultUnit<Length>("m", this, 1.0L, 1.0L);
    auto centimeters 	= new DefaultUnit<Length>("cm", this, 0.001L, meters);
    auto kilometers 	= new DefaultUnit<Length>("km", this, 1000.0L, meters);
    auto feet   		= new DefaultUnit<Length>("ft", this, 0.3048L, 1.0L / 0.3048L);
    auto yards   		= new DefaultUnit<Length>("yd", this, 3.0L, feet);
    auto inches			= new DefaultUnit<Length>("in", this, 1.0L / 12.0L, feet);

    addUnit(meters);
    addUnit(centimeters);
    addUnit(kilometers);
    addUnit(feet);
    addUnit(yards);
    addUnit(inches);
}

const Length * const Length::type = new Length();
const Unit<Length> * const Length::meters = Length::type->unit("m");
const Unit<Length> * const Length::m = Length::meters;
const Unit<Length> * const Length::centimeters = Length::type->unit("cm");
const Unit<Length> * const Length::cm = Length::centimeters;
const Unit<Length> * const Length::kilometers = Length::type->unit("km");
const Unit<Length> * const Length::km = Length::kilometers;
const Unit<Length> * const Length::feet = Length::type->unit("ft");
const Unit<Length> * const Length::ft = Length::feet;
const Unit<Length> * const Length::yards = Length::type->unit("yd");
const Unit<Length> * const Length::yd = Length::yards;
const Unit<Length> * const Length::inches = Length::type->unit("in");
const Unit<Length> * const Length::in = Length::inches;

} // namespace dewalls

