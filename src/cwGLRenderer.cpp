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
    MultiSampleFramebuffer(NULL),
    TextureFramebuffer(0),
    ColorTexture(0),
    DepthTexture(0),
    HasBlit(false)
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
}

cwGLRenderer::~cwGLRenderer() {
//    //Delete all the gl stuff
//    if(MultiSampleFramebuffer != NULL) {
//        if(TextureFramebuffer != 0 &&
//                MultiSampleFramebuffer->handle() != TextureFramebuffer) {
//            glDeleteFramebuffersEXT(1, &TextureFramebuffer);
//        }

//        if(ColorTexture != 0 &&
//                ColorTexture != MultiSampleFramebuffer->texture()) {
//            glDeleteTextures(1, &ColorTexture);
//        }

//        if(DepthTexture != 0) {
//            glDeleteTextures(1, &DepthTexture);
//        }

//        delete MultiSampleFramebuffer;
//    }
}


//void cwGLRenderer::paint(QPainter* painter, const QStyleOptionGraphicsItem *, QWidget *) {
//    painter->beginNativePainting();

//    //Draw the subclasses paintFramebuffer to the render framebuffer
//    glPushAttrib(GL_VIEWPORT_BIT | GL_SCISSOR_BIT );

//    MultiSampleFramebuffer->bind();
//    glViewport(0, 0, width(), height());
//    glDisable(GL_SCISSOR_TEST);

//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//    paintFramebuffer();

//    glPopAttrib();
//    MultiSampleFramebuffer->release();

//    //Copy the render buffer
//    copyRenderFramebufferToTextures();

//    //Render the texture framebuffer
//    if(painter->hasClipping()) {
//        glPushAttrib(GL_SCISSOR_BIT );
//        glEnable(GL_SCISSOR_TEST);
//        renderTextureFramebuffer();
//        glPopAttrib();
//    } else {
//        renderTextureFramebuffer();
//    }

//    painter->endNativePainting();
//}

///**
//  This copies the render buffer to the texture framebuffer object
//  */
//void cwGLRenderer::copyRenderFramebufferToTextures() {
//    if(HasBlit) {
//        //Copy the MultiSampleFramebuffer data to the textureFramebuffer
//        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, MultiSampleFramebuffer->handle());
//        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, TextureFramebuffer);
//        glBlitFramebufferEXT(0, 0, MultiSampleFramebuffer->width(), MultiSampleFramebuffer->height(),
//                          0, 0, MultiSampleFramebuffer->width(), MultiSampleFramebuffer->height(),
//                          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
//                          GL_NEAREST);

//        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);
//        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
//    }
//}

///**
//  \brief This renderes the texture framebuffer to the screen
//  */
//void cwGLRenderer::renderTextureFramebuffer() {
//    //Draw the texture that the TextureFramebuffer just rendered to
//    glEnable(GL_TEXTURE_2D);
//    glBindTexture(GL_TEXTURE_2D, ColorTexture);

//    glBegin(GL_QUADS);
//    glTexCoord2f(0.0, 1.0);
//    glColor3f(1.0f, 1.0f, 1.0f);
//    glVertex2f(0, 0);

//    glTexCoord2f(0.0, 0.0);
//    glVertex2f(0, height());

//    glTexCoord2f(1.0, 0.0);
//    glVertex2f(width(), height());

//    glTexCoord2f(1.0, 1.0);
//    glVertex2f(width(), 0);

//    glEnd();
//    glBindTexture(GL_TEXTURE_2D, 0);
//}



///**
//  \brief This initializes this item's opengl stuff
//  */
//void cwGLRenderer::privateInitializeGL() {
//    //Genereate the multi sample buffer for anti aliasing
//    MultiSampleFramebuffer = new QOpenGLFramebufferObject(QSize(1, 1));
//    HasBlit = false; //MultiSampleFramebuffer->hasOpenGLFramebufferBlit();

//    if(HasBlit) {
//        //Generate the texture framebuffer for rendering
//        glGenFramebuffersEXT(1, &TextureFramebuffer);

