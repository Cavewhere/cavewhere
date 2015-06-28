#ifndef WALLSTYPES_H
#define WALLSTYPES_H

#include <QString>
#include <QHash>

namespace dewalls
{

enum CaseType
{
    Upper = 0,
    Lower = 1,
    Mixed = 2
};

QString applyCaseType(CaseType, const QString& s);

enum class CtElement
{
    D = 'D',
    A = 'A',
    V = 'V'
};

inline uint qHash(CtElement key, uint seed = 0)
{
    return uint(static_cast<char>(key)) ^ seed;
}

enum class LrudElement
{
    L = 'L',
    R = 'R',
    U = 'U',
    D = 'D'
};

inline uint qHash(const LrudElement& key, uint seed = 0)
{
    return uint(static_cast<char>(key)) ^ seed;
}

enum class LrudType
{
    From = 0,
    To = 1,
    FB = 2,
    TB = 3
};

enum class RectElement
{
    E = 'E',
    N = 'N',
    U = 'U'
};

inline uint qHash(const RectElement& key, uint seed = 0)
{
    return uint(static_cast<char>(key)) ^ seed;
}

enum class TapingMethodElement
{
    InstrumentHeight = 0,
    TargetHeight = 1
};

enum class VectorType
{
    CT = 0,
    RECT = 1
};

}

#endif // WALLSTYPES_H

