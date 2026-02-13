#ifndef CWNOTELIDARSTATION_H
#define CWNOTELIDARSTATION_H

// Qt includes
#include <QVector3D>
#include <QUuid>

// Our includes
#include "cwGlobals.h"
#include "cwNoteStationBase.h"

class CAVEWHERE_LIB_EXPORT cwNoteLiDARStation : public cwNoteStationBase<QVector3D>
{
public:
    cwNoteLiDARStation()
        : cwNoteStationBase<QVector3D>()
        , m_id(QUuid::createUuid())
    {
    }

    void setId(const QUuid& id) { m_id = id; }
    QUuid id() const { return m_id; }
    bool operator==(const cwNoteLiDARStation& other) const
    {
        return id() == other.id()
               && name() == other.name()
               && positionOnNote() == other.positionOnNote();
    }

private:
    QUuid m_id;
};

#endif // CWNOTELIDARSTATION_H
