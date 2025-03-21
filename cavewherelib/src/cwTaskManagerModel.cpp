/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwTaskManagerModel.h"

//Qt includes
#include "cwTask.h"
#include "cwDebug.h"

cwTaskManagerModel::cwTaskManagerModel(QObject *parent) :
    QAbstractListModel(parent),
    TaskStartedMapper(new QSignalMapper(this)),
    TaskStoppedMapper(new QSignalMapper(this)),
    TaskFinishedMapper(new QSignalMapper(this)),
    TaskActiveMapper(new QSignalMapper(this)),
    TaskNameMapper(new QSignalMapper(this)),
    TaskProgressMapper(new QSignalMapper(this)),
    TaskNumberOfStepsMapper(new QSignalMapper(this))
{
    connect(TaskStartedMapper, SIGNAL(mappedObject(QObject*)), this, SLOT(taskHasStarted(QObject*)));
    connect(TaskStoppedMapper, SIGNAL(mappedObject(QObject*)), this, SLOT(taskHasStopped(QObject*)));
    connect(TaskFinishedMapper, SIGNAL(mappedObject(QObject*)), this, SLOT(taskHasFinished(QObject*)));
    connect(TaskActiveMapper, SIGNAL(mappedObject(QObject*)), this, SLOT(taskIsActive(QObject*)));
    connect(TaskNameMapper, SIGNAL(mappedObject(QObject*)), this, SLOT(updateTaskName(QObject*)));
    connect(TaskProgressMapper, SIGNAL(mappedObject(QObject*)), this, SLOT(updateTaskProgress(QObject*)));
    connect(TaskNumberOfStepsMapper, SIGNAL(mappedObject(QObject*)),
            this, SLOT(updateTaskNumberOfSteps(QObject*)));
}

/**
 * @brief cwTaskManagerModel::rowCount
 * @param parent
 * @return The number of running tasks
 */
int cwTaskManagerModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return RunningTasks.size();
}

/**
 * @brief cwTaskManagerModel::data
 * @param index
 * @param role
 * @return
 */
QVariant cwTaskManagerModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index);

    if(!index.isValid()) {
        return QVariant();
    }

    cwTask* task = RunningTasks.at(index.row());

    switch(role) {
    case cwFutureManagerModel::NameRole:
        return task->name();
    case cwFutureManagerModel::NumberOfStepRole:
        return task->numberOfSteps();
    case cwFutureManagerModel::ProgressRole:
        return task->progress();
    default:
        break;
    }

    return QVariant();
}

/**
 * @brief cwTaskManagerModel::roleNames
 * @return
 */
QHash<int, QByteArray> cwTaskManagerModel::roleNames() const
{
    return cwFutureManagerModel::defaultRoles();
}

/**
 * @brief cwTaskManagerModel::addTask
 * @param task
 */
void cwTaskManagerModel::addTask(cwTask *task)
{
    if(!WatchingTasks.contains(task)) {
        WatchingTasks.insert(task);

        QTimer* timer = new QTimer(this);
        timer->setInterval(250);

        TaskToTimer.insert(task, timer);

        connect(task, &cwTask::destroyed, this, &cwTaskManagerModel::taskDeleted);
        connect(task, SIGNAL(preparingToStart()), TaskStartedMapper, SLOT(map()));
        connect(task, SIGNAL(stopped()), TaskStoppedMapper, SLOT(map()));
        connect(task, SIGNAL(finished()), TaskFinishedMapper, SLOT(map()));
        connect(task, SIGNAL(progressChanged()), TaskProgressMapper, SLOT(map()));
        connect(task, SIGNAL(numberOfStepsChanged(int)), TaskNumberOfStepsMapper, SLOT(map()));
        connect(task, SIGNAL(nameChanged()), TaskNameMapper, SLOT(map()));
        connect(timer, SIGNAL(timeout()), TaskActiveMapper, SLOT(map()));

        TaskStartedMapper->setMapping(task, task);
        TaskStoppedMapper->setMapping(task, task);
        TaskFinishedMapper->setMapping(task, task);
        TaskProgressMapper->setMapping(task, task);
        TaskNumberOfStepsMapper->setMapping(task, task);
        TaskNameMapper->setMapping(task, task);
        TaskActiveMapper->setMapping(timer, task);
    }
}

/**
 * @brief cwTaskManagerModel::removeTask
 * @param task
 */
void cwTaskManagerModel::removeTask(cwTask *task)
{
    if(WatchingTasks.contains(task)) {
        Q_ASSERT(TaskToTimer.contains(task));
        QTimer* timer = TaskToTimer.value(task);

        connect(task, nullptr, this, nullptr);
        connect(task, nullptr, TaskStartedMapper, nullptr);
        connect(task, nullptr, TaskStoppedMapper, nullptr);
        connect(task, nullptr, TaskFinishedMapper, nullptr);
        connect(task, nullptr, TaskProgressMapper, nullptr);
        connect(task, nullptr, TaskNumberOfStepsMapper,nullptr);
        connect(task, nullptr, TaskNameMapper, nullptr);
        connect(timer, nullptr, TaskActiveMapper, nullptr);

        TaskStartedMapper->removeMappings(task);
        TaskStoppedMapper->removeMappings(task);
        TaskFinishedMapper->removeMappings(task);
        TaskProgressMapper->removeMappings(task);
        TaskNumberOfStepsMapper->removeMappings(task);
        TaskNameMapper->removeMappings(task);
        TaskActiveMapper->removeMappings(timer);

        WatchingTasks.remove(task);
        TaskToTimer.remove(task);
        timer->deleteLater();
    }
}

