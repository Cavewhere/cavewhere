//Our includes
#include "cwFutureManagerModel.h"
#include "asyncfuture.h"

//Qt includes
#include <QFutureWatcher>

//Std includes
#include <algorithm>

cwFutureManagerModel::cwFutureManagerModel(QObject *parent) :
    QAbstractListModel(parent),
    Timer(new QTimer(this))
{
    Timer->setInterval(250);
    connect(Timer, &QTimer::timeout, this, [this](){
        if(rowCount() > 0) {
            emit dataChanged(index(0), index(rowCount() - 1), {RunTimeRole});
        }
    });
}

void cwFutureManagerModel::addJob(const cwFuture &job)
{
    Q_ASSERT(QThread::currentThread() == thread());

    if(job.future().isCanceled() || job.future().isFinished()) {
        return;
    }

    auto watcher = new QFutureWatcher<void>(this);

    WatcherContainer container;
    container.watcher = watcher;
    container.job = job;
    container.startTime.start();

    auto remove = [this, watcher]() {
        removeWatcher(watcher);
    };

    connect(watcher, &QFutureWatcher<void>::finished,
            this, remove);

    connect(watcher, &QFutureWatcher<void>::canceled,
            this, remove);

    connect(watcher, &QFutureWatcher<void>::progressValueChanged,
            this, [this, watcher](){
        auto modelIndex = indexOf(watcher);
        emit dataChanged(modelIndex, modelIndex, {ProgressRole});

    });

    connect(watcher, &QFutureWatcher<void>::progressRangeChanged,
            this, [this, watcher](int min, int max){
        Q_UNUSED(min);
        Q_UNUSED(max);
        auto modelIndex = indexOf(watcher);
        emit dataChanged(modelIndex, modelIndex, {NumberOfStepRole});
    });

    watcher->setFuture(job.future());

    int lastRow = rowCount();
    beginInsertRows(QModelIndex(), lastRow, lastRow);
    Watchers.append(container);
    endInsertRows();

    if(!Timer->isActive()) {
        Timer->start();
    }
}

int cwFutureManagerModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return Watchers.size();
}

QVariant cwFutureManagerModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    auto& watcher = Watchers.at(index.row());

    switch(role) {
    case cwFutureManagerModel::NameRole:
        return watcher.job.name();
    case cwFutureManagerModel::NumberOfStepRole:
        return watcher.watcher->progressMaximum();
    case cwFutureManagerModel::ProgressRole:
        return watcher.watcher->progressValue();
    case cwFutureManagerModel::RunTimeRole:
        return watcher.startTime.elapsed();
    default:
        break;
    }

    return QVariant();
}

QHash<int, QByteArray> cwFutureManagerModel::roleNames() const
{
    return defaultRoles();
}

void cwFutureManagerModel::waitForFinished()
{
    while(!Watchers.isEmpty()) {
        auto watcher = Watchers.first();
        AsyncFuture::waitForFinished(watcher.job.future());
        qDebug() << "Job:" << watcher.job.name() << watcher.job.future().isFinished() << watcher.job.future().isRunning() << watcher.job.future().isCanceled();
        Q_ASSERT(!watcher.job.future().isRunning());
        if(!Watchers.isEmpty()) {
            if(watcher.watcher == Watchers.first().watcher) {
                removeWatcher(watcher.watcher);
            }
        }
    }
}

QHash<int, QByteArray> cwFutureManagerModel::defaultRoles()
{
    static QHash<int, QByteArray> roles = {
        {NameRole, "nameRole"},
        {ProgressRole, "progressRole"},
        {NumberOfStepRole, "numberOfStepsRole"},
        {RunTimeRole, "runTimeRole"}
    };
    return roles;
}

void cwFutureManagerModel::removeWatcher(QFutureWatcher<void> *watcher)
{
    QModelIndex modelIndex = indexOf(watcher);
    Q_ASSERT(modelIndex.isValid());

    beginRemoveRows(QModelIndex(), modelIndex.row(), modelIndex.row());
    Watchers.removeAt(modelIndex.row());
    endRemoveRows();

    if(rowCount() == 0) {
        Timer->stop();
    }

    delete watcher;
}

QModelIndex cwFutureManagerModel::indexOf(const QFutureWatcher<void> *watcher) const
{
    auto predict = [watcher](const cwFutureManagerModel::WatcherContainer &container){
        return container.watcher == watcher;
    };

    auto iter = std::find_if(Watchers.begin(), Watchers.end(), predict);
    return index(std::distance(Watchers.begin(), iter));
}

void cwFutureManagerModel::setInterval(int interval) {
    if(Timer->interval() != interval) {
        Timer->stop();
        Timer->start(interval);
        emit intervalChanged();
    }
}
