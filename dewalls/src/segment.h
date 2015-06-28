#ifndef DEWALLS_SEGMENT_H
#define DEWALLS_SEGMENT_H

#include <QSharedPointer>
#include <QString>
#include <QObject>
#include <QRegExp>
#include <QList>

namespace dewalls {

class SegmentImpl;
class Segment;

typedef QSharedPointer<SegmentImpl> SegmentPtr;

/**
 * @brief The data that's implicitly shared by Segment instances
 */
class SegmentImpl
{
public:
    friend class Segment;

    SegmentImpl(QString value, QString source, int startLine, int startCol);
protected:
    SegmentImpl(SegmentPtr sourceSegment, int sourceIndex, QString value, QString source,
            int startLine, int startCol);
    SegmentImpl(SegmentPtr sourceSegment, int sourceIndex, QString value, QString source,
            int startLine, int startCol, int endLine, int endCol);
private:
    SegmentImpl() = delete;
    SegmentImpl(SegmentImpl& other) = delete;
    SegmentImpl& operator=(SegmentImpl& other) = delete;

    SegmentPtr _sourceSegment;
    int _sourceIndex;
    QString _value;
    QString _source;
    int _startLine;
    int _startCol;
    int _endLine;
    int _endCol;
};

/**
 * @brief A wrapper for a QString that maintains information about where it's located in
 * some source file.  Subsegments of this Segment created with mid(), split(), trimmed(), etc.
 * will also contain proper location information.  This way, if a parser encounters a
 * syntax error in any subsegment of a file or line it's operating on, it can underline the
 * error in context using cout << segment.mid(errorStart, errorLen).underlineInContext().toStdString().
 *
 * Many methods simply delegate to the wrapped QString.  Others have an analogous signature but
 * return Segment or QList<Segment> instead of QString or QStringList.
 *
 * Segment uses implicit sharing like many Qt classes.
 */
class Segment
{
public:
    Segment();
    Segment(QString value, QString source, int startLine, int startCol);
    Segment(Segment&& other);

    Segment sourceSegment() const;
    int sourceIndex() const;
    QString value() const;
    QString source() const;
    int startLine() const;
    int startCol() const;
    int endLine() const;
    int endCol() const;

