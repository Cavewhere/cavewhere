#include "lineparser.h"

namespace dewalls {

const QRegExp LineParser::whitespaceRx("\\s+");
const QRegExp LineParser::nonwhitespaceRx("\\S+");

LineParser::LineParser()
    : LineParser(Segment(QString(), QString(), 0, 0))
{

}

LineParser::LineParser(Segment line)
    : _line(line),
      _i(0),
      _expectedIndex(0),
      _expectedItems(QStringList())
{

}

void LineParser::reset(Segment newLine)
{
    _line = newLine;
    _i = 0;
    _expectedIndex = 0;
    _expectedItems.clear();
}

bool LineParser::isAtEnd() const
{
    return _i == _line.length();
}

void LineParser::addExpected(const SegmentParseExpectedException& expected)
{
    int index = std::max(expected.segment().sourceIndex(), 0) -
            std::max(_line.sourceIndex(), 0);

    if (index > _expectedIndex)
    {
        _expectedItems.clear();
        _expectedIndex = index;
    }
    if (index == _expectedIndex)
    {
        _expectedItems << expected.expectedItems();
    }
}

SegmentParseExpectedException LineParser::allExpected()
{
    if (!_expectedItems.isEmpty())
    {
        return SegmentParseExpectedException(
                    _line.atAsSegment(_i), _expectedItems);
    }
    return SegmentParseExpectedException(
                _line.atAsSegment(_i), "<UNKNOWN>");
}

void LineParser::throwAllExpected()
{
    if (!_expectedItems.isEmpty())
    {
        throw SegmentParseExpectedException(
                    _line.atAsSegment(_i), _expectedItems);
    }
}

void LineParser::throwAllExpected(const SegmentParseExpectedException& finalEx)
{
    addExpected(finalEx);
    throwAllExpected();
}

void LineParser::expect(const QChar &c, Qt::CaseSensitivity cs)
{
    if (_i < _line.length())
    {
        QChar lhs = c;
        QChar rhs = _line.at(_i);
        if (cs == Qt::CaseInsensitive)
        {
            lhs = lhs.toLower();
            rhs = rhs.toLower();
        }
        if (lhs == rhs) {
            _i++;
            return;
        }
    }
    throw SegmentParseExpectedException(_line.atAsSegment(_i), QString(c));
}

void LineParser::expect(const QString &c, Qt::CaseSensitivity cs)
{
    if (_i + c.length() <= _line.length())
    {
        QString lhs = c;
        QString rhs = _line.mid(_i, c.length()).value();
        if (cs == Qt::CaseInsensitive)
        {
            lhs = lhs.toLower();
            rhs = rhs.toLower();
        }
        if (lhs == rhs) {
            _i += c.length();
            return;
        }
    }
    throw SegmentParseExpectedException(_line.atAsSegment(_i), c);
}

Segment LineParser::expect(const QRegExp &rx, std::initializer_list<QString> expectedItems)
{
    QRegExp rxcopy = rx;
    int index = indexIn(rxcopy, _line, _i);
    if (index != _i) {
        throw SegmentParseExpectedException(_line.atAsSegment(_i), expectedItems);
    }
    int start = _i;
    _i += rxcopy.matchedLength();
    return _line.mid(start, rxcopy.matchedLength());
}

Segment LineParser::expect(QRegExp &rx, std::initializer_list<QString> expectedItems)
{
    int index = indexIn(rx, _line, _i);
    if (index != _i) {
        throw SegmentParseExpectedException(_line.atAsSegment(_i), expectedItems);
    }
    int start = _i;
    _i += rx.matchedLength();
    return _line.mid(start, rx.matchedLength());
}

Segment LineParser::expect(QRegExp &rx, QList<QString> expectedItems)
{
    int index = indexIn(rx, _line, _i);
    if (index != _i) {
        throw SegmentParseExpectedException(_line.atAsSegment(_i), expectedItems);
    }
    int start = _i;
    _i += rx.matchedLength();
    return _line.mid(start, rx.matchedLength());
}

Segment LineParser::whitespace()
{
    return expect(whitespaceRx, {"<WHITESPACE>"});
}

bool LineParser::maybeWhitespace()
{
//    return maybe([&]() { this->whitespace(); } );
    return maybeOwn(&LineParser::whitespace);
}

Segment LineParser::nonwhitespace()
{
    return expect(nonwhitespaceRx, {"<NONWHITESPACE>"});
}

const QRegExp LineParser::unsignedDoubleLiteralRx("\\d+(\\.\\d*)?|\\.\\d+");

double LineParser::unsignedDoubleLiteral()
{
    return expect(unsignedDoubleLiteralRx, {"<UNSIGNED_DOUBLE_LITERAL>"}).value().toDouble();
}

QHash<QChar, double> createSignSignums()
{
    QHash<QChar, double> signSignums;
    signSignums['-'] = -1.0;
    signSignums['+'] = 1.0;
    return signSignums;
}

const QHash<QChar, double> LineParser::signSignums = createSignSignums();

double LineParser::doubleLiteral()
{
    double signum;
    if (!maybe(signum, [this]() { return this->oneOfMap(signSignums); } ))
    {
        signum = 1.0;
    }
    return signum * unsignedDoubleLiteral();
}

void LineParser::endOfLine()
{
    if (_i != _line.length())
    {
        throw SegmentParseExpectedException(_line.atAsSegment(_i), "<END_OF_LINE>");
    }
}

Segment LineParser::remaining()
{
    Segment result = _line.mid(_i);
    _i = _line.length();
    return result;
}

bool LineParser::maybeChar(QChar c)
{
    return maybe([&]() { this->expect(c); });
}

} // namespace dewalls

