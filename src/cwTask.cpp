/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTask.h"
#include "cwDebug.h"

//Qt includes
#include <QMutexLocker>
#include <QReadLocker>
#include <QWriteLocker>
#include <QReadWriteLock>
#include <QApplication>
#include <QThread>
#include <QThreadPool>
#include <QDebug>

cwTask::cwTask(QObject *parent) :
    QObject(parent),
    StatusLocker(QReadWriteLock::Recursive)
{
    NumberOfSteps = 0;
    CurrentStatus = Ready;
    ParentTask = nullptr;
    UsingThreadPool = true;

    setAutoDelete(false);
}

/**
  \brief This set's the parent task for this task

  This allows task build up and also.  This makes isRunning work propertly

  Does nothing if this is already running
*/
void cwTask::setParentTask(cwTask* parentTask) {
    if(!isReady()) {
        qWarning("Can't set task's parent, when the task is running!");
        return;
    }

    //Make sure we're updating the parent task
    if(parentTask == ParentTask) { return; }

    //See if we had a parent task
    if(ParentTask != nullptr) {
        //Remove this task from the parent task
        ParentTask->ChildTasks.removeOne(this);
    }

    if(isUsingThreadPool()) {
        setUsingThreadPool(false);
    }

    //Reparent this task to parentTask
    setParent(parentTask); //For memory management and prevent stall pointers
    ParentTask = parentTask;
    ParentTask->ChildTasks.append(this);
}

/**
 * @brief cwTask::setUsingThreadPool
 * @param enabled
 */
void cwTask::setUsingThreadPool(bool enabled)
{
    Q_ASSERT(isReady());
    UsingThreadPool = enabled;

    if(isUsingThreadPool()) {
        foreach(cwTask* childTask, ChildTasks) {
            childTask->setUsingThreadPool(false);
        }
    }
}

/**
 * @brief cwTask::isUsingThreadPool
 * @return
 */
bool cwTask::isUsingThreadPool() const
{
    return UsingThreadPool;
}

/**
  \brief Gets the number of steps for a task

  This function is thread safe
  */
int cwTask::numberOfSteps() const {
    QReadLocker locker(&NumberOfStepsLocker);
    return NumberOfSteps;
}

/**
  \brief Gets the status of the task
  */
cwTask::Status cwTask::status() const {
    QReadLocker locker(&StatusLocker);
    return CurrentStatus;
}

/**
  \brief Tests to see if the task is still running

  This function is thread safe
  */
bool cwTask::isRunning() const {
    QReadLocker locker(&StatusLocker);
    return CurrentStatus == Running || CurrentStatus == PreparingToStart;
}

/**
  \brief Stops the task as soon as possible

  This function is thread safe
  */
void cwTask::stop() {
    QWriteLocker locker(&StatusLocker);
    privateStop();
}

/**
  \brief Starts the task to run.

  This will call runTask() that's implement in the subclass.

  By default cwTask runs a new thread from the QThreadPool. It will not automatically be destroyed.
  If setting useThreadPool() to false will run the task on the current thread.

  If the user calls stop() or the task stops, then this function, will emit
  stopped() and the task should stop on it's next step.

  If the task is already running or preparing to start, then this function does nothing

  This function is thread safe
  */
void cwTask::start() {
    {
        QWriteLocker locker(&StatusLocker);
        if(!isReadyPrivate()) {
            qDebug() << "Can't start the task because it isn't ready, CurrentStatus:" << CurrentStatus << LOCATION;
            return;
        }

        //Make sure we are preparing to start
        CurrentStatus = PreparingToStart;
        emit preparingToStart();
    }

    if(isUsingThreadPool()) {
        QThreadPool::globalInstance()->start(this);
    } else {
        run();
    }
}

/**

  */
void cwTask::restart() {

    StatusLocker.lockForWrite();

    if(!isReadyPrivate()) {
        //Stop the tasks, and it's children
        privateStop();

        CurrentStatus = Restart;
        NeedsRestart = true;
        StatusLocker.unlock();

    } else {
        StatusLocker.unlock();
        start();
    }
}

/**
  \brief The number of steps for the task

  This function is thread safe
  */
void cwTask::setNumberOfSteps(int steps) {
    QWriteLocker locker(&NumberOfStepsLocker);
    if(steps != NumberOfSteps) {
        NumberOfSteps = steps;
        emit numberOfStepsChanged(steps);
    }
}

/**
 * @brief cwTask::setProgress
 * @param progress - Sets the current progress for the task
 *
 * The progress will be clamped between 0 and numberOfSteps()
 */
void cwTask::setProgress(int progress)
{

    Q_ASSERT(progress >= 0);
    Q_ASSERT(progress <= numberOfSteps());

    QReadLocker rlocker(&ProgressLocker);
    if(Progress != progress) {
        rlocker.unlock();

        QWriteLocker locker(&ProgressLocker);
        Progress = progress;
        locker.unlock();

        emit progressChanged();
    }
}

/**
  \brief This function should be called when the task has completed

  This will emit the finished or stopped signal and also handle
  moving the object back it's original thread
  */
