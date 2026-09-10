/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwRhiViewer.h"
#include "cwCamera.h"
#include "cwScene.h"
#include "cwRhiItemRenderer.h"

//Qt includes
#include <QPainter>
#include <QRect>
#include <QDebug>
#include <QRunnable>
#include <QTimer>
#include <QSGRenderNode>
#include <QQuickWindow>

cwRhiViewer::cwRhiViewer(QQuickItem *parent) :
    m_rendererHandle(std::make_shared<cwRhiRendererHandle>())
{
    Camera = new cwCamera(this);

    connect(this, SIGNAL(widthChanged()), SLOT(privateResize()));
    connect(this, SIGNAL(heightChanged()), SLOT(privateResize()));
    connect(Camera, SIGNAL(viewMatrixChanged()), SLOT(updateRenderer()));
    connect(Camera, SIGNAL(projectionChanged()), SLOT(updateRenderer()));
}

cwRhiViewer::~cwRhiViewer() {
}

void cwRhiViewer::privateResize() {
//    if(GLWidget == nullptr) { return; }
    if(width() == 0.0 || height() == 0.0) { return; }
    QSize framebufferSize(width(), height());

    //Update the viewport
    QRect viewportRect(QPoint(0, 0), framebufferSize);
    Camera->setViewport(viewportRect);

    //Update the subclass
    resizeGL();

    update();
}

QQuickRhiItemRenderer *cwRhiViewer::createRenderer()
{
    return new cwRhiItemRenderer();
}

/**
 * @brief cwRhiViewer::itemChange
 *
 * A hidden QQuickRhiItem keeps its renderer, its scene, and every streamed
 * texture resident in it, while getting no synchronize or render callbacks at
 * all. GPU-budget enforcement runs inside the frame, so a hidden view can never
 * give detail back and its residency counts against every view still drawing.
 * A render job is the only way to reach its scene; showing again re-streams the
 * levels from the warm disk cache through the normal selection path.
 */
void cwRhiViewer::itemChange(ItemChange change, const ItemChangeData& value)
{
    QQuickRhiItem::itemChange(change, value);

    if(change != ItemVisibleHasChanged) {
        return;
    }

    m_rendererHandle->setViewVisible(value.boolValue);

    auto* renderWindow = window();
    if(value.boolValue) {
        update();
    } else if(renderWindow) {
        // The handle keeps the job safe: the renderer it names may be gone by
        // the time the job runs, and then the job does nothing.
        auto handle = m_rendererHandle;
        renderWindow->scheduleRenderJob(QRunnable::create([handle]() {
                                            handle->releaseStreamedTextures();
                                        }),
                                        QQuickWindow::BeforeSynchronizingStage);
        // Nothing else is going to draw the frame that runs the job — this item
        // just stopped being visible.
        renderWindow->update();
    }
}


/**
 * @brief cwRhiViewer::setScene
 * @param scene
 */
void cwRhiViewer::setScene(cwScene* scene) {
    if(Scene != scene) {

        if(Scene != nullptr) {
            disconnect(Scene.data(), &cwScene::needsRendering, this, &cwRhiViewer::updateRenderer);
        }

        Scene = scene;

        if(Scene != nullptr) {
            Scene->setCamera(Camera);
            connect(Scene.data(), &cwScene::needsRendering, this, &cwRhiViewer::updateRenderer);
        }

        emit sceneChanged();
    }
}

/**
 * @brief cwRhiViewer::scene
 * @return
 */
cwScene* cwRhiViewer::scene() const {
    return Scene;
}
