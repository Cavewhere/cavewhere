#include "segmentparseexpectedexception.h"
#include <QStringList>

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
    QSet<QString> uniq;
    QStringList uniqList;

    foreach (QString item, _expectedItems)
    {
        if (!uniq.contains(item))
        {
            uniq << item;
            uniqList << item;
        }
    }

    if (uniqList.size() == 1)
    {
        return QString("Expected \"%1\"").arg(uniqList.first());
    }

    QString result("Expected one of:\n");
    foreach (QString item, uniqList)
    {
        result += "  " + item + "\n";
    }
    return result;
}

} // namespace dewalls

