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
    QAbstractListModel(parent)
{
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

        connect(task, &cwTask::preparingToStart, this, [this, task]() { taskHasStarted(task); });
        connect(task, &cwTask::stopped,          this, [this, task]() { taskHasStopped(task); });
        connect(task, &cwTask::finished,         this, [this, task]() { taskHasFinished(task); });
        connect(task, &cwTask::progressChanged,  this, [this, task]() { updateTaskProgress(task); });
        connect(task, &cwTask::numberOfStepsChanged, this, [this, task](int) { updateTaskNumberOfSteps(task); });
        connect(task, &cwTask::nameChanged,      this, [this, task]() { updateTaskName(task); });
        connect(timer, &QTimer::timeout,         this, [this, task]() { taskIsActive(task); });
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

        disconnect(task, nullptr, this, nullptr);
        disconnect(timer, nullptr, this, nullptr);
        timer->stop();

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

bool cwTaskManagerModel::isIdle() const
{
    return std::none_of(std::as_const(WatchingTasks).begin(),
                        std::as_const(WatchingTasks).end(),
                        [](const cwTask* task) { return task->isRunning(); });
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

void cwTaskManagerModel::taskHasStarted(cwTask* task)
{
    Q_ASSERT(TaskToTimer.contains(task));
    TaskToTimer.value(task)->start();
}

void cwTaskManagerModel::taskHasStopped(cwTask* task)
{
    taskEnded(task);
}

void cwTaskManagerModel::taskHasFinished(cwTask* task)
{
    taskEnded(task);
}

void cwTaskManagerModel::taskEnded(cwTask* task)
{
    Q_ASSERT(TaskToTimer.contains(task));
    TaskToTimer.value(task)->stop();
    removeActiveTask(task);
    if (isIdle()) {
        emit becameIdle();
    }
}

void cwTaskManagerModel::taskIsActive(cwTask* task)
{
    addActiveTask(task);
}

void cwTaskManagerModel::updateTaskProgress(cwTask* task)
{
    updateTask(task, cwFutureManagerModel::ProgressRole);
}

void cwTaskManagerModel::updateTaskNumberOfSteps(cwTask* task)
{
    updateTask(task, cwFutureManagerModel::NumberOfStepRole);
}

void cwTaskManagerModel::updateTaskName(cwTask* task)
{
    updateTask(task, cwFutureManagerModel::NameRole);
}

void cwTaskManagerModel::updateTask(cwTask* task, cwFutureManagerModel::Roles role)
{
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
