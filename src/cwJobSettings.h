#ifndef CWJOBSETTINGS_H
#define CWJOBSETTINGS_H

//Qt includes
#include <QObject>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwJobSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int threadCount READ threadCount WRITE setThreadCount NOTIFY threadCountChanged)
    Q_PROPERTY(int idleThreadCount READ idleThreadCount CONSTANT)

public:
    int threadCount() const;
    void setThreadCount(int threadCount);

    int idleThreadCount() const;

    static cwJobSettings* instance();
    static void initialize();

signals:
    void threadCountChanged();

private:
    cwJobSettings(QObject* parent = nullptr);

    static cwJobSettings* Settings;
    static const QString ThreadCountKey;

    void setThreadCountPrivate(int count);
    bool isThreadCountValid(int count) const;
};

inline bool cwJobSettings::isThreadCountValid(int count) const
{
    return count >= 1 && count <= idleThreadCount();
}




#endif // CWJOBSETTINGS_H
