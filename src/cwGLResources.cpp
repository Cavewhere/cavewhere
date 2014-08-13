/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGLResources.h"

cwGLResources::cwGLResources(QObject *parent) :
    QObject(parent),
    Context(nullptr)
{
}

void cwGLResources::setContext( QOpenGLContext *context)
{
    if(Context != context) {

        if(Context != nullptr) {
            disconnect(Context, SIGNAL(destroyed()), this, SLOT(contextDestroyed()));
        }

        Context = context;

        if(Context != nullptr) {
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
     Context = nullptr;
 }
