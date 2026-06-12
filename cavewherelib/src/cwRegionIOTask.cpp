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

}

cwRegionIOTask::~cwRegionIOTask()
{

}

/**
      This does a deap copy of the region, so it make a snapshot
      */
void cwRegionIOTask::setCavingRegion(const cwCavingRegionData& region) {
    Region = region;
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
void cwRegionIOTask::copyRegionTo(cwCavingRegion* region)
{
    // Q_ASSERT(Region->thread() == nullptr);

    //Preform copy
    region->setData(Region);
}

/**
 * @brief cwRegionIOTask::version
 * @return Returns the current version
 */
int cwRegionIOTask::protoVersion()
{
    //Version 9 fixes proto typos (backCompasssCalibration -> backCompassCalibration,
    //NoteTranformation -> NoteTransformation, Depercated -> Deprecated)
    return 9;
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
        {7, "2025.3-dev"},
        {8, "2026-dev"},
        {9, "2026.4"}
    };

    return protoToVersionString.value(protoVersion, "Unknown Version");
}

bool cwRegionIOTask::isVersionSupported(int fileVersion)
{
    return fileVersion <= protoVersion();
}

QString cwRegionIOTask::newerVersionWarning(const QString &subject, int fileVersion,
                                            const QString &consequence)
{
    return QStringLiteral("%1 was created by a newer version of CaveWhere "
                          "(v%2, file version %3). This copy only supports file version %4. %5")
        .arg(subject,
             toVersion(fileVersion),
             QString::number(fileVersion),
             QString::number(protoVersion()),
             consequence);
}
