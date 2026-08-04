#ifndef CWFUTUREMANAGERMODEL_H
#define CWFUTUREMANAGERMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QVector>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QQmlEngine>
#include <QTimer>

//Our includes
#include "cwGlobals.h"
#include "cwFutureManagerToken.h"
#include "cwFuture.h"

class CAVEWHERE_LIB_EXPORT cwFutureManagerModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FutureManagerModel)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(cwFutureManagerToken token READ token CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole,
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

    Q_INVOKABLE void waitForFinished();

    static QHash<int, QByteArray> defaultRoles();

    cwFutureManagerToken token();

    bool isEmpty() const;
    int count() const;

    //! How far along everything is, in [0, 1], or negative when nothing can say
    double progress() const;

signals:
    void intervalChanged();
    void allFinished();
    void countChanged();
    void progressChanged();

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

inline bool cwFutureManagerModel::isEmpty() const {
    return Watchers.isEmpty();
}

inline int cwFutureManagerModel::count() const {
    return rowCount();
}

/**
*
*/
inline cwFutureManagerToken cwFutureManagerModel::token() {
    return cwFutureManagerToken(this);
}


#endif // CWFUTUREMANAGERMODEL_H
