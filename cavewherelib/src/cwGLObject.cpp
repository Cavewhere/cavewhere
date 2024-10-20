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

//Qt includes
#include <QOpenGLShaderProgram>
#include <QFile>

cwGLObject::cwGLObject(QObject* parent) :
    QObject(parent),
    Scene(nullptr),
    QueuedDataCommand(nullptr)
{
}

cwGLObject::~cwGLObject()
{

}

// void cwGLObject::initilizeGLFunctions()
// {
//     initializeOpenGLFunctions();
// }

/**
 * @brief cwGLObject::updateData
 *
 * You should reimplement this function but call this function from the sub class
 * This function is called from the rendering thread and updates openGL data.
 */
// void cwGLObject::updateData()
// {
//     QueuedDataCommand = nullptr;
// }

void cwGLObject::setScene(cwScene *scene)
{
    if(Scene != scene) {
        if(Scene != nullptr) {
            Scene->removeItem(this);
        }

        Scene = scene;

        if(Scene != nullptr) {
            Scene->addItem(this);

            // cwInitCommand* initCommand = new cwInitCommand();
            // initCommand->setGLObject(this);
            // Scene->addSceneCommand(initCommand);

            markDataAsDirty();
        }
    }
}

 cwCamera* cwGLObject::camera() const {
    return Scene == nullptr ? nullptr : Scene->camera();
}

 // cwShaderDebugger* cwGLObject::shaderDebugger() const {
 //     return Scene == nullptr ? nullptr : Scene->shaderDebugger();
 // }

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
     if(QueuedDataCommand != nullptr) { return; }
     if(Scene == nullptr) { return; }

     // QueuedDataCommand = new cwUpdateDataCommand();
     // QueuedDataCommand->setGLObject(this);
     // Scene->addSceneCommand(QueuedDataCommand);
 }

 void cwGLObject::deleteShaders(QOpenGLShaderProgram *program)
 {
     if(program) {
         for(auto shader : program->shaders()) {
             delete shader;
         }
         delete program;
     }
 }

/**
 * @brief cwGLObject::geometryItersecter
 * @return The current geometry intersector
 */
 cwGeometryItersecter *cwGLObject::geometryItersecter() const
{
    return Scene == nullptr ? nullptr : Scene->geometryItersecter();
}

QShader cwRHIObject::loadShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QShader::fromSerialized(f.readAll());

    return QShader();
}
