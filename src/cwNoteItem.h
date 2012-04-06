#ifndef CWNOTEITEM_H
#define CWNOTEITEM_H

//Glew includes
#include <GL/glew.h>

//Our includes
#include "cwGLRenderer.h"
#include "cwImage.h"
#include "cwNote.h"
#include "cwTransformUpdater.h"

//Qt includes
#include <QDeclarativeItem>
#include <QTransform>
#include <QFutureWatcher>
#include <QTimer>

class cwNoteItem : public cwGLRenderer
{
    Q_OBJECT

    Q_PROPERTY(cwNote* note READ note WRITE setNote NOTIFY noteChanged)
    Q_PROPERTY(QString projectFilename READ projectFilename WRITE setProjectFilename NOTIFY projectFilenameChanged())

public:
    explicit cwNoteItem(QDeclarativeItem *parent = 0);

    cwNote* note() const;
    void setNote(cwNote* note);

    //This allows use to extract data
    QString projectFilename() const;
    void setProjectFilename(QString projectFilename);

//    Q_INVOKABLE QPointF mapQtViewportToNote(QPointF qtViewportCoordinate);

    //For adding a station
    Q_INVOKABLE int addStation(QPoint qtViewportCoordinate);
    Q_INVOKABLE void moveStation(QPoint qtViewportCoordinate, cwNote* note, int stationIndex);
    Q_INVOKABLE void setSelectedStation(QDeclarativeItem* station);

    //For adding a scrap
    Q_INVOKABLE void addScrapPoint(QPoint qtViewportCoordinate);
//    Q_INVOKABLE void


signals:
    void noteChanged(cwNote* note);
    void projectFilenameChanged();

protected:
    virtual void initializeGL();
    virtual void resizeGL();
    virtual void paintFramebuffer();
//    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem *, QWidget *);

private:

    //The shader program for the note
    QOpenGLShaderProgram* ImageProgram;

    //The vertex buffer
    QOpenGLBuffer NoteVertexBuffer;

    //The vertex buffer for the stations
  //  QOpenGLBuffer StationVertexBuffer;

    //The attribute location of the vVertex
    int vVertex;
    int ModelViewProjectionMatrix;

    //The texture id for rendering the notes to the screen
    GLuint NoteTexture;

    //Creates the scale matrix for the note item
//    QMatrix4x4 ViewMatrix;
    QMatrix4x4 RotationModelMatrix;

    QFutureWatcher<QPair<QByteArray, QSize> >* LoadNoteWatcher;

    cwNote* Note;
    QSize ImageSize;

    //The project filename for this class
    QString ProjectFilename;



    QDeclarativeComponent* NoteStationComponent;
    QList<QDeclarativeItem*> QMLNoteStations;
    QDeclarativeItem* SelectedNoteStation;

    cwTransformUpdater* TransformUpdater; //!< Transform objects with gl coordinates into qt item coordinates

    QGraphicsObject* TestParent;
    QGraphicsPolygonItem* TestRectangle;

//    virtual void wheelEvent(QGraphicsSceneWheelEvent *event);

    void initializeShaders();
    void initializeVertexBuffers();
    void initializeTexture();

//    void updateQMLTransformItem();
    void setRotationMatrix(QMatrix4x4 rotationMatrix);
//    void setViewMatrix(QMatrix4x4 noteScaleModelMatrix);

//    QMatrix4x4 modelViewMatrix() const;

    void drawShotsOfSelectedItem(QPainter* painter);


    /**
      This class allow use to load the mipmaps in a thread way
      from the database

      The project filename must be set so we can load the data
*/
    class LoadImage {
    public:
        LoadImage(QString projectFilename) :
            Filename(projectFilename) {    }

        typedef QPair<QByteArray, QSize> result_type;

        QPair<QByteArray, QSize> operator()(int imageId);

        QString Filename;
    };



private slots:
    void ImageFinishedLoading();
    void noteDeleted();
    void updateNoteRotation(float degrees);
    void setImage(cwImage image);
    void regenerateStationVertices();
    void updateStationPosition(int stationIndex);
    void stationRemoved(int stationIndex);
};

Q_DECLARE_METATYPE(cwNoteItem*)

/**
  \brief Get's the project Filename
  */
inline QString cwNoteItem::projectFilename() const {
    return ProjectFilename;
}

/**
  \brief Get's the note that this item is rendering
  */
inline cwNote* cwNoteItem::note() const {
    return Note;
}

/**
  \brief Removes the point to the Note object from this item
  */
inline void cwNoteItem::noteDeleted() {
    setNote(NULL);
}


#endif // CWNOTEITEM_H
