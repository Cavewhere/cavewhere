#ifndef CWTASK_H
#define CWTASK_H

#include <QMutex>
#include <QObject>
#include <QReadWriteLock>

/**
  \brief A member functions in the class are thread safe
  */

class cwTask : public QObject
{
    Q_OBJECT
public:
    enum Status {
        PreparingToStart,
        Running,
        Stopped
    };

    explicit cwTask(QObject *parent = 0);

    void setParentTask(cwTask* parentTask);
    void setThread(QThread* threadToRunOn);

    int numberOfSteps() const;
    Status status() const;
    bool isRunning() const;

    void stop();

public slots:
    void start();

signals:
    void started();
    void finished();
    void stopped();
    void progressed(int step);
    void numberOfStepsChanged(int numberOfSteps);

protected:
    void setNumberOfSteps(int steps);
    virtual void runTask() = 0;

protected slots:
    void done();

private:
    QReadWriteLock NumberOfStepsLocker;
    QReadWriteLock StatusLocker;

    int NumberOfSteps;
    Status CurrentStatus;
    //bool Running;

    QList<cwTask*> ChildTasks;
    cwTask* ParentTask;

    //QThread* RunThread; //The thread that this will run on
    //QThread* OriginalThread; //Usually main thread

private:
    Q_INVOKABLE void startOnCurrentThread();
    Q_INVOKABLE void changeThreads(QThread* thread);

};

#endif // CWTASK_H
