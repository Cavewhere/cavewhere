//Glew includes
#include "C:/Qt/qtcreator-2.2.0/glew-1.6.0/include/GL/glew.h"

//Our includes
#include "cwGLRenderer.h"

//Qt includes
#include <QPainter>



cwGLRenderer::cwGLRenderer(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{
    GLWidget = NULL;
    setFlag(QGraphicsItem::ItemHasNoContents, false);
}

void cwGLRenderer::paint(QPainter* painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->beginNativePainting();
    glBegin(GL_QUADS);
    glColor3ub(0,0,255);
    glVertex2d(0, 0);
    glVertex2d(0, height());
    glColor3ub(255,0,0);
    glVertex2d(width(), height());
    glVertex2d(width(), 0);
    glEnd();
    painter->endNativePainting();

}

/**
  \brief Sets the QGLWidget for this renderer

  This assumes that the widget's context is created and is valid.
  */
void cwGLRenderer::setGLWidget(QGLWidget* widget) {
    if(widget == GLWidget) {
        return;
    }

    qDebug() << "Set glWidget:" << widget;

    //Initialize the object here!
    GLWidget = widget;

    emit glWidgetChanged();
}
