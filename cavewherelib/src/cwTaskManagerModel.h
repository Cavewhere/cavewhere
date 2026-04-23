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
#include <QQmlEngine>

//Our includes
class cwTask;
#include "cwFutureManagerModel.h"
#include "cwGlobals.h"

/**
 * @brief The cwTaskManagerModel class
 *
 * This class show active task. Tasks that take longer than 2 seconds are shown as
 * active tasks. This class allows the gui to visualize running task, stop, and stop them
 */
class CAVEWHERE_LIB_EXPORT cwTaskManagerModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TaskManagerModel)

public:
    explicit cwTaskManagerModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    void addTask(cwTask* task);
    void removeTask(cwTask* task);

    void waitForTasks();
    bool isIdle() const;

signals:
    void becameIdle();

public slots:

private:
    QHash<cwTask*, QTimer*> TaskToTimer;
    QList<cwTask*> RunningTasks;
    QSet<cwTask*> WatchingTasks;

    void removeActiveTask(cwTask* task);
    void addActiveTask(cwTask* task);
    void updateTask(cwTask* task, cwFutureManagerModel::Roles role);

    void taskHasStarted(cwTask* task);
    void taskHasStopped(cwTask* task);
    void taskHasFinished(cwTask* task);
    void taskEnded(cwTask* task);   // shared body for stopped and finished
    void taskIsActive(cwTask* task);
    void updateTaskProgress(cwTask* task);
    void updateTaskNumberOfSteps(cwTask* task);
    void updateTaskName(cwTask* task);
};

#endif // CWTASKMANAGERMODEL_H
