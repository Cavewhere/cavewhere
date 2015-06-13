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

using namespace std;
using namespace dewalls;

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

    Segment segment("test\r\none", QObjectPtr(), 5, 3);
    cout << segment.endLine() << endl;
    cout << segment.endCol() << endl;

    Segment segment2("test\r\n\none\r\n\n\r", QObjectPtr(), 5, 3);
    cout << segment2.endLine() << endl;
    cout << segment2.endCol() << endl;

    Segment segment3 = Segment("hello\r\nworld", QObjectPtr(), 5, 3).mid(2, 4);
    cout << segment3.startLine() << endl;
    cout << segment3.startCol() << endl;
    cout << segment3.endLine() << endl;
    cout << segment3.endCol() << endl;

    QList<Segment> segs = Segment("this is a   test", QObjectPtr(), 5, 3).split("\\s+");
    for (auto iter = segs.begin(); iter < segs.end(); iter++)
    {
        cout << *iter << endl;
    }

    cout << Segment("  \r this should be trimmed    \t\n  ", QObjectPtr(), 5, 3).trimmed() << endl;

    cout << Segment("hello\nweird\nworld", QObjectPtr(), 2, 0).mid(3, 10).underlineInContext().toStdString() << endl;

    QRegExp rx("he(llo\nwei)rd");

    Segment segment4("hello\nweird\nworld", QObjectPtr(), 2, 0);

    indexIn(rx, segment4);

    cout << cap(rx, segment4, 1).underlineInContext().toStdString() << endl;

    cout << usin(UAngle(90, deg)) << endl;
    cout << uatan2(1, 1).in(deg) << endl;
}
