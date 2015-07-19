#include "length.h"
#include "defaultunit.h"
#include <QReadWriteLock>
#include <QReadLocker>

namespace dewalls {

Length::Length()
    : UnitType<Length>("length")
{
    _meters = new DefaultUnit<Length>("m", this, 1.0L, 1.0L);
    _centimeters = new DefaultUnit<Length>("cm", this, 0.001L, _meters);
    _kilometers = new DefaultUnit<Length>("km", this, 1000.0L, _meters);
    _feet = new DefaultUnit<Length>("ft", this, 0.3048L, 1.0L / 0.3048L);
    _yards = new DefaultUnit<Length>("yd", this, 3.0L, _feet);
    _inches = new DefaultUnit<Length>("in", this, 1.0L / 12.0L, _feet);

    addUnit(_meters);
    addUnit(_centimeters);
    addUnit(_kilometers);
    addUnit(_feet);
    addUnit(_yards);
    addUnit(_inches);
}

QSharedPointer<Length> Length::_type = QSharedPointer<Length>();

void Length::init()
{
    if (_type.isNull())
    {
        _type = QSharedPointer<Length>(new Length());
    }
}

} // namespace dewalls

