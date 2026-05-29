/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZLAYERDATA_H
#define CWLAZLAYERDATA_H

#include <QString>

struct cwLazLayerData {
    QString fileName; //!< Basename of the LAZ file inside "GIS Layers/"
    bool enabled = true;
};

#endif // CWLAZLAYERDATA_H
