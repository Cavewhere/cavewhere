#ifndef DEWALLS_CARDINALDIRECTION_H
#define DEWALLS_CARDINALDIRECTION_H

#include "angle.h"
#include "unitizeddouble.h"

namespace dewalls {

namespace CardinalDirection {

enum CardinalDirection
{
    North = 0,
    East = 1,
    South = 2,
    West = 3
};

CardinalDirection opposite(CardinalDirection d);
UnitizedDouble<Angle> angle(CardinalDirection d);
CardinalDirection fromChar(char c);
UnitizedDouble<Angle> quadrant(CardinalDirection from, CardinalDirection to, UnitizedDouble<Angle> angle);

} // namespace CardinalDirection

} // namespace dewalls

#endif // DEWALLS_CARDINALDIRECTION_H
