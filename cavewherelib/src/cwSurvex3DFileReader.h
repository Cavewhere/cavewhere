/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEX3DFILEREADER_H
#define CWSURVEX3DFILEREADER_H

//Our includes
#include "cwGeoPoint.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QString>

/**
 * Reads cavern's .3d output into a position lookup and a survey network.
 *
 * Coordinates arrive as doubles in the region's globalCS, so a georeferenced
 * cave's are at full UTM magnitude. \a worldOrigin is subtracted before they
 * are narrowed to QVector3D, which cannot hold a UTM coordinate. Callers with
 * no georeference pass a default-constructed cwGeoPoint.
 */
class CAVEWHERE_LIB_EXPORT cwSurvex3DFileReader
{
public:
    struct NetworkAndLookup {
        cwSurveyNetwork network;
        cwStationPositionLookup lookup;
    };

    // Parses a .3d file once, returning both the survey network (station names,
    // shot connectivity, positions) and a standalone position lookup. Two-pass
    // using img_rewind(): pass 1 indexes LABEL items, pass 2 resolves LINE
    // endpoints by coordinate match.
    NetworkAndLookup readNetworkAndLookup(const QString& threeDFilePath,
                                          const cwGeoPoint& worldOrigin);
};

#endif // CWSURVEX3DFILEREADER_H
