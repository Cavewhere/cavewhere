#include "cwGLObject.h"

cwGLObject::cwGLObject(QObject* parent) :
    QObject(parent)
{
    ShaderDebugger = NULL;
    Camera = NULL;
    Dirty = false;
}



