#ifndef CWTASK_H
#define CWTASK_H

#include <QMutex>
#include <QObject>

/**
  \brief A member functions in the class are thread safe
  */

class cwTask : public QObject
{
    Q_OBJECT
public:
    explicit cwTask(QObject *parent = 0);

    int numberOfSteps() const;
    bool isRunning() const;

    void stop();

public slots:
    void start(QThread* threadToRunOn = NULL);

signals:
    void started();
    void finished();
    void stopped();
    void progessed(int step);
    void numberOfStepsChanged(int numberOfSteps);

protected:
    void setNumberOfSteps(int steps);
    virtual void runTask() = 0;

protected:

private:
    QMutex NumberOfStepsMutex;
    QMutex RunningMutex;

    int NumberOfSteps;
    bool Running;

    QThread* OriginalThread;

private slots:
    void startCurrentThread();

};

#endif // CWTASK_H
