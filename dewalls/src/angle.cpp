#include "angle.h"
#include "defaultunit.h"
#include "percentgrade.h"

const long double PI = acosl(-1.0L);

namespace dewalls {

Angle::Angle():
    UnitType<Angle>("angle")
{
    _degrees 		= new DefaultUnit<Angle>("deg", this, PI / 180.0L, 180.0L / PI);
    _radians 		= new DefaultUnit<Angle>("rad", this, 1.0L, 1.0L);
    _gradians	 	= new DefaultUnit<Angle>("grad", this, PI / 200.0L, 200.0L / PI);
    _milsNATO	 	= new DefaultUnit<Angle>("mil", this, PI / 3200.0L, 3200.0L / PI);
    _percentGrade   = new PercentGrade("%", this);

    addUnit(_degrees);
    addUnit(_radians);
    addUnit(_gradians);
    addUnit(_milsNATO);
    addUnit(_percentGrade);
}

QSharedPointer<Angle> Angle::_type = QSharedPointer<Angle>();

void Angle::init()
{
    if (_type.isNull())
    {
        _type = QSharedPointer<Angle>(new Angle());
    }
}

} // namespace dewalls

