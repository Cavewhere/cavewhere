#ifndef DEWALLS_LINEPARSER_H
#define DEWALLS_LINEPARSER_H

#include "segment.h"
#include <QString>
#include <QChar>
#include <QSet>
#include <QHash>
#include "segmentparseexpectedexception.h"
#include <initializer_list>

namespace dewalls {

class LineParser
{
public:
    LineParser(Segment line);

    void addExpected(const SegmentParseExpectedException& ex);

    void throwAllExpected();
    void throwAllExpected(const SegmentParseExpectedException& finalEx);

    void expect(const QChar& c, Qt::CaseSensitivity cs = Qt::CaseSensitive);
    void expect(const QString& c, Qt::CaseSensitivity cs = Qt::CaseSensitive);
    Segment expect(const QRegExp& rx, std::initializer_list<QString> expectedItems);
    Segment expect(QRegExp& rx, std::initializer_list<QString> expectedItems);

    template<typename V>
    V oneOf(QHash<QChar, V> map);

    template<typename V>
    V oneOfLowercase(QHash<QChar, V> map);

    template<typename F>
    void throwAllExpected(F production);

    template<typename F>
    void oneOf(F production);

    template<typename F, typename... Args>
    void oneOf(F production, Args... args);

    template<typename R, typename F>
    void oneOf(R& result, F production);

    template<typename R, typename F, typename... Args>
    void oneOf(R& result, F production, Args... args);

    template<typename F>
    void oneOfWithLookahead(F production);

    template<typename F, typename... Args>
    void oneOfWithLookahead(F production, Args... args);

    template<typename F>
    bool maybe(F production);

    template<typename R, typename F>
    bool maybe(R& result, F production);

protected:
    Segment _line;
    int _i;
    int _expectedIndex;
    QSet<QString> _expectedItems;
};

template<typename F>
void LineParser::throwAllExpected(F production)
{
    try
    {
        production();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        throwAllExpected(ex);
    }
}

template<typename F>
void LineParser::oneOf(F production)
{
    try
    {
        production();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        throwAllExpected(ex);
    }
}

template<typename F, typename... Args>
void LineParser::oneOf(F production, Args... args)
{
    int start = _i;
    try
    {
        production();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        if (_i > start)
        {
            throwAllExpected(ex);
        }
        addExpected(ex);
        oneOf(args...);
    }
}

template<typename R, typename F>
void LineParser::oneOf(R& result, F production)
{
    try
    {
        result = production();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        throwAllExpected(ex);
    }
}

template<typename R, typename F, typename... Args>
void LineParser::oneOf(R& result, F production, Args... args)
{
    int start = _i;
    try
    {
        result = production();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        if (_i > start)
        {
            throwAllExpected(ex);
        }
        addExpected(ex);
        oneOf(result, args...);
    }
}

template<typename F>
void LineParser::oneOfWithLookahead(F production)
{
    try
    {
        production();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        throwAllExpected(ex);
    }
}

template<typename F, typename... Args>
void LineParser::oneOfWithLookahead(F production, Args... args)
{
    int start = _i;
    try
    {
        production();
    }
    catch (const SegmentParseExpectedException& ex)
    {
        addExpected(ex);
        _i = start;
        oneOfWithLookahead(args...);
    }
}

template<typename V>
V LineParser::oneOf(QHash<QChar, V> map)
{
    QChar c;
    if (_i >= _line.length() || !map.contains(c = _line.at(_i)))
    {
        QSet<QString> expectedItems;
        for (QChar exp : map.keys())
        {
            expectedItems << QString(exp);
        }
        throw SegmentParseExpectedException(_line.atAsSegment(_i), expectedItems);
    }
    _i++;
    return map[c];
}

template<typename V>
V LineParser::oneOfLowercase(QHash<QChar, V> map)
{
    QChar c;
    if (_i >= _line.length() || !map.contains(c = _line.at(_i).toLower()))
    {
        QSet<QString> expectedItems;
        for (QChar exp : map.keys())
        {
            expectedItems << QString(exp);
        }
        throw SegmentParseExpectedException(_line.atAsSegment(_i), expectedItems);
    }
    _i++;
    return map[c];
}

template<typename F>
bool LineParser::maybe(F production)
{
    int start = _i;
    try
    {
        production();
        return true;
    }
    catch (const SegmentParseExpectedException& ex)
    {
        if (_i > start)
        {
            throwAllExpected(ex);
        }
        addExpected(ex);
        return false;
    }
}

template<typename R, typename F>
bool LineParser::maybe(R& result, F production)
{
    int start = _i;
    try
    {
        result = production();
        return true;
    }
    catch (const SegmentParseExpectedException& ex)
    {
        if (_i > start)
        {
            throwAllExpected(ex);
        }
        addExpected(ex);
        return false;
    }
}

} // namespace dewalls

#endif // DEWALLS_LINEPARSER_H
