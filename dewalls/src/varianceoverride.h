#ifndef DEWALLS_VARIANCEOVERRIDE_H
#define DEWALLS_VARIANCEOVERRIDE_H

#include <QString>
#include <QSharedPointer>
#include "unitizeddouble.h"
#include "length.h"

namespace dewalls {

class FloatedVarianceOverride;
class FloatedTraverseVarianceOverride;

class VarianceOverride
{
public:
    enum class Type
    {
        FLOATED = 0,
        FLOATED_TRAVERSE = 1,
        LENGTH_OVERRIDE = 2,
        RMS_ERROR = 3
    };

    virtual Type type() const = 0;
    virtual QString toString() const = 0;

    static const QSharedPointer<VarianceOverride> FLOATED;
    static const QSharedPointer<VarianceOverride> FLOATED_TRAVERSE;

protected:
    inline VarianceOverride()
    {
    }
};

class FloatedVarianceOverride : public VarianceOverride
{
public:
    inline FloatedVarianceOverride()
        : VarianceOverride()
    {
    }

    inline virtual Type type() const
    {
        return Type::FLOATED;
    }

    inline virtual QString toString() const
    {
        return "?";
    }
};

class FloatedTraverseVarianceOverride : public VarianceOverride
{
public:
    inline FloatedTraverseVarianceOverride()
        : VarianceOverride()
    {
    }

    inline virtual Type type() const
    {
        return Type::FLOATED_TRAVERSE;
    }

    inline virtual QString toString() const
    {
        return "*";
    }
};

class LengthOverride : public VarianceOverride
{
public:
    inline LengthOverride(UnitizedDouble<Length> lengthOverride)
        : VarianceOverride(),
          _lengthOverride(lengthOverride)
    {
    }

    inline virtual Type type() const
    {
        return Type::LENGTH_OVERRIDE;
    }

    inline virtual QString toString() const
    {
        return _lengthOverride.toString();
    }

    inline UnitizedDouble<Length> lengthOverride() const
    {
        return _lengthOverride;
    }

private:
    LengthOverride() = delete;
    LengthOverride(const LengthOverride& that) = delete;

    UnitizedDouble<Length> _lengthOverride;
};

class RMSError : public VarianceOverride
{
public:
    inline RMSError(UnitizedDouble<Length> error)
        : VarianceOverride(),
          _error(error)
    {
    }

    inline virtual Type type() const
    {
        return Type::RMS_ERROR;
    }

    inline virtual QString toString() const
    {
        return "R" + _error.toString();
    }

    inline UnitizedDouble<Length> error() const
    {
        return _error;
    }

private:
    RMSError() = delete;
    RMSError(const RMSError& that) = delete;

    UnitizedDouble<Length> _error;
};

} // namespace dewalls

#endif // DEWALLS_VARIANCEOVERRIDE_H
