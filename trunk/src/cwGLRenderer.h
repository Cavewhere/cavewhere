#ifndef CWGLRENDERER_H
#define CWGLRENDERER_H

//Qt includes
#include <QDeclarativeItem>
class QGLWidget;

class cwGLRenderer : public QDeclarativeItem
{
    Q_OBJECT
    Q_PROPERTY(QGLWidget* glWidget READ glWidget WRITE setGLWidget NOTIFY glWidgetChanged)

public:
    explicit cwGLRenderer(QDeclarativeItem *parent = 0);

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *);

public slots:
    void setGLWidget(QGLWidget* widget);
    QGLWidget* glWidget();

signals:
    void glWidgetChanged();


public slots:

private:
    QGLWidget* GLWidget; //This is so we make current when setting up the object

};

inline QGLWidget* cwGLRenderer::glWidget() { return GLWidget; }

#endif // CWGLRENDERER_H
