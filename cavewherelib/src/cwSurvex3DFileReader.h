/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEX3DFILEREADER_H
#define CWSURVEX3DFILEREADER_H

//Our includes
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QString>

class CAVEWHERE_LIB_EXPORT cwSurvex3DFileReader
{
public:
    struct NetworkAndLookup {
        cwSurveyNetwork network;
        cwStationPositionLookup lookup;
    };

    cwStationPositionLookup readStationPositions(const QString& threeDFilePath);

    // Parses a .3d file once, returning both the survey network (station names,
    // shot connectivity, positions) and a standalone position lookup. Two-pass
    // using img_rewind(): pass 1 indexes LABEL items, pass 2 resolves LINE
    // endpoints by coordinate match.
    NetworkAndLookup readNetworkAndLookup(const QString& threeDFilePath);
};

#endif // CWSURVEX3DFILEREADER_H
