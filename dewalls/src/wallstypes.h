#ifndef WALLSTYPES_H
#define WALLSTYPES_H

#include <QString>

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
    D = 0,
    A = 1,
    V = 2
};

enum class LrudElement
{
    L = 0,
    R = 1,
    U = 2,
    D = 3
};

enum class LrudType
{
    From = 0,
    To = 1,
    FB = 2,
    TB = 3
};

enum class RectElement
{
    E = 0,
    N = 1,
    U = 2
};

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

