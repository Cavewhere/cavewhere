#ifndef CWFUTUREMANAGERMODEL_H
#define CWFUTUREMANAGERMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QtConcurrent>
#include <QVector>
#include <QFutureWatcher>
#include <QElapsedTimer>

//Our includes
#include "cwGlobals.h"
#include "cwFutureManagerToken.h"
#include "cwFuture.h"

class CAVEWHERE_LIB_EXPORT cwFutureManagerModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(cwFutureManagerToken token READ token CONSTANT)

public:
    enum Roles {
        NameRole,
        ProgressRole,
        NumberOfStepRole,
        RunTimeRole
    };

    cwFutureManagerModel(QObject* parent = nullptr);
    ~cwFutureManagerModel() {}

    int interval() const;
    void setInterval(int interval);

    void addJob(const cwFuture& job);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    void waitForFinished();

    static QHash<int, QByteArray> defaultRoles();

    cwFutureManagerToken token();

signals:
    void intervalChanged();

private:
    class WatcherContainer {
    public:
        cwFuture job;
        bool visible;
        QElapsedTimer startTime;
        QFutureWatcher<void>* watcher = nullptr;
    };

    QVector<WatcherContainer> Watchers;
    QTimer* Timer;

    void removeWatcher(QFutureWatcher<void> *watcher);

    QModelIndex indexOf(const QFutureWatcher<void>* watcher) const;
};

inline int cwFutureManagerModel::interval() const {
    return Timer->interval();
}

/**
*
*/
inline cwFutureManagerToken cwFutureManagerModel::token() {
    cwFutureManagerModel* model = this;
    cwFutureManagerToken token(model);
    return token;
}


#endif // CWFUTUREMANAGERMODEL_H
