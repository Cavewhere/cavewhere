#ifndef DEWALLS_WALLSLINEPARSER_H
#define DEWALLS_WALLSLINEPARSER_H

#include <QString>
#include <QHash>
#include <QSharedPointer>

#include "lineparser.h"
#include "unitizeddouble.h"
#include "length.h"
#include "angle.h"
#include "cardinaldirection.h"
#include "varianceoverride.h"
#include "wallstypes.h"
#include "wallsparser.h"

namespace dewalls {

class WallsLineParser : public LineParser
{
public:
    typedef UnitizedDouble<Length> ULength;
    typedef UnitizedDouble<Angle>  UAngle;
    typedef const Unit<Length> * LengthUnit;
    typedef const Unit<Angle>  * AngleUnit;
    typedef QHash<QChar, LengthUnit> LengthUnitSuffixMap;
    typedef QHash<QChar, AngleUnit>  AngleUnitSuffixMap;
    typedef QHash<QChar, CardinalDirection::CardinalDirection> CardinalDirectionCharMap;
    typedef QSharedPointer<VarianceOverride> VarianceOverridePtr;

    WallsLineParser(Segment segment);

    ULength unsignedLengthInches();
    ULength unsignedLengthNonInches(LengthUnit defaultUnit);
    ULength unsignedLength(LengthUnit defaultUnit);
    ULength length(LengthUnit defaultUnit);

    UAngle unsignedAngle(AngleUnitSuffixMap unitSuffixes, AngleUnit defaultUnit);
    UAngle unsignedDmsAngle();

    UAngle latitude();
    UAngle longitude();

    UAngle nonQuadrantAzimuth(AngleUnit defaultUnit);
    UAngle quadrantAzimuth(AngleUnit defaultUnit);
    UAngle azimuth(AngleUnit defaultUnit);
    UAngle azimuthOffset(AngleUnit defaultUnit);

    UAngle unsignedInclination(AngleUnit defaultUnit);
    UAngle inclination(AngleUnit defaultUnit);

    VarianceOverridePtr varianceOverride(LengthUnit defaultUnit);

    template<typename F>
    QChar escapedChar(F charPredicate, std::initializer_list<QString> expectedItems);
    template<typename F>
    QString escapedText(F charPredicate, std::initializer_list<QString> expectedItems);
    QString quotedText();

    template<typename R, typename F>
    bool optional(R& result, F production);

    void replaceMacros();
    QString movePastEndQuote();
    QString replaceMacro();

    void beginBlockCommentLine();
    void endBlockCommentLine();
    void insideBlockCommentLine();
    QString untilComment(std::initializer_list<QString> expectedItems);

    void segmentLine();
    QString segmentDirective();

    void prefixLine();
    void prefixDirective();

    void noteLine();
    void noteDirective();

    void flagLine();
    void flagDirective();
    QString slashPrefixedFlag();

    void symbolLine();

    void dateLine();
    void dateDirective();
    QDate isoDate();
    QDate usDate1();
    QDate usDate2();

    void inlineCommentOrEndOfLine();
    void inlineComment();

private:
    VarianceOverridePtr floatedVectorVarianceOverride();
    VarianceOverridePtr floatedTraverseVarianceOverride();
    VarianceOverridePtr lengthVarianceOverride(LengthUnit defaultUnit);
    VarianceOverridePtr rmsErrorVarianceOverride(LengthUnit defaultUnit);

    WallsLineParser() = delete;

    WallsParser* _parser;
};

inline WallsLineParser::WallsLineParser(Segment segment)
    : LineParser(segment)
{
}

template<typename F>
QChar WallsLineParser::escapedChar(F charPredicate, std::initializer_list<QString> expectedItems)
{
    QChar c = expectChar(charPredicate, expectedItems);
    return c == '\\' ? oneOfMap(escapedChars) : c;
}

template<typename F>
QString WallsLineParser::escapedText(F charPredicate, std::initializer_list<QString> expectedItems)
{
    QString result;
    while (maybe([&]() { result.append(this->escapedChar(charPredicate, expectedItems)); } ));
    return result;
}


template<typename R, typename F>
bool WallsLineParser::optional(R& result, F production)
{
    if (maybe([&]() { return this->expect(optionalPattern); }))
    {
        return false;
    }
    result = production();
    return true;
}

} // namespace dewalls

#endif // DEWALLS_WALLSLINEPARSER_H
