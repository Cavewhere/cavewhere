/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvex3DFileReader.h"

//Survex library
#include "img.h"

//Qt includes
#include <QVector3D>
#include <QDebug>

/**
 * @brief Reads station positions from a survex .3d file
 * @param threeDFilePath - path to the .3d file
 * @return cwStationPositionLookup containing station name -> position mapping
 *
 * This replaces the old pipeline of running survexport to produce a CSV file
 * and then parsing it. The img.h API reads .3d files directly.
 */
cwStationPositionLookup cwSurvex3DFileReader::readStationPositions(const QString& threeDFilePath)
{
    cwStationPositionLookup lookup;

    img* pimg = img_open(threeDFilePath.toUtf8().constData());
    if (!pimg) {
        qWarning() << "cwSurvex3DFileReader: Failed to open .3d file:" << threeDFilePath;
        return lookup;
    }

    img_point pt;
    int code;
    while ((code = img_read_item(pimg, &pt)) != img_STOP) {
        if (code == img_BAD) {
            qWarning() << "cwSurvex3DFileReader: Error reading .3d file:" << threeDFilePath;
            break;
        }
        if (code == img_LABEL) {
            // Skip anonymous stations - survexport --station-names excluded these
            if (pimg->flags & img_SFLAG_ANON) {
                continue;
            }
            QString name = QString::fromUtf8(pimg->label);
            lookup.setPosition(name, QVector3D(pt.x, pt.y, pt.z));
        }
    }

    img_close(pimg);
    return lookup;
}
