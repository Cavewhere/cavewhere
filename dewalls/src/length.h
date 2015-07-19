#ifndef DEWALLS_LENGTH_H
#define DEWALLS_LENGTH_H

#include <QString>
#include "unittype.h"
#include "defaultunit.h"
#include <QSharedPointer>

namespace dewalls {

class Length : public UnitType<Length>
{
public:
    static void init();

    static const Length * 	   const type() { return _type.data(); }
    static const Unit<Length> * const meters() { return _type->_meters; }
    static const Unit<Length> * const m() { return _type->_meters; }
    static const Unit<Length> * const centimeters() { return _type->_centimeters; }
    static const Unit<Length> * const cm() { return _type->_centimeters; }
    static const Unit<Length> * const kilometers() { return _type->_kilometers; }
    static const Unit<Length> * const km() { return _type->_kilometers; }
    static const Unit<Length> * const feet() { return _type->_feet; }
    static const Unit<Length> * const ft() { return _type->_feet; }
    static const Unit<Length> * const yards() { return _type->_yards; }
    static const Unit<Length> * const yd() { return _type->_yards; }
    static const Unit<Length> * const inches() { return _type->_inches; }
    static const Unit<Length> * const in() { return _type->_inches; }

private:
    Length();

    static QSharedPointer<Length> _type;
    const DefaultUnit<Length>* _meters;
    const DefaultUnit<Length>* _centimeters;
    const DefaultUnit<Length>* _kilometers;
    const DefaultUnit<Length>* _feet;
    const DefaultUnit<Length>* _yards;
    const DefaultUnit<Length>* _inches;
};

} // namespace dewalls

#endif // DEWALLS_LENGTH_H
