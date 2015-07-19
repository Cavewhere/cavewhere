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
#include <QFile>
#include <cmath>
#include "lineparser.h"
#include "varianceoverride.h"
#include "wallsvisitor.h"
#include "cardinaldirection.h"
#include "wallsparser.h"

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

    cout << quadrant(East, North, UnitizedDouble<Angle>(110.0, Angle::deg())) << endl;
}

void parseLine(WallsParser& parser, QString line)
{
    static int lineNumber = 1;

    std::cout << std::endl << std::endl << line.toStdString() << std::endl;

    parser.reset(Segment(line, "fakefile.txt", lineNumber++, 0));

    try
    {
        parser.parseLine();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        cout << ex.message().toStdString() << endl;
    }
    catch (const SegmentParseException& ex)
    {
        cout << ex.message().toStdString() << endl;
    }
}

QString toString(QHash<QString, QString> h)
{
    QString result('[');
    foreach(QString key, h.keys())
    {
        if (result.length() > 1) {
            result += ", ";
        }
        result += QString("%1 => %2").arg(key, h[key]);
    }
    return result + ']';
}

int main(int argc, char *argv[]) {
    Length::init();
    Angle::init();

    if (argc < 2) {
        cout << "Usage: dewalls <.srv file>" << endl;
        return 0;
    }

    QString filename = QString::fromLatin1(argv[1]);

    cout << "Parsing file: " << filename.toStdString() << endl;

    QFile file(filename);
    file.open(QFile::ReadOnly);

    WallsParser parser2;
    PrintingWallsVisitor printingVisitor;
    parser2.setVisitor(&printingVisitor);

    int lineNumber = 0;
    while (!file.atEnd())
    {
        QString line = file.readLine();
        if (file.error() != QFile::NoError)
        {
            cerr << QString("Error reading from file %1 at line %2: %3")
                               .arg(filename)
                               .arg(lineNumber)
                               .arg(file.errorString()).toStdString() << endl;
            return 1;
        }

        parser2.reset(Segment(line, filename, lineNumber, 0));

        try
        {
            parser2.parseLine();
        }
        catch (const SegmentParseExpectedException& ex)
        {
            cerr << ex.message().toStdString() << endl;
            return 2;
        }
        catch (const SegmentParseException& ex)
        {
            cerr << ex.message().toStdString() << endl;
            return 2;
        }

        lineNumber++;
    }

    file.close();

    if (3 > 2) {
        return 0;
    }

    typedef UnitizedDouble<Length> ULength;
    typedef UnitizedDouble<Angle> UAngle;

    auto ft = Length::ft();
    auto m = Length::m();

    auto deg = Angle::deg();
    auto percent = Angle::percent();

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

    WallsParser parser(Segment("hello weird world", "fakefile.txt", 2, 0));

    try {
        parser.expect("hello ");
        parser.oneOf([&]() { parser.expect("test"); },
                     [&]() { parser.expect("blah"); });
    }
    catch (const SegmentParseException& ex)
    {
        cout << ex.message().toStdString() << endl;
    }

    typedef QSharedPointer<VarianceOverride> VarianceOverridePtr;

    VarianceOverridePtr v = VarianceOverridePtr(new RMSError(ULength(4, m)));

    cout << v->toString().toStdString() << endl;

    v.clear();

    v = VarianceOverride::FLOATED_TRAVERSE;

    cout << v->toString().toStdString() << endl;

    quadrantTest();

    parser = WallsParser(Segment("123.456 -123.456m -123i14.5", "fakefile.txt", 2, 0));

    try {
        cout << parser.length(Length::ft()) << endl;
        parser.expect(' ');
        cout << parser.length(Length::ft()) << endl;
        parser.expect(' ');
        cout << parser.length(Length::ft()) << endl;
    }
    catch (const SegmentParseException& ex)
    {
        cout << ex.message().toStdString() << endl;
    }

    PrintingWallsVisitor visitor;
    parser.setVisitor(&visitor);

    parseLine(parser, "#units ct feet save lrud=fb:lurd incab=2.3h");
    parseLine(parser, "A*1 B1 350i4 41 +25/-6 *2, --, 4,5,C*#Seg /some/really/cool segment;4, 5>");
    parseLine(parser, "A*1 B1 350 N200mW +25 (*) *2, 3, 4,5,C*#Seg blah;4, 5>");
    parseLine(parser, "#u tape=it");
    parseLine(parser, "A1 B1 350 41 +25 15 16 (3, 5) <2, 3,4,5 >#Seg blah;4, 5>");

    parseLine( parser , "A*1 B1 350i4 41 +25/-6 *2, --, 4,5,C*#Seg /some/really/cool segment;4, 5>" );
    parseLine( parser , "A*1 B1 350 41 +25 *2, 3, 4,5,C*#Q /some/really/cool segment;4, 5>" );
    parseLine( parser , "A*1 B1 350 41 +25 *2, 3 4,5,C*;4, 5>" );
    parseLine( parser , "A*1 B1 350 41 +25 *2 3 4 5 C*;4, 5>" );
    parseLine( parser , "A*1 B1 350 41 +25 *2, 3, 4,5,C*#Seg blah;4, 5>" );
    parseLine( parser , "A*1 B1 350 41:20 +25 (?,) *2, 3, 4,5,C*#Seg blah;4, 5>" );
    parseLine( parser , "A*1 *2,3,4,5*" );
    parseLine( parser , "A*1 B1 350 N41gW +25 (*) *2, 3, 4,5,C*#Seg blah;4, 5>" );
    parseLine( parser , "A*1 B1 350 N41gW +25 ;(*) *2, 3, 4,5,C*#Seg blah;4, 5>" );
    parseLine( parser , "A*1 B1 350 N41gW +25 (*) *2, 3, 4m,3f,50g,C*#Seg blah;4, 5>" );
    parseLine( parser , "A*1 B1 350 N200gW +25 (*) *2, 3, 4,5,C*#Seg blah;4, 5>" );
    parseLine( parser , "A*1 B1 350 N200mS +25 (*) *2, 3, 4,5,C*#Seg blah;4, 5>" );
    parseLine( parser , "A*1 *2,3,4,*" );
    parseLine( parser , "A1, <bash <2,3,4,5>" );
    parseLine( parser , "A1 <bash <2,3,4,5>" );
    parseLine( parser , "A1 b<ash <2,3,4,5>" );
    parseLine( parser , "A1 B1 350 41 +25 (3, 5) <2, 3, 4,5> okay>< weird #Seg blah;4, 5>" );
    parseLine( parser , "A1 B1 350 41 +25 <2, 3, 4,5>#Seg blah;4, 5>" );
    parseLine( parser , "A1 B1 350 41 +25 (3, 5) <2, 3,4,5 >#Seg blah;4, 5>" );
    parseLine( parser , "A1 B1 350 41 +25 (3;, 5) <2, 3,4,5 >#Seg blah;4, 5>" );
    parseLine( parser , "A1 B1 350 41 +25 (3, 5) <2, 3,4,5 *#Seg blah;4, 5>" );
    parseLine( parser , "A1 B1 350 41 +25 (3, 5) hello <2, 3,4,5 *#Seg blah;4, 5>" );

    parseLine( parser , "#u tape=it" );
    parseLine( parser , "A1 B1 350 41 +25 15 16 (3, 5) <2, 3,4,5 >#Seg blah;4, 5>" );
    parseLine( parser , "A1 B1 350 41 +25 15 16 17 (3, 5) <2, 3,4,5 >#Seg blah;4, 5>" );

    parseLine( parser , "   #flag /This is a test" );
    parseLine( parser , "   #flag AB30 /This is a test" );
    parseLine( parser , "   #f" );
    parseLine( parser , "   #f AB30 CY5 /This is a test" );
    //parseLine( parser , "   #u prefix1=A prefix2=CR case=lower" );
    parseLine( parser , "   #u prefix1=A case=lower" );
    parseLine( parser , "  #prefix2 CR    ;test" );

    parseLine( parser , "   #symbol aslkjb;lkj aslkjasdf a; asdflaksjdf" );
    parseLine( parser , "   #sym ; asdlkfjasldf" );

    parseLine( parser , "    \tA1 B:B1 350 41 +25 15 16 (3, 5) <2, 3,4,5 >#Seg blah;4, 5>" );
    parseLine( parser , " FR::A1 B:B1 350 41 +25 15 16 (3, 5) <2, 3,4,5 >#Seg blah;4, 5>" );
    parseLine( parser , "   #seg /hello/world  blah;comment" );

    parseLine( parser , "   #note FR::A1  blah\\n\" hello\\n hello;comment" );

    parseLine( parser , "#date 2015-01-16" );
    parseLine( parser , "#date 01-16-2005" );
    parseLine( parser , "#date 01-16-05" );
    parseLine( parser , "#date 01/16/05" );
    parseLine( parser , "#date 01/16/1905" );

    parseLine( parser , "  #[ this is a test" );
    parseLine( parser , "  blah" );
    parseLine( parser , "  blah" );
    parseLine( parser , "  #segment /hello/world" );
    parseLine( parser , "  ;#]" );
    parseLine( parser , "  #segment /hello/world" );
    parseLine( parser , "  #]" );

    parseLine( parser , " FR::A1 B:B1 350 41 +25 15 16 (3, 5) <2, 3,4,5 >#Seg blah;4, 5>" );

    parseLine( parser , " #fox" );
    parseLine( parser , " #fix GPS9 620765 3461243 123 (R5,?) /Bat Cave Entrance" );
    parseLine( parser , " #FIX A1 W97:43:52.5 N31:16:45 323f /Entrance #s /hello/world;dms with ft elevations" );

    parseLine( parser , "#uNits $hello=\"er=vad order=NUE\"" );

    parseLine( parser , "#u ord$(hello)" );

//    System.out.println( parser.units.ctOrder );
//    System.out.println( parser.units.rectOrder );

    parseLine( parser , "#u ord$(hel lo)" );
    parseLine( parser , "#u ord$(hello" );

    parseLine( parser , "#u o=dav" );

    parseLine( parser , "a - 350 20 +5 *1,2,3,4*" );
    parseLine( parser , "- a 350 20 +5 *1,2,3,4*" );

    ULength la(2, Length::m());
    ULength lb;

    cout << (la + lb) << endl;

    WallsUnits units;
    units.typeab_corrected = true;
    units.typevb_corrected = true;
    units.inch = ULength(1, Length::in());
    units.incv = UAngle(30, Angle::deg());
    units.incd = ULength(1, Length::ft());

    ULength dist(20, Length::ft());
    UAngle fsInc(35, Angle::deg());
    UAngle bsInc(33, Angle::deg());

    units.applyHeightCorrections(dist, fsInc, bsInc, ULength(-5, Length::in()), ULength());

    cout << "dist:  " << dist << endl;
    cout << "fsInc: " << fsInc << endl;
    cout << "bsInc: " << bsInc << endl;
}
