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
    Region = new cwCavingRegion(this);
}


/**
      This does a deap copy of the region, so it make a snapshot
      */
void cwRegionIOTask::setCavingRegion(const cwCavingRegion& region) {
    Q_ASSERT(thread() == Region->thread());

    //Move the region to the current thread
    if(QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "moveRegionToThread",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(QThread*, QThread::currentThread()));
    }

    *Region = region;

    //Move Region back to task's thread
    if(QThread::currentThread() != thread()) {
        Region->moveToThread(thread());
        Region->setParent(this);
    }
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
    Q_ASSERT(thread() == Region->thread());

    //Move the region to the current thread
    if(QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "moveRegionToThread",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(QThread*, QThread::currentThread()));
    }

    //Preform copy
    region = *Region;

    //Move Region back to task's thread
    if(QThread::currentThread() != thread()) {
        Region->moveToThread(thread());
        QMetaObject::invokeMethod(this, "updateRegionParent",
                                  Qt::BlockingQueuedConnection);
    }
}

/**
 * @brief cwRegionIOTask::version
 * @return Returns the current version
 */
int cwRegionIOTask::version()
{
    return 1;
}

/**
 * @brief cwRegionIOTask::moveRegionToThread
 * @param thread
 *
 * This moves Region to thread. We need this function because, objects can only be pushed to other
 * threads, and can't be pulled to threads. See Qt documentation for details.
 */
void cwRegionIOTask::moveRegionToThread(QThread* thread)
{
    Region->setParent(nullptr);
    Region->moveToThread(thread);
}

/**
 * @brief cwRegionIOTask::updateRegionParent
 */
void cwRegionIOTask::updateRegionParent()
{
    Region->setParent(this);
}

