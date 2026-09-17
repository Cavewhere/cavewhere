//Our includes
#include "cwFutureManagerModel.h"
#include "asyncfuture.h"

//Qt includes
#include <QFutureWatcher>

//Std includes
#include <algorithm>

namespace {
    //! Nothing can say how far along it is, so there is no honest number to give
    constexpr double kIndeterminateProgress = -1.0;

    //! How often the detail line is recomputed, in milliseconds
    constexpr int kDetailPollMs = 100;

    //! Keeps the oldest active leaf that has lived long enough to be worth naming
    void findOldestLeaf(const cwProgressNodePtr& node, cwProgressNodePtr& oldest)
    {
        if(node->isFinished()) {
            return;
        }

        const auto children = node->activeChildren();
        if(children.isEmpty()) {
            if(node->isLeaf()
                    && node->ageMs() >= cwFutureManagerModel::kDetailMinAgeMs
                    && (!oldest || node->ageMs() > oldest->ageMs())) {
                oldest = node;
            }
            return;
        }

        for(const cwProgressNodePtr& child : children) {
            findOldestLeaf(child, oldest);
        }
    }
}

cwFutureManagerModel::cwFutureManagerModel(QObject *parent) :
    QAbstractListModel(parent),
    Timer(new QTimer(this)),
    DetailTimer(new QTimer(this))
{
    DetailTimer->setInterval(kDetailPollMs);
    connect(DetailTimer, &QTimer::timeout, this, &cwFutureManagerModel::pollDetails);

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
    container.tree = job.tree();
    container.startTime.start();

    auto remove = [this, watcher]() {
        removeWatcher(watcher);
    };

    connect(watcher, &QFutureWatcher<void>::finished,
            this, remove);

    connect(watcher, &QFutureWatcher<void>::canceled,
            this, remove);

    //Progress is republished from here rather than from dataChanged, which the
    //timer above fires four times a second just to refresh the elapsed clock —
    //an unfiltered hookup would announce a number that hasn't moved.
    connect(watcher, &QFutureWatcher<void>::progressValueChanged,
            this, [this, watcher](){
        auto modelIndex = indexOf(watcher);
        emit dataChanged(modelIndex, modelIndex, {ProgressRole});
        emit progressChanged();
    });

    connect(watcher, &QFutureWatcher<void>::progressRangeChanged,
            this, [this, watcher](int min, int max){
        Q_UNUSED(min);
        Q_UNUSED(max);
        auto modelIndex = indexOf(watcher);
        emit dataChanged(modelIndex, modelIndex, {NumberOfStepRole});
        emit progressChanged();
    });

    watcher->setFuture(job.future());

    int lastRow = rowCount();
    beginInsertRows(QModelIndex(), lastRow, lastRow);
    Watchers.append(container);
    endInsertRows();

    emit countChanged();
    emit progressChanged();

    if(!Timer->isActive()) {
        Timer->start();
    }

    updateDetailTimer();
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
    case cwFutureManagerModel::DetailNameRole:
        return watcher.detailName;
    case cwFutureManagerModel::DetailProgressRole:
        return watcher.detailDone;
    case cwFutureManagerModel::DetailTotalRole:
        return watcher.detailTotal;
    case cwFutureManagerModel::TreeBackedRole:
        return watcher.tree != nullptr;
    default:
        break;
    }

    return QVariant();
}

QHash<int, QByteArray> cwFutureManagerModel::roleNames() const
{
    return defaultRoles();
}

/**
 * Averages the jobs that can say how far along they are, ignoring the ones that
 * can't. Each measured job counts once: a point cloud's steps are points and a
 * scrap's are outlines, so weighting by step count would just let whichever job
 * chose the finer unit decide the whole number.
 */
double cwFutureManagerModel::progress() const
{
    double sum = 0.0;
    int measured = 0;

    for(const auto& container : Watchers) {
        const int steps = container.watcher->progressMaximum();

        if(steps <= 0) {
            continue;
        }

        const int completed = container.watcher->progressValue();
        sum += std::clamp(static_cast<double>(completed) / steps, 0.0, 1.0);
        measured++;
    }

    return measured > 0 ? sum / measured : kIndeterminateProgress;
}

void cwFutureManagerModel::waitForFinished()
{
    while(!Watchers.isEmpty()) {
        auto watcher = Watchers.first();
        AsyncFuture::waitForFinished(watcher.job.future());
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
        {RunTimeRole, "runTimeRole"},
        {DetailNameRole, "detailNameRole"},
        {DetailProgressRole, "detailProgressRole"},
        {DetailTotalRole, "detailTotalRole"},
        {TreeBackedRole, "treeBackedRole"}
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

    emit countChanged();
    emit progressChanged();

    if(rowCount() == 0) {
        Timer->stop();
        emit allFinished();
    }

    updateDetailTimer();

    delete watcher;
}

/**
 * Oldest-first keeps the line still: a leaf holds it until it finishes, so
 * parallel workers don't fight over it. The root is skipped, since the row
 * already carries its name.
 */
bool cwFutureManagerModel::detailFor(const cwProgressNodePtr &root, QString &name, qint64 &done, qint64 &total)
{
    name = QString();
    done = 0;
    total = 0;

    if(!root) {
        return false;
    }

    cwProgressNodePtr oldest;
    for(const cwProgressNodePtr& child : root->activeChildren()) {
        findOldestLeaf(child, oldest);
    }

    if(!oldest) {
        return false;
    }

    name = oldest->name();
    done = oldest->done();
    total = oldest->total();
    return true;
}

void cwFutureManagerModel::pollDetails()
{
    for(int row = 0; row < Watchers.size(); row++) {
        WatcherContainer& container = Watchers[row];

        if(!container.tree) {
            continue;
        }

        QString name;
        qint64 done = 0;
        qint64 total = 0;
        detailFor(container.tree, name, done, total);

        if(container.detailName == name
                && container.detailDone == done
                && container.detailTotal == total) {
            continue;
        }

        container.detailName = name;
        container.detailDone = done;
        container.detailTotal = total;

        const QModelIndex modelIndex = index(row);
        emit dataChanged(modelIndex, modelIndex,
                         {DetailNameRole, DetailProgressRole, DetailTotalRole});
    }
}

void cwFutureManagerModel::updateDetailTimer()
{
    const bool hasTree = std::any_of(Watchers.cbegin(), Watchers.cend(),
                                     [](const WatcherContainer& container) {
        return container.tree != nullptr;
    });

    if(hasTree) {
        if(!DetailTimer->isActive()) {
            DetailTimer->start();
        }
    } else {
        DetailTimer->stop();
    }
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
