#include "wallstypes.h"

namespace dewalls
{

QString applyCaseType(CaseType type, const QString& s)
{
    switch (type)
    {
    case CaseType::Lower:
        return s.toLower();
    case CaseType::Upper:
        return s.toUpper();
    }
    return s;
}

}
