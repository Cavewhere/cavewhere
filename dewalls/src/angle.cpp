#include "angle.h"
#include "defaultunit.h"
#include "percentgrade.h"

const long double PI = acosl(-1.0L);

namespace dewalls {

Angle::Angle():
    UnitType<Angle>("angle")
{
    auto degrees 		= new DefaultUnit<Angle>("deg", this, PI / 180.0L, 180.0L / PI);
    auto radians 		= new DefaultUnit<Angle>("rad", this, 1.0L, 1.0L);
    auto gradians	 	= new DefaultUnit<Angle>("grad", this, PI / 200.0L, 200.0L / PI);
    auto milsNATO	 	= new DefaultUnit<Angle>("mil", this, PI / 3200.0L, 3200.0L / PI);
    auto percentGrade   = new PercentGrade("%", this);

    addUnit(degrees);
    addUnit(radians);
    addUnit(gradians);
    addUnit(milsNATO);
    addUnit(percentGrade);
}

const Angle * const Angle::type = new Angle();
const Unit<Angle> * const Angle::degrees = Angle::type->unit("deg");
const Unit<Angle> * const Angle::deg = Angle::degrees;
const Unit<Angle> * const Angle::radians = Angle::type->unit("rad");
const Unit<Angle> * const Angle::rad = Angle::radians;
const Unit<Angle> * const Angle::gradians = Angle::type->unit("grad");
const Unit<Angle> * const Angle::grad = Angle::gradians;
const Unit<Angle> * const Angle::milsNATO = Angle::type->unit("mil");
const Unit<Angle> * const Angle::percentGrade = Angle::type->unit("%");
const Unit<Angle> * const Angle::percent = Angle::percentGrade;

} // namespace dewalls

