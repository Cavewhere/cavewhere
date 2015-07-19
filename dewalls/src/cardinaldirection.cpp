#include "cardinaldirection.h"

namespace dewalls {

namespace CardinalDirection {

CardinalDirection opposite(CardinalDirection d)
{
    switch (d)
    {
    case North:
        return South;
    case South:
        return North;
    case East:
        return West;
    case West:
        return East;
    }
    return d;
}

UnitizedDouble<Angle> angle(CardinalDirection d)
{
    return UnitizedDouble<Angle>(90 * d, Angle::deg());
}

CardinalDirection fromChar(char c)
{
    switch(c)
    {
    case 'n':
    case 'N':
        return North;
    case 's':
    case 'S':
        return South;
    case 'e':
    case 'E':
        return East;
    case 'w':
    case 'W':
        return West;
    }
    throw std::invalid_argument("invalid character");
}

UnitizedDouble<Angle> nonnorm_quadrant(CardinalDirection from, CardinalDirection to, UnitizedDouble<Angle> rotation)
{
    if (to == (from + 1) % 4)
    {
        return angle(from) + rotation;
    }
    else if (from == (to + 1) % 4)
    {
        return angle(from) - rotation;
    }
    throw std::invalid_argument("invalid from/to combination");
}

UnitizedDouble<Angle> quadrant(CardinalDirection from, CardinalDirection to, UnitizedDouble<Angle> rotation)
{
    UnitizedDouble<Angle> result = nonnorm_quadrant(from, to, rotation) % UnitizedDouble<Angle>(360.0, Angle::deg());
    if (result < angle(North)) {
        result += UnitizedDouble<Angle>(360.0, Angle::deg());
    }
    return result;
}

} // namespace CardinalDirection

} // namespace dewalls

