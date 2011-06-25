#ifndef CWNOTEITEM_H
#define CWNOTEITEM_H

//Glew includes
#include <GL/glew.h>

//Our includes
#include "cwGLRenderer.h"

//Qt includes
#include <QDeclarativeItem>
#include <QTransform>
#include <QFutureWatcher>
#include <QTimer>

class cwNoteItem : public cwGLRenderer
{
    Q_OBJECT

    Q_PROPERTY(QString imageSource READ imageSource WRITE setImageSource NOTIFY imageSourceChanged)

public:
    explicit cwNoteItem(QDeclarativeItem *parent = 0);

    QString imageSource() const;
    void setImageSource(QString imageSource);

public slots:
//    void fitToView();

signals:
    void imageSourceChanged();
//    void imageChanged();

public slots:

protected:
    virtual void initializeGL();
    virtual void resizeGL();
    virtual void paintFramebuffer();



private:

    //The shader program for the note
    QGLShaderProgram* ImageProgram;

    //The vertex buffer
    QGLBuffer VertexBuffer;

    //The attribute location of the vVertex
    int vVertex;
    int ModelViewProjectionMatrix;

    //The texture id for rendering the notes to the screen
    GLuint NoteTexture;

    //Creates the scale matrix for the note item
    QMatrix4x4 NoteModelMatrix;

//    QGraphicsPixmapItem* PixmapItem;

//    QTransform FastPixmapTransform;
//    QTransform SmoothTransform;

//    QPixmap FastPixmap;
//    QPixmap SmoothPixmap;

//    QFutureWatcher<QImage>* PixmapFutureWatcher;
    QFutureWatcher<QImage>* LoadNoteWatcher;

//    //LOD timer
//    QTimer* LODTimer;

//    QImage OriginalNote;
    QString NoteSource;
    QSize ImageSize;


//    //For interaction
    QPoint LastPanPoint;
//    QPointF ScaleCenter;

    virtual void mouseMoveEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mousePressEvent ( QGraphicsSceneMouseEvent * event );
//    virtual void mouseReleaseEvent ( QGraphicsSceneMouseEvent * event );
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event);

//    void SetScale(float scaleFactor);

    void initializeShaders();
    void initializeVertexBuffers();
    void initializeTexture();

    static QImage LoadImage(QString& filename);


protected slots:
//    void RenderSmooth();
//    void SetSmoothPixmap();

    void ImageFinishedLoading();



};

/**
  \brief Get's the image's sourec director
  */
inline QString cwNoteItem::imageSource() const {
    return NoteSource;
}


#endif // CWNOTEITEM_H
