#ifndef CWNOTESTATION_H
#define CWNOTESTATION_H

// Qt includes
#include <QPointF>

// Our includes
#include "cwGlobals.h"
#include "cwNoteStationBase.h"

class CAVEWHERE_LIB_EXPORT cwNoteStation : public NoteStationBase<QPointF>
{
public:
    cwNoteStation() : NoteStationBase<QPointF>() { }
};

#endif // CWNOTESTATION_H
