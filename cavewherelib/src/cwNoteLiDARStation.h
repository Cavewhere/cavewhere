#ifndef CWNOTELIDARSTATION_H
#define CWNOTELIDARSTATION_H

// Qt includes
#include <QVector3D>

// Our includes
#include "cwGlobals.h"
#include "cwNoteStationBase.h"

class CAVEWHERE_LIB_EXPORT cwNoteLiDARStation : public NoteStationBase<QVector3D>
{
public:
    cwNoteLiDARStation() : NoteStationBase<QVector3D>() { }
};

#endif // CWNOTELIDARSTATION_H
