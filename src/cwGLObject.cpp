//Our includes
#include "cwGLObject.h"
#include "cwGLRenderer.h"

cwGLObject::cwGLObject(QObject* parent) :
    QObject(parent)
{
    Dirty = false;
    Scene = NULL;
}

void cwGLObject::setScene(cwGLRenderer *renderer)
{
    if(Scene != renderer) {
        Scene = renderer;
        setDirty(true);
    }
}

 cwCamera* cwGLObject::camera() const {
    return Scene == NULL ? NULL : Scene->camera();
}

 cwShaderDebugger* cwGLObject::shaderDebugger() const {
    return Scene == NULL ? NULL : Scene->shaderDebugger();
}

/**
 * @brief cwGLObject::geometryItersecter
 * @return The current geometry intersector
 */
 cwGeometryItersecter *cwGLObject::geometryItersecter() const
{
    return Scene == NULL ? NULL : Scene->geometryItersecter();
}

 /**
  * @brief cwGLObject::setDirty
  * @param isDirty - Set the cwGLObject to dirty.
  */
 void cwGLObject::setDirty(bool isDirty)
 {
     Dirty = isDirty;
     if(Scene != NULL) {
         Scene->update();
     }
 }

