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
        Ready,
        PreparingToStart,
        Running,
        Stopped,
        Restart
    };

    explicit cwTask(QObject *parent = 0);

    void setParentTask(cwTask* parentTask);
    void setThread(QThread* threadToRunOn, Qt::ConnectionType connectionType = Qt::AutoConnection);

    int numberOfSteps() const;
    Status status() const;
    bool isRunning() const;
    bool isReady() const;

    //Do not move this to a slot!!! You will break things
    void stop();

public slots:
    void start();
    void restart();

signals:
    void started();
    void finished();
    void stopped();
    void progressed(int step);
    void statusMessage(QString message);
    void numberOfStepsChanged(int numberOfSteps);
    void shouldRerun();

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

    QList<cwTask*> ChildTasks;
    cwTask* ParentTask;

    void privateStop();
    bool isParentsRunning();

private:
    Q_INVOKABLE void startOnCurrentThread();
    Q_INVOKABLE void changeThreads(QThread* thread);

};

/**
  Returns if the task is ready
  */
inline bool cwTask::isReady() const {
    return status() == Ready;
}

#endif // CWTASK_H
