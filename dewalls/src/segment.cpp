#include "segment.h"
#include <QRegExp>
#include <iostream>

namespace dewalls {

SegmentImpl::SegmentImpl(SegmentPtr sourceSegment, int sourceIndex, QString value, QString source,
            int startLine, int startCol, int endLine, int endCol)
    : _sourceSegment(sourceSegment),
      _sourceIndex(sourceIndex),
      _value(value),
      _source(source),
      _startLine(startLine),
      _startCol(startCol),
      _endLine(endLine),
      _endCol(endCol)
{

}

SegmentImpl::SegmentImpl(SegmentPtr sourceSegment, int sourceIndex, QString value, QString source,
            int startLine, int startCol)
    : _sourceSegment(sourceSegment),
      _sourceIndex(sourceIndex),
      _value(value),
      _source(source),
      _startLine(startLine),
      _startCol(startCol),
      _endLine(startLine),
      _endCol(startCol + value.length() - 1)
{
    QRegExp LINE_BREAK("\r\n|\r|\n");

    int pos = 0;
    while ((pos = LINE_BREAK.indexIn(value, pos)) != -1)
    {
        int end = pos + LINE_BREAK.matchedLength();
        if (end >= value.length())
        {
            break;
        }
        _endLine++;
        _endCol = value.length() - end - 1;
        pos = end;
    }
}

SegmentImpl::SegmentImpl(QString value, QString source, int startLine, int startCol)
    : SegmentImpl(SegmentPtr(), -1, value, source, startLine, startCol)
{

}

Segment::Segment(SegmentPtr impl)
    : _impl(impl)
{

}

Segment::Segment(QString value, QString source, int startLine, int startCol)
    : Segment(SegmentPtr(new SegmentImpl(value, source, startLine, startCol)))
{

}

Segment Segment::mid(int position, int n) const
{
    if (n < 0)
    {
        n = length();
    }

    if (startLine() == endLine())
    {
        return Segment(SegmentPtr(new SegmentImpl(
                _impl->_sourceSegment.isNull() ? _impl : _impl->_sourceSegment,
                sourceIndex() >= 0 ? sourceIndex() + position : position,
                value().mid(position, n),
                source(),
                startLine(),
                startCol() + position,
                startLine(),
                startCol() + position + n - 1)));
    }

    int newStartLine = startLine();
    int newStartCol = startCol() + position;

    int toIndex = position;
    if (toIndex < value().length() && toIndex > 0 && value().at(toIndex) == '\n'
            && value().at(toIndex - 1) == '\r')
    {
        toIndex--;
    }

    QRegExp LINE_BREAK("\r\n|\r|\n");

    int pos = 0;
    while ((pos = LINE_BREAK.indexIn(value(), pos)) >= 0)
    {
        if (pos >= toIndex)
        {
            break;
        }
        newStartLine++;
        pos += LINE_BREAK.matchedLength();
        newStartCol = position - pos;
    }

    return Segment(SegmentPtr(new SegmentImpl(
              _impl->_sourceSegment.isNull() ? _impl : _impl->_sourceSegment,
              sourceIndex() >= 0 ? sourceIndex() + position : position,
              value().mid(position, n),
              source(),
              newStartLine,
              newStartCol)));
}

QList<Segment> Segment::split(const QRegExp &re, QString::SplitBehavior behavior) const
{
    QList<Segment> matchList;

    QRegExp matcher = re;

    int partStart = 0;
    int matchStart = 0;
    while ((matchStart = matcher.indexIn(value(), partStart)) >= 0)
    {
        if (behavior == QString::SplitBehavior::KeepEmptyParts || matchStart > partStart)
        {
            matchList << mid(partStart, matchStart - partStart);
        }
        partStart = matchStart + matcher.matchedLength();
    }

    if (behavior == QString::SplitBehavior::KeepEmptyParts || partStart < length())
    {
        matchList << mid(partStart);
    }

    return matchList;
}

QList<Segment> Segment::split(const QString &pattern, QString::SplitBehavior behavior, Qt::CaseSensitivity cs) const
{
    return split(QRegExp(pattern, cs), behavior);
}

QString Segment::underlineInContext() const
{
    QString result;
    QList<Segment> lines = _impl->_sourceSegment.isNull() ? split("\r\n|\r|\n") :
                                                            Segment(_impl->_sourceSegment).split("\r\n|\r|\n");

    for (auto line = lines.cbegin(); line < lines.cend(); line++)
    {
        if (line->startLine() < startLine() || line ->startLine() > endLine())
        {
            continue;
        }
        result.append(line->value()).append('\n');
        int k = 0;
        if (startLine() == line->startLine())
        {
            result.append(QString(startCol() - k, ' '));
            k = startCol();
        }
        if (line->startLine() < endLine())
        {
            result.append(QString(line->length() - k, '^'));
            k = line->length();
        }
        else if (endLine() == line->startLine())
        {
            if (endCol() < startCol())
            {
                result.append('^');
            }
            else
            {
                result.append(QString(endCol() + 1 - k, '^'));
                k = endCol() + 1;
            }
        }
        if (line->startLine() < endLine())
        {
            result.append('\n');
        }
    }

    return result;
}

} // namespace dewalls

