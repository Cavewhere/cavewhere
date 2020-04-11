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
    Q_PROPERTY(bool automaticUpdate READ automaticUpdate WRITE setAutomaticUpdate NOTIFY automaticUpdateChanged)

public:
    int threadCount() const;
    void setThreadCount(int threadCount);

    int idleThreadCount() const;

    bool automaticUpdate() const;
    void setAutomaticUpdate(bool automaticUpdate);

    static cwJobSettings* instance();
    static void initialize();

signals:
    void threadCountChanged();
    void automaticUpdateChanged();

private:
    static cwJobSettings* Settings;
    static const QString ThreadCountKey;
    static const QString AutomaticUpdateKey;

    bool AutomaticUpdate = true;

    cwJobSettings(QObject* parent = nullptr);

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