    int length() const;
    bool isEmpty() const;
    const QChar at(int index) const;
    Segment atAsSegment(int index) const;
    Segment mid(int position, int n = -1) const;
    Segment left(int n) const;
    Segment right(int n) const;
    QList<Segment> split(const QRegExp& re, QString::SplitBehavior behavior = QString::SplitBehavior::KeepEmptyParts) const;
    QList<Segment> split(const QString& pattern, QString::SplitBehavior behavior = QString::SplitBehavior::KeepEmptyParts, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    int compare(const QString& other, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    int compare(const QStringRef& other, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool contains(const QString& str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool contains(const QStringRef& str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool contains(const QRegExp& rx) const;
    bool contains(QRegExp& rx) const;
    bool endsWith(const QString& str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool endsWith(const QStringRef& str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    int indexOf(const QString& str, int from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    int indexOf(const QStringRef& str, int from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    int lastIndexOf(const QString& str, int from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    int lastIndexOf(const QStringRef& str, int from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    std::string toStdString() const;
    std::wstring toStdWString() const;
    Segment trimmed() const;
    QString toUpper() const;
    QString toLower() const;
    QString::const_iterator begin() const;
    QString::const_iterator end() const;
    const QChar operator[](int position) const;
    const QChar operator[](uint position) const;
    QString underlineInContext() const;

    inline friend void swap(Segment& first, Segment& second)
    {
        using std::swap;
        swap(first._impl, second._impl);
    }

    Segment& operator =(Segment other);

    friend std::ostream& operator<<(std::ostream& os, const Segment& obj)
    {
        return os << obj.value().toStdString();
    }

protected:
    Segment(SegmentPtr impl);
private:
    SegmentPtr _impl;
};

inline bool exactMatch(QRegExp& rx, const Segment& segment) {
    return rx.exactMatch(segment.value());
}

inline int indexIn(QRegExp& rx, const Segment& segment, int offset = 0, QRegExp::CaretMode caretMode = QRegExp::CaretAtZero) {
    return rx.indexIn(segment.value(), offset, caretMode);
}

inline int lastIndexIn(QRegExp& rx, const Segment& segment, int offset = -1, QRegExp::CaretMode caretMode = QRegExp::CaretAtZero) {
    return rx.lastIndexIn(segment.value(), offset, caretMode);
}

inline Segment cap(QRegExp& rx, const Segment& segment, int nth = 0)
{
    return segment.mid(rx.pos(nth), rx.cap(nth).length());
}

inline Segment::Segment()
    : Segment(QString(), QString(), 0, 0)
{

}

inline Segment::Segment(Segment&& other)
{
    swap(*this, other);
}

inline Segment& Segment::operator =(Segment other)
{
    swap(*this, other);
    return *this;
}

inline Segment Segment::sourceSegment() const
{
    return Segment(_impl->_sourceSegment);
}

inline int Segment::sourceIndex() const
{
    return _impl->_sourceIndex;
}

inline QString Segment::value() const
{
    return _impl->_value;
}

inline QString Segment::source() const
{
    return _impl->_source;
}

inline int Segment::startLine() const
{
    return _impl->_startLine;
}

inline int Segment::startCol() const
{
    return _impl->_startCol;
}

inline int Segment::endLine() const
{
    return _impl->_endLine;
}

inline int Segment::endCol() const
{
    return _impl->_endCol;
}

inline int Segment::length() const
{
    return _impl->_value.length();
}

inline bool Segment::isEmpty() const
{
    return _impl->_value.isEmpty();
}

inline const QChar Segment::at(int i) const
{
    return _impl->_value.at(i);
}

inline Segment Segment::atAsSegment(int i) const
{
    return mid(i, i == length() ? 0 : 1);
}

inline Segment Segment::left(int n) const
{
    return mid(0, n);
}

inline Segment Segment::right(int n) const
{
    return mid(length() - n, n);
}

inline int Segment::compare(const QString& other, Qt::CaseSensitivity cs) const
{
    return value().compare(other, cs);
}

inline int Segment::compare(const QStringRef& other, Qt::CaseSensitivity cs) const
{
    return value().compare(other, cs);
}

inline bool Segment::contains(const QString& str, Qt::CaseSensitivity cs) const
{
    return value().contains(str, cs);
}
inline bool Segment::contains(const QStringRef& str, Qt::CaseSensitivity cs) const
{
    return value().contains(str, cs);
}
inline bool Segment::contains(const QRegExp& rx) const
{
    return value().contains(rx);
}
inline bool Segment::contains(QRegExp& rx) const
{
    return value().contains(rx);
}
inline bool Segment::endsWith(const QString& str, Qt::CaseSensitivity cs) const
{
    return value().endsWith(str, cs);
}
inline bool Segment::endsWith(const QStringRef& str, Qt::CaseSensitivity cs) const
{
    return value().endsWith(str, cs);
}
inline int Segment::indexOf(const QString& str, int from, Qt::CaseSensitivity cs) const
{
    return value().indexOf(str, from, cs);
}
inline int Segment::indexOf(const QStringRef& str, int from, Qt::CaseSensitivity cs) const
{
    return value().indexOf(str, from, cs);
}
inline int Segment::lastIndexOf(const QString& str, int from, Qt::CaseSensitivity cs) const
{
    return value().lastIndexOf(str, from, cs);
}
inline int Segment::lastIndexOf(const QStringRef& str, int from, Qt::CaseSensitivity cs) const
{
    return value().lastIndexOf(str, from, cs);
}
inline std::string Segment::toStdString() const
{
    return value().toStdString();
}
inline std::wstring Segment::toStdWString() const
{
    return value().toStdWString();
}
inline Segment Segment::trimmed() const
{
    QString val = value();
    auto start = val.begin();
    auto end   = val.end();
    while (start < end && start->isSpace()) start++;
    while (end > start && end  ->isSpace()) end--;
    return mid(start - val.begin(), end - start);
}
inline QString Segment::toUpper() const
{
    return value().toUpper();
}
inline QString Segment::toLower() const
{
    return value().toLower();
}

inline QString::const_iterator Segment::begin() const
{
    return value().begin();
}

inline QString::const_iterator Segment::end() const
{
    return value().end();
}

inline const QChar Segment::operator [](int position) const
{
    return value()[position];
}

inline const QChar Segment::operator [](uint position) const
{
    return value()[position];
}

} // namespace dewalls

#endif // DEWALLS_SEGMENT_H
