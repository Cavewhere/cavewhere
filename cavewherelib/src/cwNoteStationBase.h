#ifndef CWNOTESTATIONBASE_H
#define CWNOTESTATIONBASE_H

// Qt includes
#include <QSharedData>
#include <QSharedDataPointer>
#include <QString>
#include <QHash>

// Our includes
#include "cwGlobals.h"

template <typename PositionType>
class CAVEWHERE_LIB_EXPORT NoteStationBase
{
public:
    NoteStationBase() {}

    void setPositionOnNote(const PositionType& point) {
        m_positionOnNote = point;
    }

    PositionType positionOnNote() const {
        return m_positionOnNote;
    }

    void setName(const QString& name) {
        m_stationName = name;
    }

    QString name() const {
        return m_stationName;
    }

    bool isValid() const {
        return !name().isEmpty();
    }

    bool operator==(const NoteStationBase& other) const {
        return other.name() == name() && other.positionOnNote() == positionOnNote();
    }

private:
    PositionType m_positionOnNote;
    QString m_stationName;
};

// qHash overload (hash by name only, matches your existing behavior)
template <typename PositionType>
inline size_t qHash(const NoteStationBase<PositionType>& note) {
    return qHash(note.name());
}

#endif // CWNOTESTATIONBASE_H
