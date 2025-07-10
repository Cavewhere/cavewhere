/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRegionIOTask.h"
#include "cwCavingRegion.h"

//Qt includes
#include <QThread>

cwRegionIOTask::cwRegionIOTask(QObject* parent) :
    cwProjectIOTask(parent)
{
    Region = new cwCavingRegion();
    Region->moveToThread(nullptr);
}

cwRegionIOTask::~cwRegionIOTask()
{
    delete Region;
}

/**
      This does a deap copy of the region, so it make a snapshot
      */
void cwRegionIOTask::setCavingRegion(const cwCavingRegion& region) {
    *Region = region;
}

/**
 * @brief cwRegionIOTask::copyRegionTo
 * @param region
 *
 * This will copy the data out the region io task into region.
 *
 * This fuction makes sure that the thread for region is correct when coping
 * the region.
 */
void cwRegionIOTask::copyRegionTo(cwCavingRegion &region)
{
    Q_ASSERT(Region->thread() == nullptr);

    //Preform copy
    region = *Region;
}

/**
 * @brief cwRegionIOTask::version
 * @return Returns the current version
 */
int cwRegionIOTask::protoVersion()
{
    //Version 7 converts cavewhere to use json and git with multiple files
    return 7;
}

/**
 * @brief cwRegionIOTask::toVersion
 * @param protoVersion
 * @return
 */
QString cwRegionIOTask::toVersion(int protoVersion)
{
    static const QHash<int, QString> protoToVersionString = {
        {0, "0.07"},
        {1, "0.08"},
        {2, "0.09-beta1"},
        {3, "0.09-beta2"},
        {4, "1.0-projectedProfile"},
        {5, "1.0-projectedProfile-v2"},
        {6, "2025.3"},
        {7, "Set the version"}
    };

    return protoToVersionString.value(protoVersion, "Unknown Version");
}

