#ifndef CWSIGNALSPY_H
#define CWSIGNALSPY_H

#include <QSignalSpy>

class cwSignalSpy : public QSignalSpy
{
public:
    explicit cwSignalSpy(const QObject *obj, const char *aSignal)
        : QSignalSpy(obj, aSignal)  {}

    template <typename Func>
    cwSignalSpy(const typename QtPrivate::FunctionPointer<Func>::Object *obj, Func signal0)
        : QSignalSpy(obj,signal0) {}

    cwSignalSpy(const QObject *obj, QMetaMethod signal)
        : QSignalSpy(obj, signal) {}

    void setObjectName(const QString& name) {
        m_objectName = name;
    }

    QString objectName() const {
        return m_objectName;
    }

private:
    QString m_objectName;
};

#endif // CWSIGNALSPY_H
