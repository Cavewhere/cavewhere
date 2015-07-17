#ifndef DEWALLS_OPTIONAL_H
#define DEWALLS_OPTIONAL_H

#include <iostream>

namespace dewalls {

template<typename T>
class Optional
{
public:
    inline Optional() { _hasValue = false; }
    inline Optional(const T& value) { _hasValue = true; _value = value; }
    inline Optional(const T* value) {
        _hasValue = value != NULL;
        if (_hasValue) _value = *value;
    }

    inline bool hasValue() const { return _hasValue; }
    inline T value() const { return _value; }
    inline void clear() { _hasValue = false; }

    inline T* operator->() const { return const_cast<T*>(&_value); }

    inline bool operator==(const Optional<T>& other) const
    {
        return _hasValue ? other._hasValue && _value == other._value
                         : !other._hasValue;
    }

    inline bool operator!=(const Optional<T>& other) const
    {
        return !operator==(this, other);
    }

    inline Optional<T>& operator=(const T& value)
    {
        _hasValue = true;
        _value = value;
        return *this;
    }

    inline Optional<T>& operator=(const T* value)
    {
        _hasValue = value != NULL;
        if (_hasValue) _value = *value;
        return *this;
    }

    inline Optional<T> operator-()
    {
        return _hasValue ? Optional<T>(-_value) : Optional<T>();
    }

    inline Optional<T>& operator+=(const Optional<T>& rhs)
    {
        _hasValue &= rhs._hasValue;
        if (_hasValue) _value += rhs._value;
        return *this;
    }

    template<typename U>
    inline Optional<T>& operator+=(const U& rhs)
    {
        if (_hasValue) _value += rhs;
        return *this;
    }

    inline Optional<T>& operator-=(const Optional<T>& rhs)
    {
        _hasValue &= rhs._hasValue;
        if (_hasValue) _value -= rhs._value;
        return *this;
    }

    template<typename U>
    inline Optional<T>& operator-=(const U& rhs)
    {
        if (_hasValue) _value -= rhs;
        return *this;
    }

    inline Optional<T>& operator*=(const Optional<T>& rhs)
    {
        _hasValue &= rhs._hasValue;
        if (_hasValue) _value *= rhs._value;
        return *this;
    }

    template<typename U>
    inline Optional<T>& operator*=(const U& rhs)
    {
        if (_hasValue) _value *= rhs;
        return *this;
    }

    inline Optional<T>& operator/=(const Optional<T>& rhs)
    {
        _hasValue &= rhs._hasValue;
        if (_hasValue) _value /= rhs._value;
        return *this;
    }

    template<typename U>
    inline Optional<T>& operator/=(const U& rhs)
    {
        if (_hasValue) _value /= rhs;
        return *this;
    }

    inline Optional<T>& operator%=(const Optional<T>& rhs)
    {
        _hasValue &= rhs._hasValue;
        if (_hasValue) _value %= rhs._value;
        return *this;
    }

    template<typename U>
    inline Optional<T>& operator%=(const U& rhs)
    {
        if (_hasValue) _value %= rhs;
        return *this;
    }

private:
    bool _hasValue;
    T _value;
};

template<typename T>
inline std::ostream& operator<<(std::ostream& os, Optional<T>& value)
{
    if (!value.hasValue()) {
        return os << "<no value>";
    }
    return os << value.value();
}

template<typename T>
inline Optional<T> operator+(Optional<T> lhs, const Optional<T>& rhs)
{
    lhs += rhs;
    return lhs;
}

template<typename T, typename U>
inline Optional<T> operator+(Optional<T> lhs, const U& rhs)
{
    lhs += rhs;
    return lhs;
}

template<typename T, typename U>
inline Optional<T> operator+(const U& lhs, const Optional<T>& rhs)
{
    return rhs.hasValue() ? Optional<T>(lhs + rhs.value()) : Optional<T>();
}

template<typename T>
inline Optional<T> operator-(Optional<T> lhs, const Optional<T>& rhs)
{
    lhs -= rhs;
    return lhs;
}

template<typename T, typename U>
inline Optional<T> operator-(Optional<T> lhs, const U& rhs)
{
    lhs -= rhs;
    return lhs;
}

template<typename T, typename U>
inline Optional<T> operator-(const U& lhs, const Optional<T>& rhs)
{
    return rhs.hasValue() ? Optional<T>(lhs - rhs.value()) : Optional<T>();
}

template<typename T>
inline Optional<T> operator*(Optional<T> lhs, const Optional<T>& rhs)
{
    lhs *= rhs;
    return lhs;
}

template<typename T, typename U>
inline Optional<T> operator*(Optional<T> lhs, const U& rhs)
{
    lhs *= rhs;
    return lhs;
}

template<typename T, typename U>
inline Optional<T> operator*(const U& lhs, const Optional<T>& rhs)
{
    return rhs.hasValue() ? Optional<T>(lhs * rhs.value()) : Optional<T>();
}

template<typename T>
inline Optional<T> operator/(Optional<T> lhs, const Optional<T>& rhs)
{
    lhs /= rhs;
    return lhs;
}

template<typename T, typename U>
inline Optional<T> operator/(Optional<T> lhs, const U& rhs)
{
    lhs /= rhs;
    return lhs;
}

template<typename T, typename U>
inline Optional<T> operator/(const U& lhs, const Optional<T>& rhs)
{
    return rhs.hasValue() ? Optional<T>(lhs / rhs.value()) : Optional<T>();
}

template<typename T>
inline Optional<T> operator%(Optional<T> lhs, const Optional<T>& rhs)
{
    lhs %= rhs;
    return lhs;
}

template<typename T, typename U>
inline Optional<T> operator%(Optional<T> lhs, const U& rhs)
{
    lhs %= rhs;
    return lhs;
}

template<typename T, typename U>
inline Optional<T> operator%(const U& lhs, const Optional<T>& rhs)
{
    return rhs.hasValue() ? Optional<T>(lhs % rhs.value()) : Optional<T>();
}

} // namespace dewalls

#endif // DEWALLS_OPTIONAL_H
