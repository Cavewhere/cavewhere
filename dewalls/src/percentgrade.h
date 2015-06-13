#ifndef DEWALLS_PERCENTGRADE_H
#define DEWALLS_PERCENTGRADE_H

#define _USE_MATH_DEFINES

#include "angle.h"
#include <cmath>

namespace dewalls {

class PercentGrade : public Unit<Angle>
{
public:
    friend class Angle;

    double toBase(double quantity) const;
    double fromBase(double quantity) const;

    PercentGrade(QString name, const Angle *type);
private:
};

inline PercentGrade::PercentGrade(QString name, const Angle *type)
    : Unit<Angle>(name, type)
{

}

inline double PercentGrade::toBase(double quantity) const
{
    return atan(quantity * 0.01L);
}

inline double PercentGrade::fromBase(double quantity) const
{
    return 100L * tan(quantity);
}

} // namespace dewalls

#endif // DEWALLS_PERCENTGRADE_H
