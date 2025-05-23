/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTASK_H
#define CWTASK_H

//Qt includes
#include <QMutex>
#include <QObject>
#include <QReadWriteLock>
#include <QTimer>
#include <QRunnable>
#include <QWaitCondition>
#include <QFuture>

//Our includes
#include "cwGlobals.h"

/**
  \brief A member functions in the class are thread safe
  */

class CAVEWHERE_LIB_EXPORT cwTask : public QObject, public QRunnable
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(int numberOfSteps READ numberOfSteps NOTIFY numberOfStepsChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)

public:
    enum Status {
        Ready,
        PreparingToStart,
        Running,
        Stopped,
        Restart,
        Restarting, //Same as ready
    };

    enum WaitToFinishType {
        IgnoreRestart,
        WaitForRestart
    };

    Q_ENUM(Status)

    explicit cwTask(QObject *parent = 0);

    void setParentTask(cwTask* parentTask);

    void setUsingThreadPool(bool enabled);
    bool isUsingThreadPool() const;

    int numberOfSteps() const;
    int progress() const;

    Status status() const;
    bool isRunning() const;
    bool isReady() const;

    QString name() const;
    void setName(QString name);

    void run();

    void waitToFinish(WaitToFinishType type = WaitForRestart);

    void addFuture(QFuture<void> future);
    void addFutures(QVector<QFuture<void>> futures);

    static int maxThreadCount();
    static QThreadPool* threadPool() { return ThreadPool; }
    static void initilizeThreadPool();

public slots:
    void start();
    void restart();
    void stop();

signals:
    void started();
    void finished();
    void stopped();
    void preparingToStart();
//    void progressed(int step);
    void progressChanged();
    void statusMessage(QString message);
    void numberOfStepsChanged(int numberOfSteps);
    void shouldRerun();
    void nameChanged();
    void threadChanged();

protected:
    void setNumberOfSteps(int steps);
    void setProgress(int progress);
    virtual void runTask() = 0;

protected slots:
    void done();

private:
    mutable QReadWriteLock NumberOfStepsLocker;
    mutable QReadWriteLock StatusLocker;
    mutable QReadWriteLock ProgressLocker;
    mutable QReadWriteLock NameLocker;
    QMutex WaitToFinishLocker;

    int NumberOfSteps;
    int Progress = 0; //!<

    Status CurrentStatus;
    bool NeedsRestart;

    QList<cwTask*> ChildTasks;
    cwTask* ParentTask;

    QString Name; //!< The name of the task

    bool UsingThreadPool;
    static QThreadPool* ThreadPool;

    void privateStop();
    bool isParentsRunning();

    bool needsRestart() const;
    bool isReadyPrivate() const;

private:

};





#endif // CWTASK_H
