//Our includes
#include "cwTask.h"

//Qt includes
#include <QMutexLocker>
#include <QReadLocker>
#include <QWriteLocker>
#include <QReadWriteLock>
#include <QApplication>
#include <QThread>
#include <QDebug>

cwTask::cwTask(QObject *parent) :
    QObject(parent)
{
    NumberOfSteps = 0;
    CurrentStatus = Stopped;
    ParentTask = NULL;

}

/**
  \brief This set's the parent task for this task

  This allows task build up and also.  This makes isRunning work propertly

  Does nothing if this is already running
*/
void cwTask::setParentTask(cwTask* parentTask) {
    if(status() != Stopped) {
        qWarning("Can't set task's parent, when the task is running!");
        return;
    }

    //Make sure we're updating the parent task
    if(parentTask == ParentTask) { return; }

    //See if we had a parent task
    if(ParentTask != NULL) {
        //Remove this task from the parent task
        ParentTask->ChildTasks.removeOne(this);
    }

    //Update the thread we're going to be running on
    setThread(parentTask->thread());

    //Reparent this task to parentTask
    setParent(parentTask); //For memory management and prevent stall pointers
    ParentTask = parentTask;
    ParentTask->ChildTasks.append(this);
}

/**
  \brief Sets the thread that this task will run on

  This will move the object to that thread immedately
  */
void cwTask::setThread(QThread* threadToRunOn) {
    threadToRunOn->start();
    QMetaObject::invokeMethod(this, "changeThreads", Qt::AutoConnection, QGenericArgument("QThread*", &threadToRunOn));
}

/**
  \brief Gets the number of steps for a task

  This function is thread safe
  */
int cwTask::numberOfSteps() const {
    QReadLocker locker(const_cast<QReadWriteLock*>(&NumberOfStepsLocker));
    return NumberOfSteps;
}

/**
  \brief Gets the status of the task
  */
cwTask::Status cwTask::status() const {
    QReadLocker locker(const_cast<QReadWriteLock*>(&StatusLocker));
    return CurrentStatus;
}

/**
  \brief Tests to see if the task is still running

  This function is thread safe
  */
bool cwTask::isRunning() const {
    return status() != Stopped;
}

/**
  \brief Stops the task as soon as possible

  This function is thread safe
  */
void cwTask::stop() {
    QWriteLocker locker(&StatusLocker);
    CurrentStatus = Stopped;

    //Go through all children and stop them
    foreach(cwTask* child, ChildTasks) {
        child->stop();
    }
}

/**
  \brief Starts the task to run.

  This will start the task implemented in the base class runTask.

  setThread() runs the task on a new therad. The object will be move to that thread, when this
  function is called. If no thread is set, then the task will run on the
  current thread. Once the task is complete, the task is moved back to it's original thread.
  If the thread is already assigned to another thread, or started is called from a thread
  that this object currently running on, then the task willn't bee moved.  The task, also
  can't be moved if it's currently running.

If the user calls stop() or the task stops, then this function, will emit
  stopped() and the task should stop on it's next step.

  If the task is completed, the task will be move to it's original thread, and finished will be
emitted. Moving to the original thread, just changes which thread the slots of this class and
subclasses run on.

If the task is already running or preparing to start, then this function does nothing

This function is thread safe
  */
void cwTask::start() {
    if(status() != Stopped) {
        return;
    }

    {
        //Make sure we are preparing to start
        QWriteLocker locker(&StatusLocker);
        CurrentStatus = PreparingToStart;
    }

//    if(RunThread != NULL) {
//        if(this->thread() == QThread::currentThread() || isRunning()) {
//            //Make sure the thread has an event loop
//            RunThread->start();

//            //Move this object to another thread
//            OriginalThread = thread();
//            moveToThread(RunThread);
//        }
//    } else {
//        OriginalThread = NULL;
//    }

    //Make sure the thread has an event loop running

    //Start the task, by calling startOnCurrentThread, this is queue on Qt event loop
    //This is an asycranous call
    QMetaObject::invokeMethod(this, "startOnCurrentThread", Qt::AutoConnection);
}

/**
  \brief The number of steps for the task

  This function is thread safe
  */
void cwTask::setNumberOfSteps(int steps) {
    if(steps != numberOfSteps()) {
        QWriteLocker locker(&NumberOfStepsLocker);
        NumberOfSteps = steps;
        emit numberOfStepsChanged(steps);
    }
}

/**
  \brief This function should be called when the task has completed

  This will emit the finished or stopped signal and also handle
  moving the object back it's original thread
  */
void cwTask::done() {
    //Moves the task back to the original thread -- WARNING SLOTS WILL CALLED BY ORIGINAL THREAD
//    if(OriginalThread != NULL) {
//        moveToThread(OriginalThread);
//        OriginalThread = NULL;
//    }

    //If the task is still running, this means that the task has finished, without error
    if(status() != Stopped) {
        {
            stop();
        }
        emit finished();
    } else {
        //Else the task has been stopped early
        emit stopped();
    }
}

/**
  \brief Starts running the task on the current thread

  THIS should only be called from start()
  */
void cwTask::startOnCurrentThread() {
    Status currentStatus = status();
    Q_ASSERT(currentStatus != Running); //The thread should definitally not me running here
    if(currentStatus == Stopped) {
        emit stopped();
        return; //This has stopped
    }

    {
        //Make sure we are preparing to start
        QWriteLocker locker(&StatusLocker);
        CurrentStatus = Running;
    }

    //Run the task
    emit started();
    runTask();
}

/**
  \brief Moves the task to the thread

  This moves this task and all it children to the thread
  */
void cwTask::changeThreads(QThread* thread) {
    moveToThread(thread);
}
