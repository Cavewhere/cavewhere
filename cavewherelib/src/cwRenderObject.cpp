/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderObject.h"
#include "cwScene.h"
#include "cwInitCommand.h"
#include "cwUpdateDataCommand.h"

//Qt includes
#include <QOpenGLShaderProgram>
#include <QFile>

cwRenderObject::cwRenderObject(QObject* parent) :
    QObject(parent),
    m_scene(nullptr)
{
}

cwRenderObject::~cwRenderObject()
{

}

void cwRenderObject::setScene(cwScene *scene)
{
    if(m_scene != scene) {
        if(m_scene != nullptr) {
            m_scene->removeItem(this);
        }

        m_scene = scene;

        if(m_scene != nullptr) {
            m_scene->addItem(this);
        }

        emit sceneChange();
    }
}

 cwCamera* cwRenderObject::camera() const {
    return m_scene == nullptr ? nullptr : m_scene->camera();
}

 /**
  * @brief cwRenderObject::markDataAsDirty
  *
  * This will push a cwUpdateDataCommand to the scene. The cwUpdateDataCommand will
  * call this classes or a sub classes updateData() function on the rendering
  * thread.
  *
  * This class will queue the UpdateDataCommand. It is important that this classes
  * updateData() function is called by the sub class or subsquent calls to this
  * function will do nothing.
  */
 void cwRenderObject::markDataAsDirty()
 {
     update();
 }


 void cwRenderObject::update()
 {
     if(m_scene == nullptr) { return; }
     m_scene->updateItem(this);
 }


/**
 * @brief cwRenderObject::geometryItersecter
 * @return The current geometry intersector
 */
 cwGeometryItersecter *cwRenderObject::geometryItersecter() const
{
    return m_scene == nullptr ? nullptr : m_scene->geometryItersecter();
}


