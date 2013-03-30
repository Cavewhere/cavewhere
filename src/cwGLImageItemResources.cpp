#include "cwGLImageItemResources.h"

cwGLImageItemResources::cwGLImageItemResources() :
    cwGLResources(),
    NoteTexture(NULL)
{
}

cwGLImageItemResources::~cwGLImageItemResources()
{
    if(context() != NULL) {
        context()->makeCurrent(context()->surface());
        GeometryVertexBuffer.destroy();
        delete NoteTexture;
    }
}
