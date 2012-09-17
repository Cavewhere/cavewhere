#include "cwGLObject.h"

cwGLObject::cwGLObject(QObject* parent) :
    QObject(parent)
{
    ShaderDebugger = NULL;
    Camera = NULL;
    Intersector = NULL;
    Dirty = false;
}

/**
 * @brief cwGLObject::setGeometryItersecter
 * @param intersecter
 *
 * This will set the intersector dirty such that the itersector is updated correctly
 */
void cwGLObject::setGeometryItersecter(cwGeometryItersecter *itersecter)
{
    if(Intersector != itersecter) {
        Intersector = itersecter;
        setDirty(true);
    }
}




