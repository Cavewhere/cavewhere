//Glew includes
#include "GL/glew.h"

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

    Camera = new cwCamera(this);
    ShaderDebugger = new cwShaderDebugger(this);

    connect(this, SIGNAL(widthChanged()), SLOT(privateResizeGL()));
    connect(this, SIGNAL(heightChanged()), SLOT(privateResizeGL()));
    connect(Camera, SIGNAL(viewChanged()), SLOT(updateRenderer()));
    connect(Camera, SIGNAL(projectionChanged()), SLOT(updateRenderer()));

    setRenderTarget(QQuickPaintedItem::InvertedYFramebufferObject);
    setAntialiasing(true);
    setOpaquePainting(true);
}

cwGLRenderer::~cwGLRenderer() {
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

/**
  Samples the depth buffer using a 3 by 3 filter

  This returns an average of valid samples in that area.  If no samples are valid (when the
  user clicks on an empty region for example), then this will always return 1.0.

  The return value will be from 0.0 to 1.0
  */
float cwGLRenderer::sampleDepthBuffer(QPoint /*point*/) {
//    GLWidget->makeCurrent();

//    const int samplerSize = 3;
//    const int samplerCenter = samplerSize / 2;
//    const QRect samplerArea(QPoint(point.x() - samplerCenter, point.y() - samplerCenter),
//                        QSize(samplerSize, samplerSize));

//    //Allocate memory
//    QVector<float> buffer;
//    int bufferSize = samplerArea.width() * samplerArea.height();
//    buffer.reserve(bufferSize);
//    buffer.resize(bufferSize);

//    //Get data from opengl framebuffer
//    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, TextureFramebuffer);
//    glReadPixels(samplerArea.x(), samplerArea.y(), //where
//                 samplerArea.width(), samplerArea.height(), //size
//                 GL_DEPTH_COMPONENT, //what buffer
//                 GL_FLOAT, //Returned data type
//                 buffer.data()); //The buffer for the data
//    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);

//    float sum = 0.0;
//    int count = 0;
//    for(int i = 0; i < buffer.size(); i++) {
//        //Make sure we're in range
//        if(buffer[i] <= .9999999f && buffer[i] > 0.0f) {
//            sum += buffer[i];
//            count++;
//        }
//    }

//    if(count > 0) {
//        //We have at least one valid sample
//        return sum / (float)count;
//    } else {
//        //Use the middle value in the buffer
//        int centerIndex = samplerSize * samplerCenter + samplerCenter;
//        return buffer[centerIndex];
//    }

    return 0.90;
}


