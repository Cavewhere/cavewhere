/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderObject.h"
#include "cwScene.h"
#include "cwGeometryItersecter.h"

//Qt includes
#include <QFile>
#include <QAtomicInteger>

namespace {
    // Monotonic source of cwRenderObject ids. Atomic so ids stay unique even if a
    // render object is ever constructed off the main thread; relaxed ordering is
    // enough — we need value uniqueness, not any happens-before relationship.
    // Starts at 1 so 0 can read as "no render object".
    QAtomicInteger<quint64> g_nextRenderObjectId = 1;
}

cwRenderObject::cwRenderObject(QObject* parent) :
    QObject(parent),
    m_scene(nullptr),
    m_renderObjectId(static_cast<cwRenderObjectId>(g_nextRenderObjectId.fetchAndAddRelaxed(1)))
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
