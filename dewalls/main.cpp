#include <iostream>
#include "unit.h"
#include "unittype.h"
#include "length.h"
#include "angle.h"
#include "defaultunit.h"
#include "unitizeddouble.h"
#include "unitizedmath.h"
#include <string>
#include "segment.h"
#include <QRegExp>
#include <QHash>
#include <QChar>
#include <cmath>
#include "lineparser.h"
#include "varianceoverride.h"
#include "wallsvisitor.h"
#include "cardinaldirection.h"

using namespace std;
using namespace dewalls;
template <typename T>

void func(T t)
{
    std::cout << t << std::endl ;
}

template<typename T, typename... Args>
void func(T t, Args... args) // recursive variadic function
{
    std::cout << t <<std::endl ;

    func(args...) ;
}

void quadrantTest()
{
    using namespace CardinalDirection;

    cout << quadrant(East, North, UnitizedDouble<Angle>(110.0, Angle::deg)) << endl;
}

int main(int argc, char *argv[]) {
    typedef UnitizedDouble<Length> ULength;
    typedef UnitizedDouble<Angle> UAngle;

    auto ft = Length::ft;
    auto m = Length::m;

    auto deg = Angle::deg;
    auto percent = Angle::percent;

    cout << UAngle(45, deg).get(percent) << endl;

    auto a = ULength(3, ft);
    auto b = ULength(5, m);
    auto c = a - b;
    auto d = new ULength(4, m);
    cout << -c << endl;
    cout << a + b + b << endl;
    cout << a * 3 << endl;
    cout << 3 * a << endl;
    cout << a / 3 << endl;
    cout << 3 / a << endl;
    cout << a.in(m) * 3 + b / 2 << endl;
    cout << 3 / *d << endl;

    Segment segment("test\r\none", "", 5, 3);
    cout << segment.endLine() << endl;
    cout << segment.endCol() << endl;

    Segment segment2("test\r\n\none\r\n\n\r", "", 5, 3);
    cout << segment2.endLine() << endl;
    cout << segment2.endCol() << endl;

    Segment segment3 = Segment("hello\r\nworld", "", 5, 3).mid(2, 4);
    cout << segment3.startLine() << endl;
    cout << segment3.startCol() << endl;
    cout << segment3.endLine() << endl;
    cout << segment3.endCol() << endl;

    QList<Segment> segs = Segment("this is a   test", "", 5, 3).split("\\s+");
    for (auto iter = segs.begin(); iter < segs.end(); iter++)
    {
        cout << *iter << endl;
    }

    cout << Segment("  \r this should be trimmed    \t\n  ", "", 5, 3).trimmed() << endl;

    cout << Segment("hello\nweird\nworld", "", 2, 0).mid(3, 10).underlineInContext().toStdString() << endl;

    QRegExp rx("he(llo\nwei)rd");

    Segment segment4("hello\nweird\nworld", "", 2, 0);

    indexIn(rx, segment4);

    cout << cap(rx, segment4, 1).underlineInContext().toStdString() << endl;

    LineParser parser(Segment("hello weird world", "fakefile.txt", 2, 0));

    try {
        parser.expect("hello ");
        parser.oneOf([&]() { parser.expect("test"); },
                     [&]() { parser.expect("blah"); });
    }
    catch (const SegmentParseException& ex)
    {
        cout << ex.message().toStdString() << endl;
    }

    VarianceOverride *v = new RMSError(ULength(4, m));

    cout << v->toString().toStdString() << endl;

    delete v;

    v = VarianceOverride::FLOATED_TRAVERSE;

    cout << v->toString().toStdString() << endl;

    quadrantTest();
}
