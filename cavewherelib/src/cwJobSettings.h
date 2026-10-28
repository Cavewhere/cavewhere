#ifndef CWJOBSETTINGS_H
#define CWJOBSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwJobSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(JobSettings)
    QML_UNCREATABLE("JobSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(int threadCount READ threadCount WRITE setThreadCount NOTIFY threadCountChanged)
    Q_PROPERTY(int idleThreadCount READ idleThreadCount CONSTANT)
    Q_PROPERTY(int defaultThreadCount READ defaultThreadCount CONSTANT)
    Q_PROPERTY(bool automaticUpdate READ automaticUpdate WRITE setAutomaticUpdate NOTIFY automaticUpdateChanged)
    Q_PROPERTY(int memoryBudgetMb READ memoryBudgetMb WRITE setMemoryBudgetMb NOTIFY memoryBudgetMbChanged)
    Q_PROPERTY(int resolvedMemoryBudgetMb READ resolvedMemoryBudgetMb NOTIFY memoryBudgetMbChanged)
    Q_PROPERTY(int totalMemoryMb READ totalMemoryMb CONSTANT)
    Q_PROPERTY(bool isAtDefaults READ isAtDefaults NOTIFY isAtDefaultsChanged)

public:
    int threadCount() const;
    void setThreadCount(int threadCount);

    int idleThreadCount() const;
    int defaultThreadCount() const;

    bool automaticUpdate() const;
    void setAutomaticUpdate(bool automaticUpdate);

    // 0 = automatic: cwMemoryBudget sizes its cap from the platform reader.
    int memoryBudgetMb() const;
    void setMemoryBudgetMb(int memoryBudgetMb);

    // The cap cwMemoryBudget uses now, in MiB, for the "Automatic (x GB)" label.
    int resolvedMemoryBudgetMb() const;

    // The device's physical memory, in MiB: the memory budget setting's upper bound.
    int totalMemoryMb() const;

    bool isAtDefaults() const;
    Q_INVOKABLE void resetToDefaults();

    static cwJobSettings* instance();
    static void initialize();

signals:
    void threadCountChanged();
    void automaticUpdateChanged();
    void memoryBudgetMbChanged();
    void isAtDefaultsChanged();

private:
    static cwJobSettings* Settings;

    static QString automaticUpdateKey() { return QLatin1String("automaticUpdate"); }
    static QString threadCountKey() { return QLatin1String("threadCount"); }
    static QString memoryBudgetMbKey() { return QStringLiteral("memoryBudgetMb"); }

    bool AutomaticUpdate = true;
    int m_memoryBudgetMb = 0;

    cwJobSettings(QObject* parent = nullptr);

    void applyMemoryBudget();

    void setThreadCountPrivate(int count);
    bool isThreadCountValid(int count) const;
};

inline bool cwJobSettings::isThreadCountValid(int count) const
{
    return count >= 1 && count <= idleThreadCount();
}

inline bool cwJobSettings::automaticUpdate() const {
    return AutomaticUpdate;
}

#endif // CWJOBSETTINGS_H
