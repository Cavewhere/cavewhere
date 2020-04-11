//Our inculdes
#include "cwJobSettings.h"
#include "cwTask.h"

//Qt includes
#include <QThreadPool>
#include <QSettings>
#include <QThread>
#include <QCoreApplication>

cwJobSettings* cwJobSettings::Settings = nullptr;
const QString cwJobSettings::ThreadCountKey = "threadCount";
const QString cwJobSettings::AutomaticUpdateKey = "automaticUpdate";


cwJobSettings::cwJobSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    int threadCount = settings.value(ThreadCountKey, QThread::idealThreadCount()).toInt();
    setThreadCountPrivate(threadCount);
    AutomaticUpdate = settings.value(AutomaticUpdateKey, AutomaticUpdate).toBool();
}

void cwJobSettings::setThreadCountPrivate(int count)
{
    if(isThreadCountValid(count)) {
        QThreadPool::globalInstance()->setMaxThreadCount(count);
        cwTask::threadPool()->setMaxThreadCount(count);
    }
}

void cwJobSettings::setThreadCount(int threadCount) {
    if(threadCount >= 1 && threadCount <= idleThreadCount()) {
        if(this->threadCount() != threadCount) {
            QSettings settings;
            settings.setValue(ThreadCountKey, threadCount);
            setThreadCountPrivate(threadCount);
            emit threadCountChanged();
        }
    }
}

int cwJobSettings::idleThreadCount() const {
    return QThread::idealThreadCount();
}

cwJobSettings *cwJobSettings::instance()
{
    return Settings;
}

void cwJobSettings::initialize()
{
    cwTask::initilizeThreadPool();
    if(Settings == nullptr) {
        Settings = new cwJobSettings(QCoreApplication::instance());
    }
}

int cwJobSettings::threadCount() const {
    Q_ASSERT(QThreadPool::globalInstance()->maxThreadCount() == cwTask::threadPool()->maxThreadCount());
    return QThreadPool::globalInstance()->maxThreadCount();
}

void cwJobSettings::setAutomaticUpdate(bool automaticUpdate) {
    if(this->automaticUpdate() != automaticUpdate) {
        QSettings settings;
        settings.setValue(AutomaticUpdateKey, automaticUpdate);
        AutomaticUpdate = automaticUpdate;
        emit automaticUpdateChanged();
    }
}
