#ifndef UNIT_H
#define UNIT_H

#include <QString>

namespace dewalls {

template<class T>
class Unit
{
public:
    QString name() const;
    const T *type() const;

    virtual double toBase(double quantity) const = 0;
    virtual double fromBase(double quantity) const = 0;

    double convert(double quantity, const Unit<T> *toUnit) const;

protected:
    Unit(QString name, const T *type);

private:
    Unit() = delete;
    Unit(const Unit &that) = delete;

    QString _name;
    const T *_type;
};

template<class T>
inline std::ostream& operator<<(std::ostream& os, const Unit<T>& unit)
{
    return os << unit.name().toStdString();
}

template<class T>
inline Unit<T>::Unit(QString name, const T *type) :
    _name(name),
    _type(type)
{

}

template<class T>
inline QString Unit<T>::name() const
{
    return _name;
}

template<class T>
inline const T *Unit<T>::type() const
{
    return _type;
}

template<class T>
inline double Unit<T>::convert(double quantity, const Unit<T> *toUnit) const
{
    return toUnit ? _type->convert(quantity, this, toUnit) : NAN;
}

}

#endif // UNIT_H
