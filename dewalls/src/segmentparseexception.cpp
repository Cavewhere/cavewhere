#include "segmentparseexception.h"

namespace dewalls {

QString SegmentParseException::message() const
{
    return detailMessage() + QString(" (in %1, line %2, column %3):\n%4").arg(
                _segment.source(),
                QString::number(_segment.startLine() + 1),
                QString::number(_segment.startCol() + 1),
                _segment.underlineInContext());
}


} // namespace dewalls

