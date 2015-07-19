#include "wallsparser.h"

namespace dewalls {

typedef UnitizedDouble<Length> ULength;
typedef UnitizedDouble<Angle>  UAngle;
typedef const Unit<Length> * LengthUnit;
typedef const Unit<Angle>  * AngleUnit;
typedef QHash<QChar, LengthUnit> LengthUnitSuffixMap;
typedef QHash<QChar, AngleUnit>  AngleUnitSuffixMap;
typedef QHash<QChar, CardinalDirection::CardinalDirection> CardinalDirectionCharMap;
typedef QSharedPointer<VarianceOverride> VarianceOverridePtr;
typedef void (WallsParser::*OwnProduction)();

QList<QPair<QString, LengthUnit>> createLengthUnits()
{
    typedef QPair<QString, LengthUnit> Pair;
    QList<Pair> result;
    result << Pair("meters", Length::meters());
    result << Pair("meter", Length::meters());
    result << Pair("m", Length::meters());
    result << Pair("feet", Length::feet());
    result << Pair("foot", Length::feet());
    result << Pair("ft", Length::feet());
    result << Pair("f", Length::feet());
    return result;
}

QList<QPair<QString, AngleUnit>> createAzmUnits()
{
    typedef QPair<QString, AngleUnit> Pair;
    QList<Pair> result;
    result << Pair("degrees", Angle::degrees());
    result << Pair("degree", Angle::degrees());
    result << Pair("deg", Angle::degrees());
    result << Pair("d", Angle::degrees());
    result << Pair("mills", Angle::milsNATO());
    result << Pair("mils", Angle::milsNATO());
    result << Pair("mil", Angle::milsNATO());
    result << Pair("m", Angle::milsNATO());
    result << Pair("grads", Angle::gradians());
    result << Pair("grad", Angle::gradians());
    result << Pair("g", Angle::gradians());
    return result;
}

QList<QPair<QString, AngleUnit>> createIncUnits()
{
    typedef QPair<QString, AngleUnit> Pair;
    QList<Pair> result;
    result << Pair("degrees", Angle::degrees());
    result << Pair("degree", Angle::degrees());
    result << Pair("deg", Angle::degrees());
    result << Pair("d", Angle::degrees());
    result << Pair("mills", Angle::milsNATO());
    result << Pair("mils", Angle::milsNATO());
    result << Pair("mil", Angle::milsNATO());
    result << Pair("m", Angle::milsNATO());
    result << Pair("grads", Angle::gradians());
    result << Pair("grad", Angle::gradians());
    result << Pair("g", Angle::gradians());
    result << Pair("percent", Angle::percentGrade());
    result << Pair("p", Angle::percentGrade());
    return result;
}

LengthUnitSuffixMap createLengthUnitSuffixes()
{
    LengthUnitSuffixMap result;
    result['m'] = result['M'] = Length::meters();
    result['f'] = result['F'] = Length::feet();
    result['i'] = result['I'] = Length::inches();
    return result;
}

AngleUnitSuffixMap createAzmUnitSuffixes()
{
    AngleUnitSuffixMap result;
    result['d'] = result['D'] = Angle::degrees();
    result['g'] = result['G'] = Angle::gradians();
    result['m'] = result['M'] = Angle::milsNATO();
    return result;
}

AngleUnitSuffixMap createIncUnitSuffixes()
{
    AngleUnitSuffixMap result;
    result['d'] = result['D'] = Angle::degrees();
    result['g'] = result['G'] = Angle::gradians();
    result['m'] = result['M'] = Angle::milsNATO();
    result['p'] = result['P'] = Angle::percentGrade();
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
    result['e'] = result['E'] = RectElement::E;
    result['n'] = result['N'] = RectElement::N;
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

QHash<QString, OwnProduction> WallsParser::createUnitsOptionMap()
{
    QHash<QString, OwnProduction> result;
    result["save"] = &WallsParser::save;
    result["restore"] = &WallsParser::restore;
    result["reset"] = &WallsParser::reset_;
    result["m"] = &WallsParser::meters;
    result["meters"] = &WallsParser::meters;
    result["f"] = &WallsParser::feet;
    result["feet"] = &WallsParser::feet;
    result["ct"] = &WallsParser::ct;
    result["d"] = &WallsParser::d;
    result["s"] = &WallsParser::s;
    result["a"] = &WallsParser::a;
    result["ab"] = &WallsParser::ab;
    result["a/ab"] = &WallsParser::a_ab;
    result["v"] = &WallsParser::v;
    result["vb"] = &WallsParser::vb;
    result["v/vb"] = &WallsParser::v_vb;
    result["o"] = &WallsParser::order;
    result["order"] = &WallsParser::order;
    result["decl"] = &WallsParser::decl;
    result["grid"] = &WallsParser::grid;
    result["rect"] = &WallsParser::rect;
    result["incd"] = &WallsParser::incd;
    result["inch"] = &WallsParser::inch;
    result["incs"] = &WallsParser::incs;
    result["inca"] = &WallsParser::inca;
    result["incab"] = &WallsParser::incab;
    result["incv"] = &WallsParser::incv;
    result["incvb"] = &WallsParser::incvb;
    result["typeab"] = &WallsParser::typeab;
    result["typevb"] = &WallsParser::typevb;
    result["case"] = &WallsParser::case_;
    result["lrud"] = &WallsParser::lrud;
    result["tape"] = &WallsParser::tape;
    result["prefix"] = &WallsParser::prefix1;
    result["prefix1"] = &WallsParser::prefix1;
    result["prefix2"] = &WallsParser::prefix2;
    result["prefix3"] = &WallsParser::prefix3;
    result["uvh"] = &WallsParser::uvh;
    result["uvv"] = &WallsParser::uvv;
    result["uv"] = &WallsParser::uv;
    result["flag"] = &WallsParser::flag;

    return result;
}

QHash<QString, OwnProduction> WallsParser::createDirectivesMap()
{
    QHash<QString, OwnProduction> result;
    result["#units"] = result["#u"] = &WallsParser::unitsLine;
    result["#flag"] = result["#f"] = &WallsParser::flagLine;
    result["#fix"] = &WallsParser::fixLine;
    result["#note"] = &WallsParser::noteLine;
    result["#symbol"] = result["#sym"] = &WallsParser::symbolLine;
    result["#segment"] = result["#seg"] = result["#s"] = &WallsParser::segmentLine;
    result["#date"] = &WallsParser::dateLine;
    result["#["] = &WallsParser::beginBlockCommentLine;
    result["#]"] = &WallsParser::endBlockCommentLine;
    result["#prefix1"] = result["#prefix2"] =
            result["#prefix3"] = result["#prefix"] = &WallsParser::prefixLine;
    return result;
}

double WallsParser::approx(double val)
{
    return floor(val * 1e6) * 1e-6;
}

WallsParser::WallsParser()
    : WallsParser(Segment())
{

}

WallsParser::WallsParser(Segment segment)
    : LineParser(segment),
      _visitor(NULL),
      _inBlockComment(false),
      _units(QSharedPointer<WallsUnits>(new WallsUnits())),
      _stack(QStack<QSharedPointer<WallsUnits>>()),
      _macros(QHash<QString, QString>()),
    lengthUnits(createLengthUnits()),
    azmUnits(createAzmUnits()),
    incUnits(createIncUnits()),
    lengthUnitSuffixes(createLengthUnitSuffixes()),
    azmUnitSuffixes(createAzmUnitSuffixes()),
    incUnitSuffixes(createIncUnitSuffixes()),
    cardinalDirections(createCardinalDirections()),
    northSouth(createNorthSouth()),
    eastWest(createEastWest()),
    escapedChars(createEscapedChars()),
    ctElements(createCtElements()),
    requiredCtElements({CtElement::D, CtElement::A}),
    rectElements(createRectElements()),
    requiredRectElements({RectElement::E, RectElement::N}),
    lrudElements(createLrudElements()),
    requiredLrudElements({LrudElement::L, LrudElement::R, LrudElement::U, LrudElement::D}),
    correctedValues(createCorrectedValues()),
    caseTypes(createCaseTypes()),
    lrudTypes(createLrudTypes()),
    tapingMethods(createTapingMethods()),
    prefixDirectives(createPrefixDirectives()),
    unitsOptionMap(createUnitsOptionMap()),
    directivesMap(createDirectivesMap()),
    notSemicolon(QRegExp("[^;]+")),
    unitsOptionRx(QRegExp("[a-zA-Z_0-9/]*")),
    macroNameRx(QRegExp("[^()=,,# \t]*")),
    stationRx(QRegExp("[^<*,,#/ \t][^,,#/ \t]{0,7}")),
    prefixRx(QRegExp("[^:,,#/ \t]+")),
    optionalRx(QRegExp("--+")),
    optionalStationRx(QRegExp("-+")),
    isoDateRx(QRegExp("\\d{4}-\\d{2}-\\d{2}")),
    usDateRx1(QRegExp("\\d{2}-\\d{2}-\\d{2,4}")),
    usDateRx2(QRegExp("\\d{2}/\\d{2}/\\d{2,4}")),
    directiveRx(QRegExp("#([][]|[a-zA-Z0-9]+)"))
{

}

WallsParser& WallsParser::operator =(const WallsParser& rhs)
{
    _visitor = rhs._visitor;
    _inBlockComment = rhs._inBlockComment;
    _units = QSharedPointer<WallsUnits>(new WallsUnits(*rhs._units));
    _stack.clear();
    foreach (QSharedPointer<WallsUnits> stackItem, rhs._stack)
    {
        _stack.push(QSharedPointer<WallsUnits>(new WallsUnits(*stackItem)));
    }
    _macros = rhs._macros;
    return *this;
}

ULength WallsParser::unsignedLengthInches()
{
    expect('i', Qt::CaseInsensitive);
    double inches = unsignedDoubleLiteral();
    return ULength(inches, Length::in());
}

ULength WallsParser::unsignedLengthNonInches(LengthUnit defaultUnit)
{
    double value = unsignedDoubleLiteral();
    LengthUnit unit = oneOfMap(lengthUnitSuffixes, defaultUnit);
    if (unit == Length::inches())
    {
        double inches = unsignedDoubleLiteral();
        return ULength(value * 12 + inches, Length::inches());
    }
    return ULength(value, unit);
}

ULength WallsParser::unsignedLength(LengthUnit defaultUnit)
{
    ULength result;
    oneOfR(result,
          [&]() { return this->unsignedLengthNonInches(defaultUnit); },
          [&]() { return this->unsignedLengthInches(); });
    return result;
}

ULength WallsParser::length(LengthUnit defaultUnit)
{
    bool negate = maybe( [&]() { return this->expect('-'); } );
    ULength length = unsignedLength(defaultUnit);
    return negate ? -length : length;
}

UAngle WallsParser::unsignedAngle(AngleUnitSuffixMap unitSuffixes, AngleUnit defaultUnit)
{
    auto this_expectColon = [&]() { this->expect(':'); };

    double value;
    bool hasValue = maybeOwn(value, &WallsParser::unsignedDoubleLiteral);
    if (maybe(this_expectColon))
    {
        double minutes, seconds;
        bool hasMinutes = maybeOwn(minutes, &WallsParser::unsignedDoubleLiteral);
        bool hasSeconds = false;
        if (maybe(this_expectColon))
        {
            hasSeconds = maybeOwn(seconds, &WallsParser::unsignedDoubleLiteral);
        }
        if (!(hasValue || hasMinutes || hasSeconds))
        {
            throwAllExpected();
        }
        return UAngle((hasValue   ? value 		     : 0) +
                      (hasMinutes ? minutes / 60.0   : 0) +
                      (hasSeconds ? seconds / 3600.0 : 0), Angle::degrees());
    }
    else if (!hasValue)
    {
        throwAllExpected();
    }
    return UAngle(value, oneOfMap(unitSuffixes, defaultUnit));
}

UAngle WallsParser::unsignedDmsAngle()
{
    auto this_expectColon = [&]() { this->expect(':'); };

    double degrees, minutes, seconds;
    bool hasDegrees = maybeOwn(degrees, &WallsParser::unsignedDoubleLiteral);
    expect(':');
    bool hasMinutes = maybeOwn(minutes, &WallsParser::unsignedDoubleLiteral);
    bool hasSeconds = false;
    if (maybe(this_expectColon))
    {
        hasSeconds = maybeOwn(seconds, &WallsParser::unsignedDoubleLiteral);
    }
    if (!(hasDegrees || hasMinutes || hasSeconds))
    {
        throwAllExpected();
    }
    return UAngle((hasDegrees ? degrees 	     : 0) +
                  (hasMinutes ? minutes / 60.0   : 0) +
                  (hasSeconds ? seconds / 3600.0 : 0), Angle::degrees());
}

UAngle WallsParser::latitude()
{
    int start = _i;
    CardinalDirection::CardinalDirection side = oneOfMap(northSouth);
    UAngle latitude = unsignedDmsAngle();

    if (approx(latitude.get(Angle::degrees())) > 90.0)
    {
        throw SegmentParseException(_line.mid(start, _i), "latitude out of range");
    }

    if (side == CardinalDirection::South)
    {
        return -latitude;
    }
    return latitude;
}

UAngle WallsParser::longitude()
{
    int start = _i;
    CardinalDirection::CardinalDirection side = oneOfMap(eastWest);
    UAngle longitude = unsignedDmsAngle();

    if (approx(longitude.get(Angle::degrees())) > 180.0)
    {
        throw SegmentParseException(_line.mid(start, _i), "longitude out of range");
    }

    if (side == CardinalDirection::West)
    {
        return -longitude;
    }
    return longitude;
}

UAngle WallsParser::nonQuadrantAzimuth(AngleUnit defaultUnit)
{
    int start = _i;

    UAngle result = unsignedAngle(azmUnitSuffixes, defaultUnit);

    if (approx(result.get(Angle::degrees())) > 360.0)
    {
        throw SegmentParseException(_line.mid(start, _i), "azimuth out of range");
    }

    return result;
}

UAngle WallsParser::quadrantAzimuth(AngleUnit defaultUnit)
{
    CardinalDirection::CardinalDirection from = oneOfMap(cardinalDirections);

    int start = _i;
    UAngle angle;
    if (maybe(angle, [&]() { return this->nonQuadrantAzimuth(defaultUnit); }))
    {
        if (approx(angle.get(Angle::degrees())) > 90.0)
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

UAngle WallsParser::azimuth(AngleUnit defaultUnit)
{
    UAngle result;
    oneOfR(result, [&]() { return this->quadrantAzimuth(defaultUnit); },
                   [&]() { return this->nonQuadrantAzimuth(defaultUnit); });
    return result;
}

UAngle WallsParser::azimuthOffset(AngleUnit defaultUnit)
{
    double signum;
    if (!maybe(signum, [this]() { return this->oneOfMap(signSignums); } ))
    {
        signum = 1.0;
    }
    return nonQuadrantAzimuth(defaultUnit) * signum;
}

UAngle WallsParser::unsignedInclination(AngleUnit defaultUnit)
{
    int start = _i;
    UAngle result = unsignedAngle(incUnitSuffixes, defaultUnit);

    if (approx(result.get(Angle::degrees())) > 90.0)
    {
        throw SegmentParseException(_line.mid(start, _i), "inclination out of range");
    }

    return result;
}

UAngle WallsParser::inclination(AngleUnit defaultUnit)
{
    int start = _i;
    double signum;
    bool hasSignum = maybe(signum, [this]() { return this->oneOfMap(signSignums); } );
    UAngle angle = unsignedInclination(defaultUnit);

    if (hasSignum)
    {
        if (angle.get(angle.unit()) == 0.0)
        {
            throw SegmentParseException(_line.mid(start, _i), "zero inclinations must not be preceded by a sign");
        }
        return angle * signum;
    }
    return angle;
}

VarianceOverridePtr WallsParser::varianceOverride(LengthUnit defaultUnit)
{
    VarianceOverridePtr result;
    oneOfR(result,
           [&]() { return this->floatedVectorVarianceOverride(); },
           [&]() { return this->floatedTraverseVarianceOverride(); },
           [&]() { return this->lengthVarianceOverride(defaultUnit); },
           [&]() { return this->rmsErrorVarianceOverride(defaultUnit); },
           [&]() { return VarianceOverridePtr(NULL); });
    return result;
}

VarianceOverridePtr WallsParser::floatedVectorVarianceOverride()
{
    expect('?');
    return VarianceOverride::FLOATED;
}

VarianceOverridePtr WallsParser::floatedTraverseVarianceOverride()
{
    expect('*');
    return VarianceOverride::FLOATED_TRAVERSE;
}

VarianceOverridePtr WallsParser::lengthVarianceOverride(LengthUnit defaultUnit)
{
    return VarianceOverridePtr(new LengthOverride(unsignedLength(defaultUnit)));
}

VarianceOverridePtr WallsParser::rmsErrorVarianceOverride(LengthUnit defaultUnit)
{
    expect('r', Qt::CaseInsensitive);
    return VarianceOverridePtr(new RMSError(unsignedLength(defaultUnit)));
}

QString WallsParser::quotedTextOrNonwhitespace()
{
    QString result;
    oneOfR(result, [&]() { return this->quotedText(); },
    [&]() { return this->nonwhitespace().value(); });
    return result;
}

QString WallsParser::quotedText()
{
    expect('"');
    QString result = escapedText([](QChar c) { return c != '"'; }, {"<QUOTED TEXT>"});
    expect('"');
    return result;
}

QString WallsParser::movePastEndQuote()
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

void WallsParser::parseLine()
{
    maybeWhitespace();

    if (isAtEnd())
    {
        return;
    }

    if (_inBlockComment)
    {
        throwAllExpected([&]() {
            this->oneOfWithLookahead([&]() { this->endBlockCommentLine(); },
            [&]() { this->insideBlockCommentLine(); });
        });
    }
    else
    {
        throwAllExpected([&]() { this->oneOf([&]() { this->comment(); },
            [&]() { this->directiveLine(); },
            [&]() { this->vectorLine(); });
        });
    }
}

void WallsParser::directiveLine()
{
    int start = _i;
    OwnProduction directive = oneOfMapLowercase(directiveRx, directivesMap);
    _i = start;
    replaceMacros();
    (this->*directive)();
}

QString WallsParser::replaceMacro()
{
    _i += 2;
    int start = _i;
    while (_i < _line.length())
    {
        QChar c = _line.at(_i++);
        if (c == ')')
        {
            Segment macroName = _line.mid(start, _i - 1 - start);
            if (!_macros.contains(macroName.value()))
            {
                throw SegmentParseException(macroName, "macro not defined");
            }
            return _macros[macroName.value()];
        }
        else if (c.isSpace())
        {
            throw SegmentParseExpectedException(_line.atAsSegment(_i - 1), "<NONWHITESPACE>");
        }
    }
    throw SegmentParseExpectedException(_line.atAsSegment(_i), std::initializer_list<QString>{"<NON_WHITESPACE>", ")"});
}

void WallsParser::replaceMacros()
{
    QString newLine;

    bool replaced = false;

    while (_i < _line.length())
    {
        QChar c = _line.at(_i);
        switch(c.toLatin1())
        {
        case '"':
            newLine += movePastEndQuote();
            break;
        case '$':
            if (_i + 1 < _line.length() && _line.at(_i + 1) == '(')
            {
                replaced = true;
                newLine += replaceMacro();
                break;
            }
        default:
            newLine += c;
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

void WallsParser::beginBlockCommentLine()
{
    maybeWhitespace();
    expect("#[");
    _inBlockComment = true;
    _visitor->visitBlockCommentLine(remaining().value());
}

void WallsParser::endBlockCommentLine()
{
    maybeWhitespace();
    expect("#]");
    remaining();
    _inBlockComment = false;
}

void WallsParser::insideBlockCommentLine()
{
    _visitor->visitBlockCommentLine(remaining().value());
}

Segment WallsParser::untilComment(std::initializer_list<QString> expectedItems)
{
    return expect(notSemicolon, expectedItems);
}

void WallsParser::segmentLine()
{
    maybeWhitespace();
    _units->segment = combineSegments(_units->segment, segmentDirective());
    maybeWhitespace();
    inlineCommentOrEndOfLine();
}

Segment WallsParser::segmentDirective()
{
    oneOf([&]() { this->expect("#segment", Qt::CaseInsensitive); },
    [&]() { this->expect("#seg", Qt::CaseInsensitive); },
    [&]() { this->expect("#s", Qt::CaseInsensitive); } );

    if (maybeWhitespace())
    {
        Segment result;
        maybe(result, [&]() { return this->untilComment({"<SEGMENT>"}); });
        return result;
    }
    return _line.mid(_i, 0);
}

QString WallsParser::combineSegments(QString base, Segment offset)
{
    if (offset.value().startsWith('/'))
    {
        return offset.value();
    }

    QStringList baseParts = base.split("/", QString::SkipEmptyParts);
    QList<Segment> offsetParts = offset.split("/", QString::SkipEmptyParts);

    foreach (Segment part, offsetParts)
    {
        if (part.value() == "..")
        {
            if (baseParts.isEmpty())
            {
                throw new SegmentParseException(part, "already at root segment, can't go up");
            }
            baseParts.removeLast();
        }
        else if (part.value() != ".")
        {
            baseParts << part.value();
        }
    }

    return '/' + baseParts.join('/');
}

void WallsParser::prefixLine()
{
    maybeWhitespace();
    prefixDirective();
    maybeWhitespace();
    inlineCommentOrEndOfLine();
}

void WallsParser::prefixDirective()
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

    _units->setPrefix(prefixIndex, prefix);
}

void WallsParser::noteLine()
{
    maybeWhitespace();
    noteDirective();
    maybeWhitespace();
    inlineCommentOrEndOfLine();
}

void WallsParser::noteDirective()
{
    oneOf([&]() { this->expect("#note", Qt::CaseInsensitive); },
    [&]() { this->expect("#n", Qt::CaseInsensitive); });

    whitespace();
    QString station = expect(stationRx, {"<STATION NAME>"}).value();
    whitespace();
    QString note = escapedText([](QChar c) { return c != ';'; }, {"<NOTE>"});

    _visitor->visitNoteLine(station, note);
}

void WallsParser::flagLine()
{
    maybeWhitespace();
    flagDirective();
    maybeWhitespace();
    inlineCommentOrEndOfLine();
}

void WallsParser::flagDirective()
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
        maybeChar(',');
        maybeWhitespace();
    }

    QString flag;
    bool hasFlag = maybe(flag, [&]() { return this->slashPrefixedFlag(); });
    maybeWhitespace();

    if (stations.isEmpty())
    {
        _units->flag = flag;
    }
    else
    {
        if (!hasFlag)
        {
            throwAllExpected();
        }
        _visitor->visitFlaggedStations(flag, stations);
    }

    inlineCommentOrEndOfLine();
}

QString WallsParser::slashPrefixedFlag()
{
    expect('/');
    return expect(notSemicolon, {"<FLAG>"}).value();
}

void WallsParser::symbolLine()
{
    maybeWhitespace();

    oneOf([&]() { this->expect("#symbol", Qt::CaseInsensitive); },
    [&]() { this->expect("#sym", Qt::CaseInsensitive); });

    // ignore the rest for now
    remaining();
}

void WallsParser::dateLine()
{
    maybeWhitespace();
    dateDirective();
    maybeWhitespace();
    inlineCommentOrEndOfLine();
}

void WallsParser::dateDirective()
{
    expect("#date", Qt::CaseInsensitive);
    whitespace();
    oneOfR(_units->date,
    [&]() { return this->isoDate(); },
    [&]() { return this->usDate1(); },
    [&]() { return this->usDate2(); });
}

QDate WallsParser::isoDate()
{
    Segment dateSegment = expect(isoDateRx, {"<DATE>"});
    return QDate::fromString(dateSegment.value(), Qt::ISODate);
}

QDate WallsParser::usDate1()
{
    QString str = expect(usDateRx1, {"<DATE>"}).value();
    return QDate::fromString(str, str.length() > 8 ? "MM-dd-yyyy" : "MM-dd-yy");
}

QDate WallsParser::usDate2()
{
    QString str = expect(usDateRx2, {"<DATE>"}).value();
    return QDate::fromString(str, str.length() > 8 ? "MM/dd/yyyy" : "MM/dd/yy");
}

void WallsParser::unitsLine()
{
    maybeWhitespace();
    oneOf([&]() { this->expect("#units", Qt::CaseInsensitive); },
    [&]() { this->expect("#u", Qt::CaseInsensitive); });

    _visitor->beginUnitsLine();

    try
    {
        unitsOptions();
        _visitor->endUnitsLine();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        _visitor->abortUnitsLine();
        throw ex;
    }
    catch (const SegmentParseException& ex)
    {
        _visitor->abortUnitsLine();
        throw ex;
    }
}

void WallsParser::unitsOptions()
{
    while(!maybe([&]() { this->inlineCommentOrEndOfLine(); } ))
    {
        whitespace();
        maybe([&]() {
            this->oneOf(
                        [&]() { this->unitsOption(); },
                        [&]() { this->macroOption(); }
                        );
        });
    }
}

void WallsParser::unitsOption()
{
    void (WallsParser::*option)() = oneOfMapLowercase(unitsOptionRx, unitsOptionMap);
   (this->*option)();
}

void WallsParser::macroOption()
{
    expect('$');
    QString macroName = expect(macroNameRx, {"<MACRO NAME>"}).value();
    QString macroValue;
    if (maybeChar('='))
    {
        macroValue = quotedTextOrNonwhitespace();
    }
    _macros[macroName] = macroValue;
}

void WallsParser::save()
{
    if (_stack.size() > 10)
    {
        throw SegmentParseException(_line.mid(_i - 4, _i), "units stack is full");
    }
    _stack.push(QSharedPointer<WallsUnits>(new WallsUnits(*_units)));
}

void WallsParser::restore()
{
    if (_stack.isEmpty())
    {
        throw SegmentParseException(_line.mid(_i - 7, _i), "units stack is empty");
    }
    _units = _stack.pop();
}

void WallsParser::reset_()
{
    _units = QSharedPointer<WallsUnits>(new WallsUnits());
}

void WallsParser::meters()
{
    _units->d_unit = _units->s_unit = Length::meters();
}

void WallsParser::feet()
{
    _units->d_unit = _units->s_unit = Length::feet();
}

void WallsParser::ct()
{
    _units->vectorType = VectorType::CT;
}

void WallsParser::d()
{
    expect('=');
    _units->d_unit = oneOfList(lengthUnits, Qt::CaseInsensitive);
}

void WallsParser::s()
{
    expect('=');
    _units->s_unit = oneOfList(lengthUnits, Qt::CaseInsensitive);
}

void WallsParser::a()
{
    expect('=');
    _units->a_unit = oneOfList(azmUnits, Qt::CaseInsensitive);
}

void WallsParser::ab()
{
    expect('=');
    _units->ab_unit = oneOfList(azmUnits, Qt::CaseInsensitive);
}

void WallsParser::a_ab()
{
    expect('=');
    _units->a_unit = _units->ab_unit = oneOfList(azmUnits, Qt::CaseInsensitive);
}

void WallsParser::v()
{
    expect('=');
    _units->v_unit = oneOfList(incUnits, Qt::CaseInsensitive);
}

void WallsParser::vb()
{
    expect('=');
    _units->vb_unit = oneOfList(incUnits, Qt::CaseInsensitive);
}

void WallsParser::v_vb()
{
    expect('=');
    _units->v_unit = _units->vb_unit = oneOfList(incUnits, Qt::CaseInsensitive);
}

void WallsParser::order()
{
    expect('=');
    oneOf([&]() { this->ctOrder(); },
    [&]() { this->rectOrder(); });
}

void WallsParser::ctOrder()
{
    _units->ctOrder = elementChars(ctElements, requiredCtElements);
}

void WallsParser::rectOrder()
{
    _units->rectOrder = elementChars(rectElements, requiredRectElements);
}

void WallsParser::decl()
{
    expect('=');
    _units->decl = azimuthOffset(_units->a_unit);
}

void WallsParser::grid()
{
    expect('=');
    _units->grid = azimuthOffset(_units->a_unit);
}

void WallsParser::rect()
{
    if (maybeChar('='))
    {
        _units->rect = azimuthOffset(_units->a_unit);
    }
    else
    {
        _units->vectorType = VectorType::RECT;
    }
}

void WallsParser::incd()
{
    expect('=');
    _units->incd = length(_units->d_unit);
}

void WallsParser::inch()
{
    expect('=');
    _units->inch = length(_units->s_unit);
}

void WallsParser::incs()
{
    expect('=');
    _units->incs = length(_units->s_unit);
}

void WallsParser::inca()
{
    expect('=');
    _units->inca = azimuthOffset(_units->a_unit);
}

void WallsParser::incab()
{
    expect('=');
    _units->incab = azimuthOffset(_units->ab_unit);
}

void WallsParser::incv()
{
    expect('=');
    _units->incv = inclination(_units->v_unit);
}

void WallsParser::incvb()
{
    expect('=');
    _units->incvb = inclination(_units->vb_unit);
}

void WallsParser::typeab()
{
    expect('=');
    _units->typeab_corrected = oneOfList(correctedValues, Qt::CaseInsensitive);
    _units->typeab_no_average = false;
    _units->typeab_tolerance = NAN;
    if (maybeChar(','))
    {
        _units->typeab_tolerance = unsignedDoubleLiteral();
        if (maybeChar(','))
        {
            expect('x', Qt::CaseInsensitive);
            _units->typeab_no_average = true;
        }
    }
}

void WallsParser::typevb()
{
    expect('=');
    _units->typevb_corrected = oneOfList(correctedValues, Qt::CaseInsensitive);
    _units->typevb_no_average = false;
    _units->typevb_tolerance = NAN;
    if (maybeChar(','))
    {
        _units->typevb_tolerance = unsignedDoubleLiteral();
        if (maybeChar(','))
        {
            expect('x', Qt::CaseInsensitive);
            _units->typevb_no_average = true;
        }
    }
}

void WallsParser::case_()
{
    expect('=');
    _units->case_ = oneOfList(caseTypes, Qt::CaseInsensitive);
}

void WallsParser::lrud()
{
    expect('=');
    _units->lrud = oneOfList(lrudTypes, Qt::CaseInsensitive);
    if (maybeChar(':'))
    {
        lrudOrder();
    }
    else
    {
        _units->lrud_order = QList<LrudElement>({LrudElement::L, LrudElement::R, LrudElement::U, LrudElement::D});
    }
}

void WallsParser::lrudOrder()
{
    _units->lrud_order = elementChars(lrudElements, requiredLrudElements);
}

void WallsParser::prefix1()
{
    prefix(0);
}

void WallsParser::prefix2()
{
    prefix(1);
}

void WallsParser::prefix3()
{
    prefix(2);
}

void WallsParser::prefix(int index)
{
    QString prefix;

    if (maybeChar('='))
    {
        prefix = expect(prefixRx, {"<PREFIX>"}).value();
    }
    _units->setPrefix(index, prefix);
}

void WallsParser::tape()
{
    expect('=');
    _units->tape = oneOfList(tapingMethods, Qt::CaseInsensitive);
}

void WallsParser::uvh()
{
    expect('=');
    _units->uvh = unsignedDoubleLiteral();
}

void WallsParser::uvv()
{
    expect('=');
    _units->uvv = unsignedDoubleLiteral();
}

void WallsParser::uv()
{
    expect('=');
    _units->uv = unsignedDoubleLiteral();
}

void WallsParser::flag()
{
    QString flag;
    if (maybeChar('='))
    {
        flag = quotedTextOrNonwhitespace();
    }
    _units->flag = flag;
}

void WallsParser::vectorLine()
{
    maybeWhitespace();
    fromStation();
    try
    {
        whitespace();
        afterFromStation();
        _visitor->endVectorLine();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        _visitor->abortVectorLine();
        throw ex;
    }
    catch (const SegmentParseException& ex)
    {
        _visitor->abortVectorLine();
        throw ex;
    }
}

void WallsParser::fromStation()
{
    QString from = expect(stationRx, {"<STATION NAME>"}).value();
    if (optionalStationRx.indexIn(from) >= 0) {
        from.clear();
    }
    _visitor->beginVectorLine();
    _visitor->visitFrom(from);
}

void WallsParser::afterFromStation()
{
    oneOf([&]() {
        this->toStation();
        this->whitespace();
        this->afterToStation();
    }, [&]() {
        this->lruds();
        this->afterLruds();
    });
}

void WallsParser::toStation()
{
    QString to = expect(stationRx, {"<STATION NAME>"}).value();
    if (optionalStationRx.indexIn(to) >= 0)
    {
        to.clear();
    }
    _visitor->visitTo(to);
}

void WallsParser::afterToStation()
{
    int k = 0;
    if (_units->vectorType == VectorType::RECT)
    {
        foreach(RectElement elem, _units->rectOrder)
        {
            if (k++ > 0)
            {
                whitespace();
            }
            rectElement(elem);
        }
    }
    else
    {
        foreach(CtElement elem, _units->ctOrder)
        {
            if (k++ > 0)
            {
                whitespace();
            }
            ctElement(elem);
        }
    }
    foreach(TapingMethodElement elem, _units->tape)
    {
        if (!maybeWhitespace())
        {
            break;
        }
        if (!maybe([&]() { this->tapingMethodElement(elem); }))
        {
            break;
        }
    }
    maybeWhitespace();
    afterVectorMeasurements();
}

void WallsParser::rectElement(RectElement elem)
{
    switch(elem) {
    case RectElement::E:
        east();
        break;
    case RectElement::N:
        north();
        break;
    case RectElement::U:
        rectUp();
        break;
    }
}

void WallsParser::east()
{
    _visitor->visitEast(length(_units->d_unit));
}

void WallsParser::north()
{
    _visitor->visitNorth(length(_units->d_unit));
}

void WallsParser::rectUp()
{
    _visitor->visitRectUp(length(_units->d_unit));
}

void WallsParser::ctElement(CtElement elem)
{
    switch(elem)
    {
    case CtElement::D:
        distance();
        break;
    case CtElement::A:
        azimuth();
        break;
    case CtElement::V:
        inclination();
        break;
    }
}

void WallsParser::distance()
{
    _visitor->visitDistance(unsignedLength(_units->d_unit));
}

void WallsParser::azimuth()
{
    UAngle fsAzm;
    if (optional(fsAzm, [&]() { return this->azimuth(_units->a_unit); }))
    {
        _visitor->visitFrontsightAzimuth(fsAzm);
    }
    if (maybeChar('/'))
    {
        UAngle bsAzm;
        if (optional(bsAzm, [&]() { return this->azimuth(_units->a_unit); }))
        {
            _visitor->visitBacksightAzimuth(bsAzm);
        }
    }
}

void WallsParser::inclination()
{
    UAngle fsInc;
    if (optional(fsInc, [&]() { return this->inclination(_units->v_unit); }))
    {
        _visitor->visitFrontsightInclination(fsInc);
    }
    if (maybeChar('/'))
    {
        UAngle bsInc;
        if (optional(bsInc, [&]() { return this->inclination(_units->vb_unit); }))
        {
            _visitor->visitBacksightInclination(bsInc);
        }
    }
}

void WallsParser::tapingMethodElement(TapingMethodElement elem)
{
    switch(elem)
    {
    case TapingMethodElement::InstrumentHeight:
        instrumentHeight();
        break;
    case TapingMethodElement::TargetHeight:
        targetHeight();
        break;
    }
}

void WallsParser::instrumentHeight()
{
    ULength ih;
    if (optional(ih, [&]() { return this->length(_units->s_unit); }))
    {
        _visitor->visitInstrumentHeight(ih);
    }
}

void WallsParser::targetHeight()
{
    ULength th;
    if (optional(th, [&]() { return this->length(_units->s_unit); }))
    {
        _visitor->visitTargetHeight(th);
    }
}

void WallsParser::lrudElement(LrudElement elem)
{
    switch(elem)
    {
    case LrudElement::L:
        left();
        break;
    case LrudElement::R:
        right();
        break;
    case LrudElement::U:
        up();
        break;
    case LrudElement::D:
        down();
        break;
    }
}

void WallsParser::left()
{
    ULength left;
    if (optional(left, [&]() { return this->unsignedLength(_units->s_unit); }))
    {
        _visitor->visitLeft(left);
    }
}

void WallsParser::right()
{
    ULength right;
    if (optional(right, [&]() { return this->unsignedLength(_units->s_unit); }))
    {
        _visitor->visitRight(right);
    }
}

void WallsParser::up()
{
    ULength up;
    if (optional(up, [&]() { return this->unsignedLength(_units->s_unit); }))
    {
        _visitor->visitUp(up);
    }
}

void WallsParser::down()
{
    ULength down;
    if (optional(down, [&]() { return this->unsignedLength(_units->s_unit); }))
    {
        _visitor->visitDown(down);
    }
}

void WallsParser::afterVectorMeasurements()
{
    if (maybe([&]() { return this->varianceOverrides(); }))
    {
        maybeWhitespace();
    }
    afterVectorVarianceOverrides();
}

void WallsParser::varianceOverrides()
{
    expect('(');
    maybeWhitespace();
    VarianceOverridePtr horizontal = varianceOverride(_units->d_unit);
    _visitor->visitHorizontalVarianceOverride(horizontal);
    maybeWhitespace();
    if (maybeChar(','))
    {
        maybeWhitespace();
        VarianceOverridePtr vertical = varianceOverride(_units->d_unit);
        if (horizontal.isNull() && vertical.isNull())
        {
            throw allExpected();
        }
        _visitor->visitVerticalVarianceOverride(vertical);
    }
    else
    {
        _visitor->visitVerticalVarianceOverride(horizontal);
    }
    expect(')');
}

void WallsParser::afterVectorVarianceOverrides()
{
    if (maybe([&]() { this->lruds(); }))
    {
        maybeWhitespace();
    }
    afterLruds();
}

void WallsParser::lruds()
{
    oneOfWithLookahead([&]() {
        this->expect('<');
        this->lrudContent();
        this->expect('>');
    }, [&]() {
        this->expect('*');
        this->lrudContent();
        this->expect('*');
    });
}

void WallsParser::lrudContent()
{
    oneOfWithLookahead([&]() {
        this->commaDelimLrudContent();
    }, [&]() {
        this->whitespaceDelimLrudContent();
    });
}

void WallsParser::commaDelimLrudContent()
{
    maybeWhitespace();
    int m = 0;
    foreach(LrudElement elem, _units->lrud_order)
    {
        if (m++ > 0)
        {
            maybeWhitespace();
            expect(',');
            maybeWhitespace();
        }
        lrudElement(elem);
    }
    maybeWhitespace();
    afterRequiredCommaDelimLrudMeasurements();
}

void WallsParser::whitespaceDelimLrudContent()
{
    maybeWhitespace();
    int m = 0;
    foreach(LrudElement elem, _units->lrud_order)
    {
        if (m++ > 0)
        {
            whitespace();
        }
        lrudElement(elem);
    }
    maybeWhitespace();
    afterRequiredWhitespaceDelimLrudMeasurements();
}

void WallsParser::afterRequiredCommaDelimLrudMeasurements()
{
    if (maybeChar(','))
    {
        maybeWhitespace();
        oneOf([&]() {
            this->lrudFacingAngle();
            this->maybeWhitespace();
            if (this->maybeChar(','))
            {
                this->maybeWhitespace();
                this->lrudCFlag();
            }
        }, [&]() { this->lrudCFlag(); });
    }
}

void WallsParser::afterRequiredWhitespaceDelimLrudMeasurements()
{
    maybe([&]() {
        this->oneOf([&]() {
            this->lrudFacingAngle();
            if (this->maybeWhitespace())
            {
                maybe([&]() { this->lrudCFlag(); });
            }
        },
        [&]() { this->lrudCFlag(); });
    });
}

void WallsParser::lrudFacingAngle()
{
    _visitor->visitLrudFacingAngle(azimuth(_units->a_unit));
}

void WallsParser::lrudCFlag()
{
    expect('c', Qt::CaseInsensitive);
}

void WallsParser::afterLruds()
{
    if (maybe([&]() { this->inlineDirective(); }))
    {
        maybeWhitespace();
    }
    inlineCommentOrEndOfLine();
}

void WallsParser::inlineDirective()
{
    // currently this is the only directive that can be on a vector line
    inlineSegmentDirective();
}

void WallsParser::inlineSegmentDirective()
{
    _visitor->visitInlineSegment(segmentDirective());
}

void WallsParser::fixLine()
{
    maybeWhitespace();
    expect("#fix", Qt::CaseInsensitive);
    whitespace();
    fixedStation();
    try
    {
        whitespace();
        afterFixedStation();
        _visitor->endFixLine();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        _visitor->abortFixLine();
        throw ex;
    }
    catch (const SegmentParseException& ex)
    {
        _visitor->abortFixLine();
        throw ex;
    }
}

void WallsParser::fixedStation()
{
    QString fixed = expect(stationRx, {"<STATION NAME>"}).value();
    _visitor->beginFixLine();
    _visitor->visitFixedStation(fixed);
}

void WallsParser::afterFixedStation()
{
    int k = 0;
    foreach(RectElement elem, _units->rectOrder)
    {
        if (k++ > 0)
        {
            whitespace();
        }
        fixRectElement(elem);
    }
    if (maybeWhitespace())
    {
        afterFixMeasurements();
    }
}

void WallsParser::fixRectElement(RectElement elem)
{
    switch(elem)
    {
    case RectElement::E:
        fixEast();
        break;
    case RectElement::N:
        fixNorth();
        break;
    case RectElement::U:
        fixUp();
        break;
    }
}

void WallsParser::fixEast()
{
    oneOf([&]() { this->_visitor->visitEast(this->length(this->_units->d_unit)); },
    [&]() { this->_visitor->visitLongitude(this->longitude()); });
}

void WallsParser::fixNorth()
{
    oneOf([&]() { this->_visitor->visitNorth(this->length(this->_units->d_unit)); },
    [&]() { this->_visitor->visitLatitude(this->latitude()); });
}

void WallsParser::fixUp()
{
    _visitor->visitRectUp(length(_units->d_unit));
}

void WallsParser::afterFixMeasurements()
{
    if (maybe([&]() { this->varianceOverrides(); }))
    {
        maybeWhitespace();
    }
    afterFixVarianceOverrides();
}

void WallsParser::afterFixVarianceOverrides()
{
    if (maybe([&]() { this->inlineNote(); }))
    {
        maybeWhitespace();
    }
    afterFixInlineNote();
}

void WallsParser::inlineNote()
{
    expect('/');
    _visitor->visitInlineNote(escapedText([](QChar c) { return c != ';' && c != '#'; }, {"<NOTE>"}));
}

void WallsParser::afterFixInlineNote()
{
    if (maybe([&]() { this->inlineDirective(); }))
    {
        maybeWhitespace();
    }
    inlineCommentOrEndOfLine();
}

void WallsParser::inlineCommentOrEndOfLine()
{
    oneOf([&]() { this->inlineComment(); },
    [&]() { this->endOfLine(); });
}

void WallsParser::comment()
{
    expect(';');
    _visitor->visitCommentLine(remaining().value());
}

void WallsParser::inlineComment()
{
    expect(';');
    _visitor->visitInlineComment(remaining().value());
}

} // namespace dewalls
