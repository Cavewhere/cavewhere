#ifndef CWNOTEITEM_H
#define CWNOTEITEM_H

//Glew includes
#include <GL/glew.h>

//Our includes
#include "cwGLRenderer.h"
#include "cwImage.h"

//Qt includes
#include <QDeclarativeItem>
#include <QTransform>
#include <QFutureWatcher>
#include <QTimer>

class cwNoteItem : public cwGLRenderer
{
    Q_OBJECT

    Q_PROPERTY(cwImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(QString projectFilename READ projectFilename WRITE setProjectFilename NOTIFY projectFilenameChanged())

public:
    explicit cwNoteItem(QDeclarativeItem *parent = 0);

    cwImage image() const;
    void setImage(cwImage image);

    //This allows use to extract data
    QString projectFilename() const;
    void setProjectFilename(QString projectFilename);

public slots:

signals:
    void imageChanged();
    void projectFilenameChanged();

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

    QFutureWatcher<QPair<QByteArray, QSize> >* LoadNoteWatcher;

    cwImage Image;
    QSize ImageSize;

    //The project filename for this class
    QString ProjectFilename;


//    //For interaction
    QPoint LastPanPoint;

    virtual void mouseMoveEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mousePressEvent ( QGraphicsSceneMouseEvent * event );
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event);

    void initializeShaders();
    void initializeVertexBuffers();
    void initializeTexture();


    /**
      This class allow use to load the mipmaps in a thread way
      from the database

      The project filename must be set so we can load the data
*/
    class LoadImage {
    public:
        LoadImage(QString projectFilename) :
            Filename(projectFilename)
        {

        }

        typedef QPair<QByteArray, QSize> result_type;

        QPair<QByteArray, QSize> operator()(int imageId);

        QString Filename;
    };



protected slots:
    void ImageFinishedLoading();

};

/**
  \brief Get's the image's sourec director
  */
inline cwImage cwNoteItem::image() const {
    return Image;
}

/**
  \brief Get's the project Filename
  */
inline QString cwNoteItem::projectFilename() const {
    return ProjectFilename;
}


#endif // CWNOTEITEM_H
