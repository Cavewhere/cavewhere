#include "wallsparser.h"

namespace dewalls {

WallsParser::WallsParser()
    : _visitor(NULL),
      _inBlockComment(false),
      _units(QSharedPointer<WallsUnits>()),
      _stack(QStack<QSharedPointer<WallsUnits>>()),
      _macros(QHash<QString, QString>())
{

}

} // namespace dewalls

