#ifndef DEWALLS_UNITIZEDDOUBLE_H
#define DEWALLS_UNITIZEDDOUBLE_H

#include <iostream>
#include "unit.h"

namespace dewalls {

template<class T>
class UnitizedDouble
{
public:
    UnitizedDouble(double quantity, const Unit<T> *unit);
    UnitizedDouble(UnitizedDouble&& other);

    const Unit<T> *unit() const;
    double get(const Unit<T> *toUnit) const;
    UnitizedDouble<T> in(const Unit<T> *unit) const;

    friend void swap(UnitizedDouble<T>& first, UnitizedDouble<T>& second)
    {
        using std::swap;

        swap(first._quantity, second._quantity);
        swap(first._unit, second._unit);
    }

    UnitizedDouble<T>& operator =(UnitizedDouble<T> other);
    UnitizedDouble<T>& operator +=(const UnitizedDouble<T>& rhs);
    UnitizedDouble<T>  operator -();
    UnitizedDouble<T>& operator -=(const UnitizedDouble<T>& rhs);
    UnitizedDouble<T>& operator *=(const double& rhs);
    UnitizedDouble<T>& operator /=(const double& rhs);
    UnitizedDouble<T>& operator %=(const UnitizedDouble<T>& rhs);

    QString toString() const;

    friend std::ostream& operator<<(std::ostream& os, const UnitizedDouble<T>& obj)
    {
        return os << obj._quantity << ' ' << *obj._unit;
    }

private:
    UnitizedDouble();

    const Unit<T> *_unit;
    double _quantity;
};

template<class T>
inline UnitizedDouble<T>::UnitizedDouble()
{

}

template<class T>
inline UnitizedDouble<T>::UnitizedDouble(UnitizedDouble&& other)
    : UnitizedDouble()
{
    swap(*this, other);
}

template<class T>
inline UnitizedDouble<T>& UnitizedDouble<T>::operator =(UnitizedDouble<T> other)
{
    swap(*this, other);
    return *this;
}

template<class T>
inline UnitizedDouble<T>& UnitizedDouble<T>::operator +=(const UnitizedDouble<T>& rhs)
{
    _quantity += rhs.get(_unit);
    return *this;
}

template<class T>
inline UnitizedDouble<T> UnitizedDouble<T>::operator -()
{
    return UnitizedDouble<T>(-_quantity, _unit);
}

template<class T>
inline UnitizedDouble<T>& UnitizedDouble<T>::operator -=(const UnitizedDouble<T>& rhs)
{
    _quantity -= rhs.get(_unit);
    return *this;
}

template<class T>
inline UnitizedDouble<T>& UnitizedDouble<T>::operator *=(const double& rhs)
{
    _quantity *= rhs;
    return *this;
}

template<class T>
inline UnitizedDouble<T>& UnitizedDouble<T>::operator /=(const double& rhs)
{
    _quantity /= rhs;
    return *this;
}

template<class T>
inline UnitizedDouble<T>& UnitizedDouble<T>::operator %=(const UnitizedDouble<T>& rhs)
{
    _quantity = fmod(_quantity, rhs.get(_unit));
    return *this;
}

template<class T>
inline bool operator ==(const UnitizedDouble<T>& lhs, const UnitizedDouble<T>& rhs)
{
    return lhs.get(lhs.unit()) == rhs.get(lhs.unit());
}

template<class T>
inline bool operator !=(const UnitizedDouble<T>& lhs, const UnitizedDouble<T>& rhs)
{
    return !operator ==(lhs, rhs);
}

template<class T>
inline bool operator < (const UnitizedDouble<T>& lhs, const UnitizedDouble<T>& rhs)
{
    return lhs.get(lhs.unit()) < rhs.get(lhs.unit());
}

template<class T>
inline bool operator > (const UnitizedDouble<T>& lhs, const UnitizedDouble<T>& rhs)
{
    return operator < (rhs, lhs);
}

template<class T>
inline bool operator <=(const UnitizedDouble<T>& lhs, const UnitizedDouble<T>& rhs)
{
    return !operator > (rhs, lhs);
}

template<class T>
inline bool operator >=(const UnitizedDouble<T>& lhs, const UnitizedDouble<T>& rhs)
{
    return !operator < (rhs, lhs);
}

template<class T>
inline UnitizedDouble<T> operator +(UnitizedDouble<T> lhs, const UnitizedDouble<T>& rhs)
{
    lhs += rhs;
    return lhs;
}

template<class T>
inline UnitizedDouble<T> operator -(UnitizedDouble<T> lhs, const UnitizedDouble<T>& rhs)
{
    lhs -= rhs;
    return lhs;
}

template<class T>
inline UnitizedDouble<T> operator *(UnitizedDouble<T> lhs, const double& rhs)
{
    lhs *= rhs;
    return lhs;
}

template<class T>
inline UnitizedDouble<T> operator *(double lhs, const UnitizedDouble<T>& rhs)
{
    return rhs * lhs;
}

template<class T>
inline UnitizedDouble<T> operator /(UnitizedDouble<T> lhs, const double& rhs)
{
    lhs /= rhs;
    return lhs;
}

template<class T>
inline UnitizedDouble<T> operator /(double lhs, const UnitizedDouble<T>& rhs)
{
    return UnitizedDouble<T>(lhs / rhs.get(rhs.unit()), rhs.unit());
}

template<class T>
inline double operator / (const UnitizedDouble<T>& lhs, const UnitizedDouble<T>& rhs)
{
    return lhs.get(lhs.unit()) / rhs.get(lhs.unit());
}

template<class T>
inline UnitizedDouble<T> operator %(UnitizedDouble<T> lhs, const UnitizedDouble<T>& rhs)
{
    lhs %= rhs;
    return lhs;
}

template<class T>
inline UnitizedDouble<T>::UnitizedDouble(double quantity, const Unit<T> *unit):
    _unit(unit),
    _quantity(quantity)
{

}

template<class T>
inline const Unit<T> *UnitizedDouble<T>::unit() const
{
    return _unit;
}

template<class T>
inline double UnitizedDouble<T>::get(const Unit<T> *toUnit) const
{
    return _unit->convert(_quantity, toUnit);
}

template<class T>
inline UnitizedDouble<T> UnitizedDouble<T>::in(const Unit<T> *unit) const
{
    return UnitizedDouble<T>(get(unit), unit);
}

template<class T>
inline QString UnitizedDouble<T>::toString() const
{
    return QString("%1").arg(_quantity) + " " + _unit->name();
}

} // namespace dewalls

#endif // DEWALLS_UNITIZEDDOUBLE_H
