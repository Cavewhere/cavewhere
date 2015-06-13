#ifndef DEWALLS_UNITTYPE_H
#define DEWALLS_UNITTYPE_H

#include <QHash>
#include <QString>
#include "unit.h"

namespace dewalls {

template<class T>
class UnitType
{
public:
    QString name() const;
    QHash<QString, const Unit<T> *> units() const;
    const Unit<T> *baseUnit() const;
    const Unit<T> *unit(QString name) const;

    double convert(double quantity, const Unit<T> *fromUnit, const Unit<T> *toUnit) const;

protected:
    UnitType(QString name);
    void addUnit(const Unit<T> *unit);
    void setBaseUnit(const Unit<T> *baseUnit);

private:
    UnitType() = delete;
    UnitType(const UnitType<T> &that) = delete;

    void operator=(UnitType const&) = delete;

    QString _name;
    const Unit<T> *_baseUnit;
    QHash<QString, const Unit<T> *> _units;
};

template<class T>
inline UnitType<T>::UnitType(QString name)
    : _name(name),
      _units(QHash<QString, const Unit<T> *>())
{

}

template<class T>
inline QString UnitType<T>::name() const
{
    return _name;
}

template<class T>
inline const Unit<T> *UnitType<T>::baseUnit() const
{
    return _baseUnit;
}

template<class T>
inline const Unit<T> *UnitType<T>::unit(QString name) const
{
    return _units[name];
}

template<class T>
inline void UnitType<T>::addUnit(const Unit<T> *unit)
{
    _units[unit->name()] = unit;
}

template<class T>
inline void UnitType<T>::setBaseUnit(const Unit<T> *baseUnit)
{
   _baseUnit = baseUnit;
}

template<class T>
inline double UnitType<T>::convert(double quantity, const Unit<T> *fromUnit, const Unit<T> *toUnit) const
{
    return fromUnit == toUnit ? quantity :
                                toUnit->fromBase(fromUnit->toBase(quantity));
}

} // namespace dewalls

#endif // DEWALLS_UNITTYPE_H