void cwTaskManagerModel::waitForTasks()
{
    for(auto task : WatchingTasks) {
        task->waitToFinish();
    }
}

/**
 * @brief cwTaskManagerModel::convertToTask
 * @param task
 * @return Return's task as a cwTask, if task isn't a cwTask, this assert's
 */
cwTask *cwTaskManagerModel::convertToTask(QObject *task)
{
    Q_ASSERT(dynamic_cast<cwTask*>(task) != nullptr);
    return static_cast<cwTask*>(task);
}

/**
 * @brief cwTaskManagerModel::removeActiveTask
 * @param task
 */
void cwTaskManagerModel::removeActiveTask(cwTask *task)
{
    int index = RunningTasks.indexOf(task);
    if(index >= 0) {
        beginRemoveRows(QModelIndex(), index, index);
        RunningTasks.removeAt(index);
        endRemoveRows();
    }
}

/**
 * @brief cwTaskManagerModel::addActiveTask
 * @param task
 */
void cwTaskManagerModel::addActiveTask(cwTask *task)
{
    if(RunningTasks.contains(task)) {
        return;
    }

    QTimer* timer = TaskToTimer.value(task);
    timer->stop();

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    RunningTasks.append(task);
    endInsertRows();
}

/**
 * @brief cwTaskManagerModel::taskDeleted
 * @param task
 */
void cwTaskManagerModel::taskDeleted(QObject *taskObject)
{
    Q_UNUSED(taskObject);
}

/**
 * @brief cwTaskManagerModel::taskHasStarted
 * @param taskObject
 */
void cwTaskManagerModel::taskHasStarted(QObject *taskObject)
{
    cwTask* task = convertToTask(taskObject);

    Q_ASSERT(TaskToTimer.contains(task));

    QTimer* timer = TaskToTimer.value(task);
    timer->start();
}

/**
 * @brief cwTaskManagerModel::taskHasStopped
 * @param taskObject
 */
void cwTaskManagerModel::taskHasStopped(QObject *taskObject)
{
    cwTask* task = convertToTask(taskObject);

    Q_ASSERT(TaskToTimer.contains(task));

    QTimer* timer = TaskToTimer.value(task);
    timer->stop();

    removeActiveTask(task);
}

/**
 * @brief cwTaskManagerModel::taskHasFinished
 * @param taskObject
 */
void cwTaskManagerModel::taskHasFinished(QObject *taskObject)
{
    cwTask* task = convertToTask(taskObject);

    Q_ASSERT(TaskToTimer.contains(task));

    QTimer* timer = TaskToTimer.value(task);
    timer->stop();

    removeActiveTask(task);
}

/**
 * @brief cwTaskManagerModel::taskIsActive
 * @param taskObject
 */
void cwTaskManagerModel::taskIsActive(QObject *taskObject)
{
    cwTask* task = convertToTask(taskObject);
    addActiveTask(task);
}

/**
 * @brief cwTaskManagerModel::updateTaskProgress
 * @param taskObject
 *
 * This udpates the model with the new progress
 */
void cwTaskManagerModel::updateTaskProgress(QObject *taskObject)
{
    updateTask(taskObject, cwFutureManagerModel::ProgressRole);
}

/**
 * @brief cwTaskManagerModel::updateTaskNumberOfSteps
 * @param taskObject
 *
 * This udpates the model with new number of steps
 */
void cwTaskManagerModel::updateTaskNumberOfSteps(QObject *taskObject)
{
    updateTask(taskObject, cwFutureManagerModel::NumberOfStepRole);
}

/**
 * @brief cwTaskManagerModel::updateTaskName
 * @param taskObject
 *
 * This updates the model with new name of task Object
 */
void cwTaskManagerModel::updateTaskName(QObject *taskObject)
{
    updateTask(taskObject, cwFutureManagerModel::NameRole);
}

/**
 * @brief cwTaskManagerModel::updateTask
 * @param taskObject
 * @param role
 *
 * This is a helper function to signal that the model's data has been updated
 */
void cwTaskManagerModel::updateTask(QObject *taskObject, cwFutureManagerModel::Roles role)
{
    cwTask* task = convertToTask(taskObject);
    int index = RunningTasks.indexOf(task);
    if(index > -1) {
        QVector<int> roleChanged;
        roleChanged.append(role);

        QModelIndex modelIndex = this->index(index);
        //        qDebug() << "Data changed:" << index << role;
        if(modelIndex.isValid()) {
            emit dataChanged(modelIndex, modelIndex, roleChanged);
        }
    }
}
