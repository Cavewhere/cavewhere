#ifndef CWJOB_H
#define CWJOB_H

//Qt inculdes
#include <QObject>
#include <QPointer>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwJob
{
public:
    cwJob();
    cwJob(QObject* context);

    void setContext(QObject* context)
    {
        Context = context;
    }

    QObject* context() const
    {
        return Context.data();
    }

private:
    QPointer<QObject> Context;

};

#endif // CWJOB_H
