#ifndef DEWALLS_SEGMENTPARSEEXCEPTION_H
#define DEWALLS_SEGMENTPARSEEXCEPTION_H

#include <exception>

#include "segment.h"

namespace dewalls {

class SegmentParseException : public std::exception
{
public:
    SegmentParseException(Segment segment);
    Segment segment() const;
    virtual QString detailMessage() const;
    virtual QString message() const;
private:
    Segment _segment;
};

inline Segment SegmentParseException::segment() const
{
    return _segment;
}

inline QString SegmentParseException::detailMessage() const
{
    return "";
}

} // namespace dewalls

#endif // DEWALLS_SEGMENTPARSEEXCEPTION_H
