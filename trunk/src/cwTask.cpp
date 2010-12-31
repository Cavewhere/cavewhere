//Our includes
#include "cwTask.h"

//Qt includes
#include <QMutexLocker>
#include <QReadLocker>
#include <QWriteLocker>
#include <QReadWriteLock>
#include <QApplication>

cwTask::cwTask(QObject *parent) :
    QObject(parent)
{
    NumberOfSteps = 0;
    Running = false;
    OriginalThread = NULL;

}

/**
  \brief Gets the number of steps for a task

  This function is thread safe
  */
int cwTask::numberOfSteps() const {
    QReadLocker locker(NumberOfStepsMutex);
    return NumberOfSteps;
}

/**
  \brief Tests to see if the task is still running

  This function is thread safe
  */
bool cwTask::isRunning() const {
    QReadLocker locker(RunningMutex);
    return Running;
}

/**
  \brief Stops the task as soon as possible

  This function is thread safe
  */
void cwTask::stop() {
    QWriteLocker locker(RunningMutex);
    running = false;
}

/**
  \brief Starts the task to run.

  This will start the task implemented in the base class runTask.

  threadToRunOn, is the thread
  that this object will be moved to. If threadToRunOn is NULL, then the task will run on the
  current thread. Once the task is complete, the task is moved back to it's original thread.

If the user calls stop() or the task stops, then this function, will emit
  stopped() and the task should stop on it's next step.

  If the task is completed, the task will be move to it's original thread, and finished will be
emitted. Moving to the original thread, just changes which thread the slots of this class and
subclasses run on.

This function is thread safe
  */
void cwTask::start(QThread* threadToRunOn) {
    if(threadToRunOn != NULL) {
        //Move this object to another thread
        OriginalThread = thread();
        moveToThread(threadToRunOn);
    } else {
        OriginalThread = NULL;
    }

    //Make sure the thread has an event loop running

    //Start the task, by calling startOnCurrentThread, this is queue on Qt event loop
    //This is an asycranous call
    QMetaObject::invokeMethod(this, "startOnCurrentThread", Qt::QueuedConnection);
}

/**
  \brief The number of steps for the task

  This function is thread safe
  */
void cwTask::setNumberOfSteps(int steps) {
    if(steps != numberOfSteps()) {
        QWriteLocker locker(NumberOfStepsMutex);
        NumberOfSteps = steps;
        emit numberOfStepsChanged();
    }
}

/**
  \brief Starts running the task on the current thread

  THIS should only be called from start()
  */
Q_INVOKABLE void cwTask::startOnCurrentThread() {
    {
        //Make sure we are running now
        QWriteLocker locker(RunningMutex);
        Running = true;
    }

    //Run the task
    emit started();
    runTask();

    //Moves the task back to the original thread -- WARNING SLOTS WILL CALLED BY ORIGINAL THREAD
    if(OriginalThread != NULL) {
        moveToThread(OriginalThread);
    }

    //If the task is still running, this means that the task has finished, without error
    if(isRunning()) {
        {
            QWriteLocker locker(RunningMutex);
            Running = false;
        }
        emit finished();
    } else {
        //Else the task has been stopped early
        emit stopped();
    }
}