void cwTask::done() {
    QWriteLocker locker(&StatusLocker);

    //If the task is still running, this means that the task has finished, without error
    if(CurrentStatus == Restart) {
        CurrentStatus = Restarting;
        locker.unlock();
        emit shouldRerun();
    } else if(CurrentStatus == Stopped) {
        privateStop();
        CurrentStatus = Ready;
        locker.unlock();
        emit stopped();
    } else if(CurrentStatus == Running) {
        CurrentStatus = Ready;
        locker.unlock();
        emit finished();
    }
}

/**
  \brief Starts running the task on the task's thread

  THIS should only be called from start()
  */
void cwTask::startOnCurrentThread() {

    if(!isParentsRunning()) {
        //Parent task aren't running
        stop(); //Stop
        done(); //We are finished
        return;
    }

    {
        QWriteLocker locker(&StatusLocker);

        Q_ASSERT(!isReadyPrivate()); //The thread should definitally not me running here

        if(CurrentStatus != PreparingToStart) {
            done();
            return;
        }

        //Make sure we are preparing to start
        CurrentStatus = Running;

        NeedsRestart = false;
    }

    //Set the progress to zero
    setProgress(0);

    if(isUsingThreadPool()) {
        QThreadPool::globalInstance()->start(this);
    } else {
        run();
    }
}

/**
  \brief Moves the task to the thread

  This moves this task and all it children to the thread
  */
void cwTask::changeThreads(QThread* thread) {
    moveToThread(thread);
    emit threadChanged();
}

/**
  \brief For internal use only

  This isn't thread safe function, make it thread safe by locking StatusLocker, before
  calling this function
  */
void cwTask::privateStop() {
    if(CurrentStatus == Running || CurrentStatus == PreparingToStart) {
        CurrentStatus = Stopped;

        //Go through all children and stop them
        foreach(cwTask* child, ChildTasks) {
            Q_ASSERT(child->thread() == thread()); //Child task must be on the same thread as thier parents
            child->privateStop();
        }
    }
}

/**
  \brief Check to see if the parents are still running

  This return false if the parent task have stopped and returns true if the parent task are still
  running.

  If this task doesn't have a parent, this always returns true
  */
bool cwTask::isParentsRunning() {
    cwTask* currentParentTask = ParentTask;
    while(currentParentTask != nullptr) {
        if(!currentParentTask->isRunning()) {
            return false;
        }
        currentParentTask = currentParentTask->ParentTask;
    }
    return true;
}

bool cwTask::needsRestart() const
{
    QReadLocker locker(&StatusLocker);
    return NeedsRestart;
}

bool cwTask::isReadyPrivate() const
{
    return CurrentStatus == Ready || CurrentStatus == Restarting;
}

/**
* @brief cwTask::setName
* @param name - The new name of the task
*
* The default name of the task is a empty string
*/
void cwTask::setName(QString name) {
    QWriteLocker locker(&NameLocker);
    if(Name != name) {
        Name = name;
        emit nameChanged();
    }
}

/**
 * @brief cwTask::run
 *
 * Runs the task.
 */
void cwTask::run()
{
    if(!isParentsRunning()) {
        //Parent task aren't running
        stop(); //Stop
        done(); //We are finished
        return;
    }

    {
        QWriteLocker locker(&StatusLocker);

        Q_ASSERT(!isReadyPrivate()); //The thread should definitally not me running here

        if(CurrentStatus != PreparingToStart) {
            done();
            return;
        }

        //Make sure we are preparing to start
        CurrentStatus = Running;

        NeedsRestart = false;
    }

    //Set the progress to zero
    setProgress(0);

    //Run the task
    emit started();
    runTask();
}

/**
 * @brief cwTask::waitToFinish
 *
 * This blocks the current thread until the cwTask finishes, ei. emit finished signal
 */
void cwTask::waitToFinish()
{
    if(isUsingThreadPool()) {
        //We need to process the events because there could be events that cause the status to change
        QCoreApplication::processEvents();

        {
            QReadLocker locker(&StatusLocker);
            switch (CurrentStatus) {
            case Ready:
                break;
            case PreparingToStart:
            case Running:
            case Stopped:
            case Restarting:
            case Restart:
                QEventLoop loop;
                QObject::connect(this, &cwTask::finished, &loop, &QEventLoop::quit);
                QObject::connect(this, &cwTask::stopped, &loop, &QEventLoop::quit);
                locker.unlock();
                loop.exec();
                break;
            }
        }

        QCoreApplication::processEvents();
    }
}

/**
* @brief cwTask::name
* @return Return's the name of the task. If the task isn't named this returns the task's class name
*/
QString cwTask::name() const {
    QReadLocker locker(&NameLocker);
    if(Name.isEmpty()) {
        return QString::fromLocal8Bit(metaObject()->className());
    }
    return Name;
}

/**
* @brief cwTask::progress
* @return Returns the progress, this will between 0 and numberOfSteps();
*/
int cwTask::progress() const {
    QReadLocker locker(&ProgressLocker);
    return Progress;
}

/**
  Returns true if the task is ready, otherwise false
  */
bool cwTask::isReady() const {
    QReadLocker locker(&StatusLocker);
    return isReadyPrivate();
}
