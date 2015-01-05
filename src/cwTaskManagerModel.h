/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWTASKMANAGERMODEL_H
#define CWTASKMANAGERMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QTimer>
#include <QSet>
#include <QHash>
#include <QList>
#include <QSignalMapper>

//Our includes
class cwTask;

/**
 * @brief The cwTaskManagerModel class
 *
 * This class show active task. Tasks that take longer than 2 seconds are shown as
 * active tasks. This class allows the gui to visualize running task, stop, and stop them
 */
class cwTaskManagerModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole,
        ProgressRole,
        NumberOfStepRole
    };

    explicit cwTaskManagerModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    void addTask(cwTask* task);
    void removeTask(cwTask* task);

signals:

public slots:

private:
    QHash<cwTask*, QTimer*> TaskToTimer;
    QList<cwTask*> RunningTasks;
    QSet<cwTask*> WatchingTasks;

    QSignalMapper* TaskStartedMapper;
    QSignalMapper* TaskStoppedMapper;
    QSignalMapper* TaskFinishedMapper;
    QSignalMapper* TaskActiveMapper;
    QSignalMapper* TaskNameMapper;
    QSignalMapper* TaskProgressMapper;
    QSignalMapper* TaskNumberOfStepsMapper;

    cwTask* convertToTask(QObject* task);
    void removeActiveTask(cwTask* task);
    void addActiveTask(cwTask* task);

    void updateTask(QObject* taskObject, Roles role);
private slots:
    void taskDeleted(QObject* taskObject);
    void taskHasStarted(QObject* taskObject);
    void taskHasStopped(QObject* taskObject);
    void taskHasFinished(QObject* taskObject);
    void taskIsActive(QObject* taskObject);
    void updateTaskProgress(QObject* taskObject);
    void updateTaskNumberOfSteps(QObject* taskObject);
    void updateTaskName(QObject* taskObject);
};

#endif // CWTASKMANAGERMODEL_H
