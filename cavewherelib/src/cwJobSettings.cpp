//Our inculdes
#include "cwJobSettings.h"
#include "cwTask.h"
#include "cwMemoryBudget.h"
#include "cwRenderBudgets.h"

//Qt includes
#include <QThreadPool>
#include <QSettings>
#include <QThread>
#include <QCoreApplication>

//Std includes
#include <algorithm>

cwJobSettings* cwJobSettings::Settings = nullptr;

cwJobSettings::cwJobSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    int threadCount = settings.value(threadCountKey(), QThread::idealThreadCount()).toInt();
    setThreadCountPrivate(threadCount);
    AutomaticUpdate = settings.value(automaticUpdateKey(), AutomaticUpdate).toBool();
    m_memoryBudgetMb = std::max(settings.value(memoryBudgetMbKey(), m_memoryBudgetMb).toInt(), 0);
    applyMemoryBudget();
}

void cwJobSettings::applyMemoryBudget()
{
    if(auto* budget = cwMemoryBudget::instance()) {
        budget->setCapOverrideBytes(static_cast<qint64>(m_memoryBudgetMb) * cw::budgets::kBytesPerMegabyte);
    }
}

void cwJobSettings::setThreadCountPrivate(int count)
{
    if(isThreadCountValid(count)) {
        cwTask::threadPool()->setMaxThreadCount(count);
    }
}

void cwJobSettings::setThreadCount(int threadCount) {
    if(threadCount >= 1 && threadCount <= idleThreadCount()) {
        if(this->threadCount() != threadCount) {
            QSettings settings;
            settings.setValue(threadCountKey(), threadCount);
            setThreadCountPrivate(threadCount);
            emit threadCountChanged();
            emit isAtDefaultsChanged();
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
    cwMemoryBudget::initialize();
    if(Settings == nullptr) {
        Settings = new cwJobSettings(QCoreApplication::instance());
    }
}

int cwJobSettings::threadCount() const {
    return cwTask::threadPool()->maxThreadCount();
}

void cwJobSettings::setAutomaticUpdate(bool automaticUpdate) {
    if(this->automaticUpdate() != automaticUpdate) {
        QSettings settings;
        settings.setValue(automaticUpdateKey(), automaticUpdate);
        AutomaticUpdate = automaticUpdate;
        emit automaticUpdateChanged();
        emit isAtDefaultsChanged();
    }
}

int cwJobSettings::memoryBudgetMb() const
{
    return m_memoryBudgetMb;
}

void cwJobSettings::setMemoryBudgetMb(int memoryBudgetMb)
{
    memoryBudgetMb = std::max(memoryBudgetMb, 0);
    if(m_memoryBudgetMb == memoryBudgetMb) {
        return;
    }
    m_memoryBudgetMb = memoryBudgetMb;
    QSettings settings;
    settings.setValue(memoryBudgetMbKey(), m_memoryBudgetMb);
    applyMemoryBudget();
    emit memoryBudgetMbChanged();
    emit isAtDefaultsChanged();
}

int cwJobSettings::resolvedMemoryBudgetMb() const
{
    if(auto* budget = cwMemoryBudget::instance()) {
        return static_cast<int>(budget->capBytes() / cw::budgets::kBytesPerMegabyte);
    }
    return m_memoryBudgetMb;
}

int cwJobSettings::totalMemoryMb() const
{
    return static_cast<int>(cwMemoryBudget::totalBytes() / cw::budgets::kBytesPerMegabyte);
}

bool cwJobSettings::isAtDefaults() const {
    return threadCount() == idleThreadCount() && automaticUpdate() && memoryBudgetMb() == 0;
}

void cwJobSettings::resetToDefaults() {
    setThreadCount(idleThreadCount());
    setAutomaticUpdate(true);
    setMemoryBudgetMb(0);
}
