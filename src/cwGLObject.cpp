/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwGLObject.h"
#include "cwScene.h"
#include "cwInitCommand.h"
#include "cwUpdateDataCommand.h"

cwGLObject::cwGLObject(QObject* parent) :
    QObject(parent),
    Scene(NULL)
{
//    Dirty = false;
    //    Scene = NULL;
}

cwGLObject::~cwGLObject()
{

}

/**
 * @brief cwGLObject::updateData
 *
 * You should reimplement this function but call this function from the sub class
 * This function is called from the rendering thread and updates openGL data.
 */
void cwGLObject::updateData()
{
    QueuedDataCommand = NULL;
}

void cwGLObject::setScene(cwScene *scene)
{
    if(Scene != scene) {
        if(Scene != NULL) {
            Scene->removeItem(this);
        }

        Scene = scene;

        if(Scene != NULL) {
            Scene->addItem(this);

            cwInitCommand* initCommand = new cwInitCommand();
            initCommand->setGLObject(this);
            Scene->addSceneCommand(initCommand);
        }

//        setDirty(true);
    }
}

 cwCamera* cwGLObject::camera() const {
    return Scene == NULL ? NULL : Scene->camera();
}

 cwShaderDebugger* cwGLObject::shaderDebugger() const {
     return Scene == NULL ? NULL : Scene->shaderDebugger();
 }

 /**
  * @brief cwGLObject::markDataAsDirty
  *
  * This will push a cwUpdateDataCommand to the scene. The cwUpdateDataCommand will
  * call this classes or a sub classes updateData() function on the rendering
  * thread.
  *
  * This class will queue the UpdateDataCommand. It is important that this classes
  * updateData() function is called by the sub class or subsquent calls to this
  * function will do nothing.
  */
 void cwGLObject::markDataAsDirty()
 {
     if(QueuedDataCommand != NULL) { return; }
     if(Scene == NULL) { return; }

     QueuedDataCommand = new cwUpdateDataCommand();
     QueuedDataCommand->setGLObject(this);
     Scene->addSceneCommand(QueuedDataCommand);

 }

/**
 * @brief cwGLObject::geometryItersecter
 * @return The current geometry intersector
 */
 cwGeometryItersecter *cwGLObject::geometryItersecter() const
{
    return Scene == NULL ? NULL : Scene->geometryItersecter();
}

// /**
//  * @brief cwGLObject::setDirty
//  * @param isDirty - Set the cwGLObject to dirty.
//  */
// void cwGLObject::setDirty(bool isDirty)
// {
//     Dirty = isDirty;
//     if(Scene != NULL) {
//         Scene->update();
//     }
// }

