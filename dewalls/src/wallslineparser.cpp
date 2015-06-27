#include "wallslineparser.h"

namespace dewalls {

typedef UnitizedDouble<Length> ULength;
typedef UnitizedDouble<Angle>  UAngle;
typedef const Unit<Length> * LengthUnit;
typedef const Unit<Angle>  * AngleUnit;
typedef QHash<QChar, LengthUnit> LengthUnitSuffixMap;
typedef QHash<QChar, AngleUnit>  AngleUnitSuffixMap;
typedef QHash<QChar, CardinalDirection::CardinalDirection> CardinalDirectionCharMap;
typedef QSharedPointer<VarianceOverride> VarianceOverridePtr;

double approx(double val) {
    return floor(val * 1e6) * 1e-6;
}

ULength WallsLineParser::unsignedLengthInches()
{
    expect('i', Qt::CaseInsensitive);
    double inches = unsignedDoubleLiteral();
    return ULength(inches, Length::in);
}

QList<QPair<QString, LengthUnit>> createLengthUnits()
{
    typedef QPair<QString, LengthUnit> Pair;
    QList<Pair> result;
    result << Pair("meters", Length::meters);
    result << Pair("meter", Length::meters);
    result << Pair("m", Length::meters);
    result << Pair("feet", Length::feet);
    result << Pair("foot", Length::feet);
    result << Pair("ft", Length::feet);
    result << Pair("f", Length::feet);
    return result;
}

QList<QPair<QString, AngleUnit>> createAzmUnits()
{
    typedef QPair<QString, AngleUnit> Pair;
    QList<Pair> result;
    result << Pair("degrees", Angle::degrees);
    result << Pair("degree", Angle::degrees);
    result << Pair("deg", Angle::degrees);
    result << Pair("d", Angle::degrees);
    result << Pair("mills", Angle::milsNATO);
    result << Pair("mils", Angle::milsNATO);
    result << Pair("mil", Angle::milsNATO);
    result << Pair("m", Angle::milsNATO);
    result << Pair("grads", Angle::gradians);
    result << Pair("grad", Angle::gradians);
    result << Pair("g", Angle::gradians);
    return result;
}

QList<QPair<QString, AngleUnit>> createIncUnits()
{
    typedef QPair<QString, AngleUnit> Pair;
    QList<Pair> result;
    result << Pair("degrees", Angle::degrees);
    result << Pair("degree", Angle::degrees);
    result << Pair("deg", Angle::degrees);
    result << Pair("d", Angle::degrees);
    result << Pair("mills", Angle::milsNATO);
    result << Pair("mils", Angle::milsNATO);
    result << Pair("mil", Angle::milsNATO);
    result << Pair("m", Angle::milsNATO);
    result << Pair("grads", Angle::gradians);
    result << Pair("grad", Angle::gradians);
    result << Pair("g", Angle::gradians);
    result << Pair("percent", Angle::percentGrade);
    result << Pair("p", Angle::percentGrade);
    return result;
}

LengthUnitSuffixMap createLengthUnitSuffixes()
{
    LengthUnitSuffixMap result;
    result['m'] = result['M'] = Length::meters;
    result['f'] = result['F'] = Length::feet;
    result['i'] = result['I'] = Length::inches;
    return result;
}

AngleUnitSuffixMap createAzmUnitSuffixes()
{
    AngleUnitSuffixMap result;
    result['d'] = result['D'] = Angle::degrees;
    result['g'] = result['G'] = Angle::gradians;
    result['m'] = result['M'] = Angle::milsNATO;
    return result;
}

AngleUnitSuffixMap createIncUnitSuffixes()
{
    AngleUnitSuffixMap result;
    result['d'] = result['D'] = Angle::degrees;
    result['g'] = result['G'] = Angle::gradians;
    result['m'] = result['M'] = Angle::milsNATO;
    result['p'] = result['P'] = Angle::percentGrade;
    return result;
}

CardinalDirectionCharMap createCardinalDirections()
{
    CardinalDirectionCharMap result;
    result['n'] = result['N'] = CardinalDirection::North;
    result['s'] = result['S'] = CardinalDirection::South;
    result['e'] = result['E'] = CardinalDirection::East;
    result['w'] = result['W'] = CardinalDirection::West;
    return result;
}


CardinalDirectionCharMap createNorthSouth()
{
    CardinalDirectionCharMap result;
    result['n'] = result['N'] = CardinalDirection::North;
    result['s'] = result['S'] = CardinalDirection::South;
    return result;
}

CardinalDirectionCharMap createEastWest()
{
    CardinalDirectionCharMap result;
    result['e'] = result['E'] = CardinalDirection::East;
    result['w'] = result['W'] = CardinalDirection::West;
    return result;
}

QHash<QChar, QChar> createEscapedChars()
{
    QHash<QChar, QChar> result;
    result['r'] = 'r';
    result['n'] = 'n';
    result['f'] = 'f';
    result['t'] = 't';
    result['"'] = '"';
    result['\\'] = '\\';
    return result;
}

QHash<QChar, CtElement> createCtElements()
{
    QHash<QChar, CtElement> result;
    result['d'] = result['D'] = CtElement::D;
    result['a'] = result['A'] = CtElement::A;
    result['v'] = result['V'] = CtElement::V;
    return result;
}

QHash<QChar, RectElement> createRectElements()
{
    QHash<QChar, RectElement> result;
    result['n'] = result['N'] = RectElement::N;
    result['e'] = result['E'] = RectElement::E;
    result['u'] = result['U'] = RectElement::U;
    return result;
}

QHash<QChar, LrudElement> createLrudElements()
{
    QHash<QChar, LrudElement> result;
    result['l'] = result['L'] = LrudElement::L;
    result['r'] = result['R'] = LrudElement::R;
    result['u'] = result['U'] = LrudElement::U;
    result['d'] = result['D'] = LrudElement::D;
    return result;
}

QList<QPair<QString, bool>> createCorrectedValues()
{
    typedef QPair<QString, bool> Pair;
    QList<Pair> result;
    result << Pair("corrected", true);
    result << Pair("c", true);
    result << Pair("normal", false);
    result << Pair("n", false);
    return result;
}

QList<QPair<QString, CaseType>> createCaseTypes()
{
    typedef QPair<QString, CaseType> Pair;
    QList<Pair> result;
    result << Pair("upper", CaseType::Upper);
    result << Pair("u", CaseType::Upper);
    result << Pair("lower", CaseType::Lower);
    result << Pair("l", CaseType::Lower);
    result << Pair("mixed", CaseType::Mixed);
    result << Pair("m", CaseType::Mixed);
    return result;
}

QList<QPair<QString, LrudType>> createLrudTypes()
{
    typedef QPair<QString, LrudType> Pair;
    QList<Pair> result;
    result << Pair("from", LrudType::From);
    result << Pair("fb", LrudType::FB);
    result << Pair("f", LrudType::From);
    result << Pair("to", LrudType::To);
    result << Pair("tb", LrudType::TB);
    result << Pair("t", LrudType::To);
    return result;
}

QList<QPair<QString, QList<TapingMethodElement>>> createTapingMethods()
{
    typedef QList<TapingMethodElement> Method;
    typedef QPair<QString, Method> Pair;
    QList<Pair> result;
    result << Pair("IT", Method({TapingMethodElement::InstrumentHeight, TapingMethodElement::TargetHeight}));
    result << Pair("IS", Method({TapingMethodElement::InstrumentHeight}));
    result << Pair("ST", Method({TapingMethodElement::TargetHeight}));
    result << Pair("SS", Method());
    return result;
}

QList<QPair<QString, int>> createPrefixDirectives()
{
    typedef QPair<QString, int> Pair;
    QList<Pair> result;
    result << Pair("#prefix1", 0) ;
    result << Pair("#prefix2", 1) ;
    result << Pair("#prefix3", 2) ;
    result << Pair("#prefix", 0) ;
    return result;
}

const QList<QPair<QString, LengthUnit>> lengthUnits = createLengthUnits();
const QList<QPair<QString, AngleUnit>> azmUnits = createAzmUnits();
const QList<QPair<QString, AngleUnit>> incUnits = createIncUnits();
const LengthUnitSuffixMap lengthUnitSuffixes = createLengthUnitSuffixes();
const AngleUnitSuffixMap azmUnitSuffixes = createAzmUnitSuffixes();
const AngleUnitSuffixMap incUnitSuffixes = createIncUnitSuffixes();
const CardinalDirectionCharMap cardinalDirections = createCardinalDirections();
const CardinalDirectionCharMap northSouth = createNorthSouth();
const CardinalDirectionCharMap eastWest = createEastWest();
const QHash<QChar, QChar> escapedChars = createEscapedChars();
const QHash<QChar, CtElement> ctElements = createCtElements();
const QHash<QChar, RectElement> rectElements = createRectElements();
const QList<QPair<QString, bool>> correctedValues = createCorrectedValues();
const QList<QPair<QString, CaseType>> caseTypes = createCaseTypes();
const QList<QPair<QString, LrudType>> lrudTypes = createLrudTypes();
const QList<QPair<QString, QList<TapingMethodElement>>> tapingMethods = createTapingMethods();
const QList<QPair<QString, int>> prefixDirectives = createPrefixDirectives();

const QRegExp notSemicolon("[^;]+");
const QRegExp unitsOptionRx("[a-zA-Z_0-9/]*");
const QRegExp macroNameRx("[^()=;,# \t]*");
const QRegExp stationRx("[^<*;,#/ \t][^;,#/ \t]{0,7}");
const QRegExp prefixRx("[^:;,#/ \t]+");

const QRegExp optionalRx("--+");
const QRegExp optionalStationRx("-+");

const QRegExp isoDateRx("\\d{4}-\\d{2}-\\d{2}");
const QRegExp usDateRx1("\\d{2}-\\d{2}-\\d{2,4}");
const QRegExp usDateRx2("\\d{2}/\\d{2}/\\d{2,4}");

ULength WallsLineParser::unsignedLengthNonInches(LengthUnit defaultUnit)
{
    double value = unsignedDoubleLiteral();
    LengthUnit unit = oneOfMap(lengthUnitSuffixes, defaultUnit);
    if (unit == Length::inches)
    {
        double inches = unsignedDoubleLiteral();
        return ULength(value * 12 + inches, Length::inches);
    }
    return ULength(value, unit);
}

ULength WallsLineParser::unsignedLength(LengthUnit defaultUnit)
{
    ULength result;
    oneOfR(result,
          [&]() { return this->unsignedLengthNonInches(defaultUnit); },
          [&]() { return this->unsignedLengthInches(); });
    return result;
}

ULength WallsLineParser::length(LengthUnit defaultUnit)
{
    bool negate = maybe( [&]() { return this->expect('-'); } );
    ULength length = unsignedLength(defaultUnit);
    return negate ? -length : length;
}

UAngle WallsLineParser::unsignedAngle(AngleUnitSuffixMap unitSuffixes, AngleUnit defaultUnit)
{
    auto this_unsignedDoubleLiteral = [&]() { return this->unsignedDoubleLiteral(); };
    auto this_expectColon = [&]() { this->expect(':'); };

    double value;
    bool hasValue = maybe(value, this_unsignedDoubleLiteral);
    if (maybe(this_expectColon))
    {
        double minutes, seconds;
        bool hasMinutes = maybe(minutes, this_unsignedDoubleLiteral);
        bool hasSeconds = false;
        if (maybe(this_expectColon))
        {
            hasSeconds = maybe(seconds, this_unsignedDoubleLiteral);
        }
        if (!(hasValue || hasMinutes || hasSeconds))
        {
            throwAllExpected();
        }
        return UAngle((hasValue   ? value 		     : 0) +
                      (hasMinutes ? minutes / 60.0   : 0) +
                      (hasSeconds ? seconds / 3600.0 : 0), Angle::degrees);
    }
    else if (!hasValue)
    {
        throwAllExpected();
    }
    return UAngle(value, oneOfMap(unitSuffixes, defaultUnit));
}

UAngle WallsLineParser::unsignedDmsAngle()
{
    auto this_unsignedDoubleLiteral = [&]() { return this->unsignedDoubleLiteral(); };
    auto this_expectColon = [&]() { this->expect(':'); };

    double degrees, minutes, seconds;
    bool hasDegrees = maybe(degrees, this_unsignedDoubleLiteral);
    expect(':');
    bool hasMinutes = maybe(minutes, this_unsignedDoubleLiteral);
    bool hasSeconds = false;
    if (maybe(this_expectColon))
    {
        hasSeconds = maybe(seconds, this_unsignedDoubleLiteral);
    }
    if (!(hasDegrees || hasMinutes || hasSeconds))
    {
        throwAllExpected();
    }
    return UAngle((hasDegrees ? degrees 	     : 0) +
                  (hasMinutes ? minutes / 60.0   : 0) +
                  (hasSeconds ? seconds / 3600.0 : 0), Angle::degrees);
}

UAngle WallsLineParser::latitude()
{
    int start = _i;
    CardinalDirection::CardinalDirection side = oneOfMap(northSouth);
    UAngle latitude = unsignedDmsAngle();

    if (approx(latitude.get(Angle::degrees)) > 90.0)
    {
        throw SegmentParseException(_line.mid(start, _i), "latitude out of range");
    }

    if (side == CardinalDirection::South)
    {
        return -latitude;
    }
    return latitude;
}

UAngle WallsLineParser::longitude()
{
    int start = _i;
    CardinalDirection::CardinalDirection side = oneOfMap(eastWest);
    UAngle longitude = unsignedDmsAngle();

    if (approx(longitude.get(Angle::degrees)) > 180.0)
    {
        throw SegmentParseException(_line.mid(start, _i), "longitude out of range");
    }

    if (side == CardinalDirection::West)
    {
        return -longitude;
    }
    return longitude;
}

UAngle WallsLineParser::nonQuadrantAzimuth(AngleUnit defaultUnit)
{
    int start = _i;

    UAngle result = unsignedAngle(azmUnitSuffixes, defaultUnit);

    if (approx(result.get(Angle::degrees)) > 360.0)
    {
        throw SegmentParseException(_line.mid(start, _i), "azimuth out of range");
    }

    return result;
}

UAngle WallsLineParser::quadrantAzimuth(AngleUnit defaultUnit)
{
    CardinalDirection::CardinalDirection from = oneOfMap(cardinalDirections);

    int start = _i;
    UAngle angle;
    if (maybe(angle, [&]() { return this->nonQuadrantAzimuth(defaultUnit); }))
    {
        if (approx(angle.get(Angle::degrees)) > 90.0)
        {
            throw SegmentParseException(_line.mid(start, _i), "azimuth out of range");
        }

        CardinalDirection::CardinalDirection to = oneOfMap(
                    from == CardinalDirection::North ||
                    from == CardinalDirection::South ?
                        eastWest : northSouth);

        return CardinalDirection::quadrant(from, to, angle);
    }
    return CardinalDirection::angle(from);
}

UAngle WallsLineParser::azimuth(AngleUnit defaultUnit)
{
    UAngle result;
    oneOfR(result, [&]() { return this->quadrantAzimuth(defaultUnit); },
                   [&]() { return this->nonQuadrantAzimuth(defaultUnit); });
    return result;
}

UAngle WallsLineParser::azimuthOffset(AngleUnit defaultUnit)
{
    double signum;
    if (!maybe(signum, [this]() { return this->oneOfMap(signSignums); } ))
    {
        signum = 1.0;
    }
    return nonQuadrantAzimuth(defaultUnit) * signum;
}

UAngle WallsLineParser::unsignedInclination(AngleUnit defaultUnit)
{
    int start = _i;
    UAngle result = unsignedAngle(incUnitSuffixes, defaultUnit);

    if (approx(result.get(Angle::degrees)) > 90.0)
    {
        throw SegmentParseException(_line.mid(start, _i), "inclination out of range");
    }

    return result;
}

UAngle WallsLineParser::inclination(AngleUnit defaultUnit)
{
    int start = _i;
    double signum;
    bool hasSignum = maybe(signum, [this]() { return this->oneOfMap(signSignums); } );
    UAngle angle = unsignedInclination(defaultUnit);

    if (!hasSignum)
    {
        if (angle.get(angle.unit()) == 0.0)
        {
            throw SegmentParseException(_line.mid(start, _i), "zero inclinations must not be preceded by a sign");
        }
        return angle * signum;
    }
    return angle;
}

VarianceOverridePtr WallsLineParser::varianceOverride(LengthUnit defaultUnit)
{
    VarianceOverridePtr result;
    oneOfR(result,
           [&]() { return this->floatedVectorVarianceOverride(); },
           [&]() { return this->floatedTraverseVarianceOverride(); },
           [&]() { return this->lengthVarianceOverride(defaultUnit); },
           [&]() { return this->rmsErrorVarianceOverride(defaultUnit); });
    return result;
}

VarianceOverridePtr WallsLineParser::floatedVectorVarianceOverride()
{
    expect('?');
    return VarianceOverride::FLOATED;
}

VarianceOverridePtr WallsLineParser::floatedTraverseVarianceOverride()
{
    expect('*');
    return VarianceOverride::FLOATED_TRAVERSE;
}

VarianceOverridePtr WallsLineParser::lengthVarianceOverride(LengthUnit defaultUnit)
{
    return VarianceOverridePtr(new LengthOverride(unsignedLength(defaultUnit)));
}

VarianceOverridePtr WallsLineParser::rmsErrorVarianceOverride(LengthUnit defaultUnit)
{
    expect('r', Qt::CaseInsensitive);
    return VarianceOverridePtr(new RMSError(unsignedLength(defaultUnit)));
}

QString WallsLineParser::quotedText()
{
    expect('"');
    QString result = escapedText([](QChar c) { return c != '"'; }, {"<QUOTED TEXT>"});
    expect('"');
    return result;
}

QString WallsLineParser::movePastEndQuote()
{
    int start = _i;
    while (_i < _line.length())
    {
        QChar c = _line.at(_i++);
        if (c == '\\')
        {
            _i++;
        }
        else if (c == '"')
        {
            break;
        }
    }

    return _line.value().mid(start, _i);
}

QString WallsLineParser::replaceMacro()
{
    _i += 2;
    int start = _i;
    while (_i < _line.length())
    {
        QChar c = _line.at(_i++);
        if (c == ')')
        {
            Segment macroName = _line.mid(start, _i - 1);
            if (!_parser->_macros.contains(macroName.value()))
            {
                throw SegmentParseException(macroName, "macro not defined");
            }
            return _parser->_macros[macroName.value()];
        }
        else if (c.isSpace())
        {
            throw SegmentParseExpectedException(_line.atAsSegment(_i), "<NONWHITESPACE>");
        }
    }
    throw SegmentParseExpectedException(_line.atAsSegment(_i), std::initializer_list<QString>{"<NON_WHITESPACE>", ")"});
}

void WallsLineParser::replaceMacros()
{
    QString newLine;

    bool replaced = false;

    while (_i < _line.length())
    {
        QChar c = _line.at(_i);
        switch(c.toLatin1())
        {
        case '"':
            newLine.append(movePastEndQuote());
            break;
        case '$':
            if (_i + 1 < _line.length() && _line.at(_i + 1) == '(')
            {
                replaced = true;
                newLine.append(replaceMacro());
                break;
            }
        default:
            newLine.append(c);
            _i++;
            break;
        }
    }

    _i = 0;

    if (replaced)
    {
        _line = Segment(newLine, _line.source(), _line.startLine(), _line.startCol());
    }
}

void WallsLineParser::beginBlockCommentLine()
{
    maybeWhitespace();
    expect("#[");
    _parser->_inBlockComment = true;
    _parser->visitor()->visitBlockCommentLine(remaining().value());
}

void WallsLineParser::endBlockCommentLine()
{
    maybeWhitespace();
    expect("#]");
    remaining();
    _parser->_inBlockComment = false;
}

void WallsLineParser::insideBlockCommentLine()
{
    _parser->_visitor->visitBlockCommentLine(remaining().value());
}

QString WallsLineParser::untilComment(std::initializer_list<QString> expectedItems)
{
    return expect(notSemicolon, expectedItems).value();
}

void WallsLineParser::segmentLine()
{
    maybeWhitespace();
    _parser->_units->segment = segmentDirective();
    maybeWhitespace();
    inlineCommentOrEndOfLine();
}

QString WallsLineParser::segmentDirective()
{
    oneOf([&]() { this->expect("#segment", Qt::CaseInsensitive); },
    [&]() { this->expect("#seg", Qt::CaseInsensitive); },
    [&]() { this->expect("#s", Qt::CaseInsensitive); } );

    if (maybeWhitespace())
    {
        QString result;
        maybe(result, [&]() { return this->untilComment({"<SEGMENT>"}); });
        return result;
    }
    return QString();
}

void WallsLineParser::prefixLine()
{
    maybeWhitespace();
    prefixDirective();
    maybeWhitespace();
    inlineCommentOrEndOfLine();
}

void WallsLineParser::prefixDirective()
{
    int prefixIndex = oneOfList(prefixDirectives, Qt::CaseInsensitive);

    QString prefix;

    if (maybeWhitespace())
    {
        Segment segment;
        if (maybe(segment, [&]() { return this->expect(prefixRx, {"<PREFIX>"}); }))
        {
            prefix = segment.value();
        }
    }

    _parser->_units->setPrefix(prefixIndex, prefix);
}

void WallsLineParser::noteLine()
{
    maybeWhitespace();
    noteDirective();
    maybeWhitespace();
    inlineCommentOrEndOfLine();
}

void WallsLineParser::noteDirective()
{
    oneOf([&]() { this->expect("#note", Qt::CaseInsensitive); },
    [&]() { this->expect("#n", Qt::CaseInsensitive); });

    whitespace();
    QString station = expect(stationRx, {"<STATION NAME>"}).value();
    whitespace();
    QString note = escapedText([](QChar c) { return c != ';'; }, {"<NOTE>"});

    _parser->_visitor->visitNoteLine(station, note);
}

void WallsLineParser::flagLine()
{
    maybeWhitespace();
    flagDirective();
    maybeWhitespace();
    inlineCommentOrEndOfLine();
}

void WallsLineParser::flagDirective()
{
    oneOf([&]() { this->expect("#flag", Qt::CaseInsensitive); },
    [&]() { this->expect("#f", Qt::CaseInsensitive); });

    QStringList stations;

    maybeWhitespace();

    while(true)
    {
        Segment station;
        if (!maybe([&]() { return this->expect(stationRx, {"<STATION NAME>"}); }))
        {
            break;
        }
        stations << station.value();

        maybeWhitespace();
        maybe([&]() { this->expect(','); });
        maybeWhitespace();
    }

    QString flag;
    bool hasFlag = maybe(flag, [&]() { return this->slashPrefixedFlag(); });
    maybeWhitespace();

    if (stations.isEmpty())
    {
        _parser->_units->flag = flag;
    }
    else
    {
        if (!hasFlag)
        {
            throwAllExpected();
        }
        _parser->_visitor->visitFlaggedStations(flag, stations);
    }

    inlineCommentOrEndOfLine();
}

QString WallsLineParser::slashPrefixedFlag()
{
    expect('/');
    return expect(notSemicolon, {"<FLAG>"}).value();
}

void WallsLineParser::symbolLine()
{
    maybeWhitespace();

    oneOf([&]() { this->expect("#symbol", Qt::CaseInsensitive); },
    [&]() { this->expect("#sym", Qt::CaseInsensitive); });

    // ignore the rest for now
    remaining();
}

void WallsLineParser::dateLine()
{
    maybeWhitespace();
    dateDirective();
    maybeWhitespace();
    inlineCommentOrEndOfLine();
}

void WallsLineParser::dateDirective()
{
    expect("#date", Qt::CaseInsensitive);
    whitespace();
    oneOfR(_parser->_units->date,
    [&]() { return this->isoDate(); },
    [&]() { return this->usDate1(); },
    [&]() { return this->usDate2(); });
}

QDate WallsLineParser::isoDate()
{
    Segment dateSegment = expect(isoDateRx, {"<DATE>"});
    return QDate::fromString(dateSegment.value(), Qt::ISODate);
}

QDate WallsLineParser::usDate1()
{
    QString str = expect(usDateRx1, {"<DATE>"}).value();
    return QDate::fromString(str, str.length() > 8 ? "MM-dd-yyyy" : "MM-dd-yy");
}

QDate WallsLineParser::usDate2()
{
    QString str = expect(usDateRx2, {"<DATE>"}).value();
    return QDate::fromString(str, str.length() > 8 ? "MM/dd/yyyy" : "MM/dd/yy");
}

void WallsLineParser::inlineCommentOrEndOfLine()
{
    oneOf([&]() { this->inlineComment(); },
    [&]() { this->endOfLine(); });
}

void WallsLineParser::inlineComment()
{
    expect(';');
    _parser->_visitor->visitInlineComment(remaining().value());
}

} // namespace dewalls
