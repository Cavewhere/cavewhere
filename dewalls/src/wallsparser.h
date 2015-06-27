#ifndef DEWALLS_WALLSPARSER_H
#define DEWALLS_WALLSPARSER_H

#include <QHash>
#include <QString>
#include <QStack>
#include <QSharedPointer>
#include "wallsvisitor.h"
#include "wallsunits.h"

namespace dewalls {

class WallsParser
{
public:
    WallsParser();

    WallsVisitor* visitor() const;
    void setVisitor(WallsVisitor* visitor);

private:
    WallsVisitor* _visitor;
    bool _inBlockComment;
    QSharedPointer<WallsUnits> _units;
    QStack<QSharedPointer<WallsUnits>> _stack;
    QHash<QString, QString> _macros;
    friend class WallsLineParser;
};

inline WallsVisitor* WallsParser::visitor() const
{
    return _visitor;
}

inline void WallsParser::setVisitor(WallsVisitor* visitor)
{
    _visitor = visitor;
}

} // namespace dewalls

#endif // DEWALLS_WALLSPARSER_H
