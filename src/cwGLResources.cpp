#include "cwGLResources.h"

cwGLResources::cwGLResources(QObject *parent) :
    QObject(parent),
    Context(NULL)
{
}

void cwGLResources::setContext( QOpenGLContext *context)
{
    if(Context != context) {

        if(Context != NULL) {
            disconnect(Context, SIGNAL(destroyed()), this, SLOT(contextDestroyed()));
        }

        Context = context;

        if(Context != NULL) {
            connect(Context, SIGNAL(destroyed()), this, SLOT(contextDestroyed()));
        }
    }
}

 QOpenGLContext *cwGLResources::context() const
{
     return Context;
 }

 void cwGLResources::contextDestroyed()
 {
     Context = NULL;
 }
