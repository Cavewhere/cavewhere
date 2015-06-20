#ifndef DEWALLS_SEGMENTPARSEEXPECTEDEXCEPTION_H
#define DEWALLS_SEGMENTPARSEEXPECTEDEXCEPTION_H

#include <QSet>
#include "segmentparseexception.h"
#include <initializer_list>

namespace dewalls {

class SegmentParseExpectedException : public SegmentParseException
{
public:
    SegmentParseExpectedException(Segment segment, QString expectedItem);
    SegmentParseExpectedException(Segment segment, QSet<QString> expectedItems);
    SegmentParseExpectedException(Segment segment, std::initializer_list<QString> expectedItems);
    QSet<QString> expectedItems() const;
    virtual QString detailMessage() const;
private:
    QSet<QString> _expectedItems;
};

inline QSet<QString> SegmentParseExpectedException::expectedItems() const
{
    return _expectedItems;
}

} // namespace dewalls

#endif // DEWALLS_SEGMENTPARSEEXPECTEDEXCEPTION_H
