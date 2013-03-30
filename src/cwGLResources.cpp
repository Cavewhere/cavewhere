#include "cwGLResources.h"

cwGLResources::cwGLResources(QObject *parent) :
    QObject(parent),
    Context(NULL)
{
}

void cwGLResources::setContext( QOpenGLContext *context)
{
    Context = context;
}

 QOpenGLContext *cwGLResources::context() const
{
    return Context;
}
