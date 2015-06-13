#ifndef DEWALLS_DEFAULTUNIT_H
#define DEWALLS_DEFAULTUNIT_H

#include "unit.h"

namespace dewalls {

template<class T>
class DefaultUnit : public Unit<T>
{
public:
    DefaultUnit(QString name, const T *type, long double toBase, long double fromBase);
    DefaultUnit(QString name, const T *type, long double quantity, const DefaultUnit<T> *inTermsOf);

    double toBase(double quantity) const;
    double fromBase(double quantity) const;

private:
    const long double _toBase;
    const long double _fromBase;
};

template<class T>
inline DefaultUnit<T>::DefaultUnit(QString name, const T *type, long double toBase, long double fromBase) :
    Unit<T>(name, type),
    _toBase(toBase),
    _fromBase(fromBase)
{

}

template<class T>
inline DefaultUnit<T>::DefaultUnit(QString name, const T *type, long double quantity, const DefaultUnit<T> *inTermsOf) :
    Unit<T>(name, type),
    _toBase(quantity * inTermsOf->_toBase),
    _fromBase(quantity * inTermsOf->_fromBase)
{

}

template<class T>
inline double DefaultUnit<T>::toBase(double quantity) const
{
    return quantity * _toBase;
}

template<class T>
inline double DefaultUnit<T>::fromBase(double quantity) const
{
    return quantity * _fromBase;
}

} // namespace dewalls

#endif // DEWALLS_DEFAULTUNIT_H
