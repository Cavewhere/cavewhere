#include "cwKeyword.h"


cwKeyword::cwKeyword(const QString& key, const QString& value) :
    Key(key),
    Value(value)
{

}

bool cwKeyword::operator==(const cwKeyword &other) const
{
    return Key == other.Key && Value == other.Value;
}
