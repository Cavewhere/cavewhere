#include "segmentparseexpectedexception.h"

namespace dewalls {

SegmentParseExpectedException::SegmentParseExpectedException(Segment segment, QString expectedItem)
    : SegmentParseException(segment),
      _expectedItems(QSet<QString>())
{
    _expectedItems << expectedItem;
}

SegmentParseExpectedException::SegmentParseExpectedException(Segment segment, QSet<QString> expectedItems)
    : SegmentParseException(segment),
      _expectedItems(expectedItems)
{

}

SegmentParseExpectedException::SegmentParseExpectedException(Segment segment, std::initializer_list<QString> expectedItems)
    : SegmentParseException(segment),
      _expectedItems(QSet<QString>())
{
    for (auto item : expectedItems)
    {
        _expectedItems << item;
    }
}

QString SegmentParseExpectedException::detailMessage() const
{
    if (_expectedItems.size() == 1)
    {
        return QString("Expected \"%1\"").arg(*_expectedItems.begin());
    }

    QString result("Expected one of:\n");
    for (QString item : _expectedItems)
    {
        result += "  " + item + "\n";
    }
    return result;
}

} // namespace dewalls

