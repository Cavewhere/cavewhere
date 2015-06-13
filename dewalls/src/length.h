#ifndef DEWALLS_LENGTH_H
#define DEWALLS_LENGTH_H

#include <QString>
#include "unittype.h"

namespace dewalls {

class Length : public UnitType<Length>
{
public:
    static const Length * const type;
    static const Unit<Length> * const meters;
    static const Unit<Length> * const m;
    static const Unit<Length> * const centimeters;
    static const Unit<Length> * const cm;
    static const Unit<Length> * const kilometers;
    static const Unit<Length> * const km;
    static const Unit<Length> * const feet;
    static const Unit<Length> * const ft;
    static const Unit<Length> * const yards;
    static const Unit<Length> * const yd;
    static const Unit<Length> * const inches;
    static const Unit<Length> * const in;
private:
    Length();
};

} // namespace dewalls

#endif // DEWALLS_LENGTH_H
