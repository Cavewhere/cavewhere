/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGLImageItemResources.h"

cwGLImageItemResources::cwGLImageItemResources() :
    cwGLResources(),
    NoteTexture(nullptr)
{
}

cwGLImageItemResources::~cwGLImageItemResources()
{
    if(context() != nullptr) {
        context()->makeCurrent(context()->surface());
        GeometryVertexBuffer.destroy();
        delete NoteTexture;
    }
}
