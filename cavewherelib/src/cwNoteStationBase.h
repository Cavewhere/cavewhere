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
    NoteStationBase()
        : m_data(new PrivateData()) { }

    void setPositionOnNote(const PositionType& point) {
        m_data->PositionOnNote = point;
    }

    PositionType positionOnNote() const {
        return m_data->PositionOnNote;
    }

    void setName(const QString& name) {
        m_data->StationName = name;
    }

    QString name() const {
        return m_data->StationName;
    }

    bool isValid() const {
        return !name().isEmpty();
    }

    bool operator==(const NoteStationBase& other) const {
        return other.name() == name() && other.positionOnNote() == positionOnNote();
    }

private:
    class PrivateData : public QSharedData {
    public:
        PrivateData() { }
        PositionType PositionOnNote;
        QString StationName;
    };

    QSharedDataPointer<PrivateData> m_data;
};

// qHash overload (hash by name only, matches your existing behavior)
template <typename PositionType>
inline size_t qHash(const NoteStationBase<PositionType>& note) {
    return qHash(note.name());
}

#endif // CWNOTESTATIONBASE_H
