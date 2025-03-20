/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderObject.h"
#include "cwScene.h"

//Qt includes
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


void cwRenderObject::setVisible(bool newVisible)
{
    if (m_visible == newVisible)
        return;
    m_visible = newVisible;
    update();
    emit visibleChanged();
}
