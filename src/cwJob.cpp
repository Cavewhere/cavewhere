//Our inculdes
#include "cwJob.h"
#include "QCoreApplication"

cwJob::cwJob() :
    Context(QCoreApplication::instance())
{

}

cwJob::cwJob(QObject *context) :
    Context(context)
{

}