//        //Generate the color texture
//        glGenTextures(1, &ColorTexture);
//        glBindTexture(GL_TEXTURE_2D, ColorTexture);
//        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
//        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
//        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
//        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
//        glBindTexture(GL_TEXTURE_2D, 0);

//        //Generate the depth texture
//        glGenTextures(1, &DepthTexture);
//        glBindTexture(GL_TEXTURE_2D, DepthTexture);
//        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
//        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
//        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
//        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
//        glBindTexture(GL_TEXTURE_2D, 0);
//    }

//    initializeGL();
//}

void cwGLRenderer::privateResizeGL() {
    qDebug() << "Private resize:" << width() << height() << contentsSize();

//    if(GLWidget == NULL) { return; }
    if(width() == 0.0 || height() == 0.0) { return; }
    QSize framebufferSize(width(), height());

//    if(!framebufferSize.isValid()) { return; }

//    //Recreate the multisample framebuffer
//    delete MultiSampleFramebuffer;
//    QOpenGLFramebufferObjectFormat multiSampleFormat;
//    if(HasBlit) {
//        multiSampleFormat.setSamples(8);

//        //Don't alloc
//        glBindTexture(GL_TEXTURE_2D, ColorTexture);
//        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
//                     framebufferSize.width(), framebufferSize.height(), 0,
//                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);

//        glBindTexture(GL_TEXTURE_2D, DepthTexture);
//        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
//                     framebufferSize.width(), framebufferSize.height(), 0,
//                     GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

//        glBindFramebufferEXT(GL_FRAMEBUFFER, TextureFramebuffer);
//        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ColorTexture, 0);

//        glBindFramebufferEXT(GL_FRAMEBUFFER, TextureFramebuffer);
//        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthTexture, 0);

//        glBindTexture(GL_TEXTURE_2D, 0);

//        GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
//        if(status != GL_FRAMEBUFFER_COMPLETE) {
//            qDebug() << "Can't complete framebuffer:" << TextureFramebuffer;
//        }

//        glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
//    } else {
//        //No multi sampling, doesn't support blit
//        multiSampleFormat.setSamples(0);
//    }

//    multiSampleFormat.setAttachment(QOpenGLFramebufferObject::Depth);
//    multiSampleFormat.setInternalTextureFormat(GL_RGBA);
//    MultiSampleFramebuffer = new QOpenGLFramebufferObject(framebufferSize, multiSampleFormat);

//    if(!HasBlit) {
//        //No multi sampling support
//        TextureFramebuffer = MultiSampleFramebuffer->handle();
//        ColorTexture = MultiSampleFramebuffer->texture();
//    }
    //Update the viewport
    QRect viewportRect(QPoint(0, 0), framebufferSize);
    Camera->setViewport(viewportRect);

    //Update the subclass
    resizeGL();

    update();
}

///**
//  \brief Sets the QGLWidget for this renderer

//  This assumes that the widget's context is created and is valid.
//  */
//void cwGLRenderer::setGLWidget(QGLWidget* widget) {
//    if(widget == GLWidget) {
//        return;
//    }

//    //Initialize the object here!
//    GLWidget = widget;
//    GLWidget->makeCurrent();
//    privateInitializeGL();

//    emit glWidgetChanged();

//    privateResizeGL();
//}

/**
  Unprojects the screen point at point and returns a QVector3d in world coordinates
  */
QVector3D cwGLRenderer::unProject(QPoint point) {
    //Sample the depth buffer
    float depth = sampleDepthBuffer(point);

    //Unproject the point
    return Camera->unProject(point, depth);
}

/**
  Samples the depth buffer using a 3 by 3 filter

  This returns an average of valid samples in that area.  If no samples are valid (when the
  user clicks on an empty region for example), then this will always return 1.0.

  The return value will be from 0.0 to 1.0
  */
float cwGLRenderer::sampleDepthBuffer(QPoint point) {
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

//void cwGLRenderer::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
//{
////    setImplicitSize(newGeometry.width(), newGeometry.height());
//    privateResizeGL();
//}


