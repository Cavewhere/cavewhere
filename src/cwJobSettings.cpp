//Our inculdes
#include "cwJobSettings.h"

//Qt includes
#include <QThreadPool>
#include <QSettings>
#include <QThread>
#include <QCoreApplication>

cwJobSettings* cwJobSettings::Settings = nullptr;
const QString cwJobSettings::ThreadCountKey = "threadCount";

cwJobSettings::cwJobSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    int threadCount = settings.value(ThreadCountKey, QThread::idealThreadCount()).toInt();
    setThreadCountPrivate(threadCount);
}

void cwJobSettings::setThreadCountPrivate(int count)
{
    if(isThreadCountValid(count)) {
        QThreadPool::globalInstance()->setMaxThreadCount(count);
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
    if(Settings == nullptr) {
        Settings = new cwJobSettings(QCoreApplication::instance());
    }
}

int cwJobSettings::threadCount() const {
    return QThreadPool::globalInstance()->maxThreadCount();
}
