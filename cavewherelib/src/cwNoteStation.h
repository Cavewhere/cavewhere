#ifndef CWNOTESTATION_H
#define CWNOTESTATION_H

// Qt includes
#include <QPointF>
#include <QUuid>

// Our includes
#include "cwGlobals.h"
#include "cwNoteStationBase.h"

class CAVEWHERE_LIB_EXPORT cwNoteStation : public cwNoteStationBase<QPointF>
{
public:
    cwNoteStation()
        : cwNoteStationBase<QPointF>()
        , m_id(QUuid::createUuid())
    {
    }

    void setId(const QUuid& id) { m_id = id; }
    QUuid id() const { return m_id; }

private:
    QUuid m_id;
};

#endif // CWNOTESTATION_H
