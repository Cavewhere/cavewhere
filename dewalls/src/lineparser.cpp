#include "lineparser.h"

namespace dewalls {

LineParser::LineParser(Segment line)
    : _line(line),
      _i(0),
      _expectedIndex(0),
      _expectedItems(QSet<QString>())
{

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
        _expectedItems |= expected.expectedItems();
    }
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


} // namespace dewalls

