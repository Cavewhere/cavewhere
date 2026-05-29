/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZLAYERDATA_H
#define CWLAZLAYERDATA_H

#include <QString>
#include <QUuid>

struct cwLazLayerData {
    QString fileName; //!< Basename of the LAZ file inside "GIS Layers/"
    QUuid id;         //!< Persistent layer identity loaded from .cwlaz; null means "use the layer's auto-generated id"
    bool enabled = true;
};

#endif // CWLAZLAYERDATA_H
