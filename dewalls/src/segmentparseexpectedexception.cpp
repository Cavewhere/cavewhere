#include "segmentparseexpectedexception.h"

namespace dewalls {

SegmentParseExpectedException::SegmentParseExpectedException(Segment segment, QList<QString> expectedItems)
    : SegmentParseException(segment),
      _expectedItems(expectedItems)
{

}

SegmentParseExpectedException::SegmentParseExpectedException(Segment segment, QString expectedItem)
    : SegmentParseExpectedException(segment, QList<QString>({expectedItem}))
{
}

SegmentParseExpectedException::SegmentParseExpectedException(Segment segment, std::initializer_list<QString> expectedItems)
    : SegmentParseExpectedException(segment, QList<QString>(expectedItems))
{
}

QString SegmentParseExpectedException::detailMessage() const
{
    if (_expectedItems.size() == 1)
    {
        return QString("Expected \"%1\"").arg(_expectedItems.first());
    }

    QString result("Expected one of:\n");
    for (QString item : _expectedItems)
    {
        result += "  " + item + "\n";
    }
    return result;
}

} // namespace dewalls

