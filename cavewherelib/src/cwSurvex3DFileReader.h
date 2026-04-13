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
#include "CaveWhereLibExport.h"

//Qt includes
#include <QString>

class CAVEWHERE_LIB_EXPORT cwSurvex3DFileReader
{
public:
    cwStationPositionLookup readStationPositions(const QString& threeDFilePath);
};

#endif // CWSURVEX3DFILEREADER_H
