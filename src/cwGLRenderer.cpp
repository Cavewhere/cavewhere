
//Our includes
#include "cwGLRenderer.h"
#include "cwCamera.h"
#include "cwShaderDebugger.h"

//Qt includes
#include <QPainter>
#include <QRect>
#include <QDebug>
#include <QGLWidget>

//Std includes
#include "cwMath.h"


cwGLRenderer::cwGLRenderer(QQuickItem *parent) :
    QQuickPaintedItem(parent),
    Initialized(false)
{
//    GLWidget = NULL;
    setFlag(QQuickItem::ItemHasContents, true);

    GeometryItersecter = new cwGeometryItersecter();
    Camera = new cwCamera(this);
    ShaderDebugger = new cwShaderDebugger(this);

    connect(this, SIGNAL(widthChanged()), SLOT(privateResizeGL()));
    connect(this, SIGNAL(heightChanged()), SLOT(privateResizeGL()));
    connect(Camera, SIGNAL(viewChanged()), SLOT(updateRenderer()));
    connect(Camera, SIGNAL(projectionChanged()), SLOT(updateRenderer()));

    setRenderTarget(QQuickPaintedItem::InvertedYFramebufferObject);
    setAntialiasing(false);
    setOpaquePainting(false);
}

cwGLRenderer::~cwGLRenderer() {
    delete GeometryItersecter;
    Camera->deleteLater();
    ShaderDebugger->deleteLater();
}

void cwGLRenderer::privateResizeGL() {
//    if(GLWidget == NULL) { return; }
    if(width() == 0.0 || height() == 0.0) { return; }
    QSize framebufferSize(width(), height());

    //Update the viewport
    QRect viewportRect(QPoint(0, 0), framebufferSize);
    Camera->setViewport(viewportRect);

    //Update the subclass
    resizeGL();

    update();
}


QSGNode * cwGLRenderer::updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *data) {
    if(!Initialized) {
        initializeGL();
        Initialized = true;
    }
    return QQuickPaintedItem::updatePaintNode(oldNode, data);
}

