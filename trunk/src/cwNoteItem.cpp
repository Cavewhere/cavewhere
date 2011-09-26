//Our includes
#include "cwNoteItem.h"
#include "cwShaderDebugger.h"
#include "cwGLShader.h"
#include "cwCamera.h"
#include "cwProjectImageProvider.h"
#include "cwTrip.h"

//Qt includes
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QUrl>
#include <QtConcurrentRun>
#include <QtConcurrentMap>
#include <QFuture>
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QDeclarativeItem>
#include <QDeclarativeContext>



cwNoteItem::cwNoteItem(QDeclarativeItem *parent) :
    cwGLRenderer(parent),
    LoadNoteWatcher(new QFutureWatcher<QPair<QByteArray, QSize> >(this)),
    Note(NULL)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);

    connect(LoadNoteWatcher, SIGNAL(finished()), SLOT(ImageFinishedLoading()));
}

void cwNoteItem::setProjectFilename(QString projectFilename) {
    if(projectFilename != ProjectFilename) {
        ProjectFilename = projectFilename;
        emit projectFilenameChanged();
    }
}

/**
  \brief Sets up the shaders for this item
  */
void cwNoteItem::initializeGL() {
    initializeShaders();
    initializeVertexBuffers();
    initializeTexture();
}

/**
  Initilizes the shaders for this object
  */
void cwNoteItem::initializeShaders() {
    cwGLShader* imageVertexShader = new cwGLShader(QGLShader::Vertex);
    imageVertexShader->setSourceFile("shaders/NoteItem.vert");

    cwGLShader* imageFragmentShader = new cwGLShader(QGLShader::Fragment);
    imageFragmentShader->setSourceFile("shaders/NoteItem.frag");

    ImageProgram = new QGLShaderProgram(this);
    ImageProgram->addShader(imageVertexShader);
    ImageProgram->addShader(imageFragmentShader);

    bool linkingErrors = ImageProgram->link();
    if(!linkingErrors) {
        qDebug() << "Linking errors:" << ImageProgram->log();
    }

    ShaderDebugger->addShaderProgram(ImageProgram);

    ImageProgram->bind();

    vVertex = ImageProgram->attributeLocation("vVertex");
    ModelViewProjectionMatrix = ImageProgram->uniformLocation("ModelViewProjectionMatrix");

    ImageProgram->setUniformValue("Texture", 0); //set the texture unit to 0
}

/**
  Initilizes geometry for rendering the texture
  */
void cwNoteItem::initializeVertexBuffers() {
    //Create the vertex buffer
    NoteVertexBuffer = QGLBuffer(QGLBuffer::VertexBuffer);
    NoteVertexBuffer.create();    //Create the vertexes buffer to render a quad

    QVector<QVector2D> vertices;
    vertices.reserve(4);
    vertices.append(QVector2D(0.0, 1.0));
    vertices.append(QVector2D(0.0, 0.0));
    vertices.append(QVector2D(1.0, 1.0));
    vertices.append(QVector2D(1.0, 0.0));

    //Allocate the buffer array for this object
    NoteVertexBuffer.bind();
    NoteVertexBuffer.setUsagePattern(QGLBuffer::StaticDraw);
    NoteVertexBuffer.allocate(vertices.data(), vertices.size() * sizeof(QVector2D));
    NoteVertexBuffer.release();
}

/**
  \brief This regenerates all the station vertices for the note item
  */
void cwNoteItem::regenerateStationVertices() {

    //Make sure we have a note component so we can create it
    if(NoteStationComponent == NULL) {
        QDeclarativeContext* context = QDeclarativeEngine::contextForObject(this);
        if(context == NULL) { return; }
        NoteStationComponent = new QDeclarativeComponent(context->engine(), "qml/NoteStation.qml", this);
        if(NoteStationComponent->isError()) {
            qDebug() << "Notecomponent errors:" << NoteStationComponent->errorString();
        }
    }

    if(QMLNoteStations.size() < Note->numberOfStations()) {
        int notesToAdd = Note->numberOfStations() - QMLNoteStations.size();
        //Add more stations to the NoteStations
        for(int i = 0; i < notesToAdd; i++) {
            int noteStationIndex = QMLNoteStations.size();

            if(noteStationIndex < 0 || noteStationIndex >= Note->stations().size()) {
                qDebug() << "noteStationIndex out of bounds:" << noteStationIndex << QMLNoteStations.size();
                continue;
            }

            QDeclarativeItem* stationItem = qobject_cast<QDeclarativeItem*>(NoteStationComponent->create());
            if(stationItem == NULL) {
                qDebug() << "Problem creating new station item ... THIS IS A BUG!";
                continue;
            }

            QMLNoteStations.append(stationItem);
        }
    } else if(QMLNoteStations.size() > Note->numberOfStations()) {
        //Remove stations from NoteStations
        int notesToRemove = QMLNoteStations.size() - Note->numberOfStations();
        //Add more stations to the NoteStations
        for(int i = 0; i < notesToRemove; i++) {
            QDeclarativeItem* deleteStation = QMLNoteStations.last();
            QMLNoteStations.removeLast();
            deleteStation->deleteLater();
        }
    }

    for(int i = 0; i < QMLNoteStations.size(); i++) {
        QDeclarativeItem* stationItem = QMLNoteStations[i];
        stationItem->setParentItem(this);
        stationItem->setProperty("stationId", i);
        stationItem->setProperty("noteViewer", QVariant::fromValue<QObject*>(this));
        stationItem->setProperty("note", QVariant::fromValue<QObject*>(Note));
    }

    updateQMLTransformItem();

}

/**
  \brief The initilizes the texture map
  */
void cwNoteItem::initializeTexture() {
    //Generate the color texture
    glGenTextures(1, &NoteTexture);
    glBindTexture(GL_TEXTURE_2D, NoteTexture);
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
    glBindTexture(GL_TEXTURE_2D, 0);
}

/**
  \brief Called when the note item is resized
  */
void cwNoteItem::resizeGL() {

    if(!ImageSize.isValid()) { return; }

    QSize windowSize(width(), height());

    QTransform transformer;
    transformer.rotate(Note->rotate());
    QRectF rotatedImageRect = transformer.mapRect(QRectF(QPointF(), ImageSize));

    QSize scaledImageSize = rotatedImageRect.size().toSize();
    scaledImageSize.scale(windowSize, Qt::KeepAspectRatio);

    float changeInWidth = windowSize.width() / (float)scaledImageSize.width();
    float changeInHeight = windowSize.height() / (float)scaledImageSize.height();

    float widthProjection = ImageSize.width() * changeInWidth;
    float heightProjection = ImageSize.height() * changeInHeight;

    QMatrix4x4 orthognalProjection;
    orthognalProjection.ortho(0.0, widthProjection,
                              0.0, heightProjection,
                              -1.0, 1.0);
    Camera->setProjectionMatrix(orthognalProjection);

    updateQMLTransformItem();
}

/**
  \brief Draws the note item
  */
void cwNoteItem::paintFramebuffer() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(Note == NULL) { return; }

    glDisable(GL_DEPTH_TEST);

    NoteVertexBuffer.bind();

    ImageProgram->bind();
    ImageProgram->setAttributeBuffer(vVertex, GL_FLOAT, 0, 2);
    ImageProgram->enableAttributeArray(vVertex);
    ImageProgram->setUniformValue(ModelViewProjectionMatrix, Camera->viewProjectionMatrix() * modelMatrix());

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, NoteTexture);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); //Draw the quad

    NoteVertexBuffer.release();
    ImageProgram->release();
    glBindTexture(GL_TEXTURE_2D, 0);

}

void cwNoteItem::paint(QPainter* painter, const QStyleOptionGraphicsItem * styleItem, QWidget * widget) {
    cwGLRenderer::paint(painter, styleItem, widget);

    drawShotsOfSelectedItem(painter);
}

/**
  \brief Draws the legs of the selected item
  */
void cwNoteItem::drawShotsOfSelectedItem(QPainter* painter) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::red);

    if(SelectedNoteStation == NULL) { return; }
    if(Note == NULL) { return; }

    bool okay;
    int stationIndex = SelectedNoteStation->property("stationId").toInt(&okay);

    if(!okay) { return; }

    cwTrip* parentTrip = Note->parentTrip();
    if(parentTrip == NULL) { return; }

    cwNoteStation selectedStation = Note->station(stationIndex);
    if(!selectedStation.station().isValid()) { return; }

    QVector3D selectStationPosition = selectedStation.station().position();
    selectStationPosition.setZ(0.0);

    //Transformation matrix for maniplating the points
    QMatrix4x4 matrix = Note->noteTransformationMatrix();

    //For projecting the points on to the page of notes
    QMatrix4x4 projection = Camera->viewProjectionMatrix() * modelMatrix();

    //Get all the neighboring stations for this trip
    QSet<cwStationReference> neighboringStations = parentTrip->neighboringStations(selectedStation.name());
    foreach(cwStationReference station, neighboringStations) {
        QVector3D neighborPosition = station.position();
        neighborPosition.setZ(0.0);

        //Scale and rotate the vector
        QVector3D vectorBetween = neighborPosition - selectStationPosition;
        vectorBetween = matrix * vectorBetween;

        QVector3D centerPoint = projection * QVector3D(selectedStation.positionOnNote());
        centerPoint = Camera->mapNormalizeScreenToGLViewport(centerPoint);

        QVector3D neighborPoint = projection * QVector3D(selectedStation.positionOnNote() + vectorBetween.toPointF());
        neighborPoint = Camera->mapNormalizeScreenToGLViewport(neighborPoint);

        painter->drawLine(centerPoint.toPointF(), neighborPoint.toPointF());
    }
}

/**
  \brief Initializes the pan with the first point in the pan
  */
 void cwNoteItem::panFirstPoint(QPointF firstMousePoint) {
     LastPanPoint = Camera->mapToGLViewport(firstMousePoint.toPoint());
     Camera->unProject(LastPanPoint, 0.1, NoteScaleModelMatrix);
//     event->accept();
 }

 /**
   \brief Called when the user moves the mouse in the pan area

   This will pan the note
   */
 void cwNoteItem::panMove(QPointF mousePosition) {
     QPoint currentPanPoint = Camera->mapToGLViewport(mousePosition.toPoint());

     QVector3D unProjectedCurrent = Camera->unProject(currentPanPoint, 0.1, NoteScaleModelMatrix);
     QVector3D unProjectedLast = Camera->unProject(LastPanPoint, 0.1, NoteScaleModelMatrix);

     QVector3D delta = unProjectedCurrent - unProjectedLast;

     NoteScaleModelMatrix.translate(delta);
     setNoteScaleModelMatrix(NoteScaleModelMatrix);

     LastPanPoint = currentPanPoint;

     update();
 }

void cwNoteItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    //Calc the scaleFactor
    float scaleFactor = 1.1;
    if(event->delta() < 0) {
        //Zoom out
        scaleFactor = 1.0 / scaleFactor;
    }

    QPoint screenCoords = Camera->mapToGLViewport(event->pos().toPoint());
    QVector3D beforeZoom = Camera->unProject(screenCoords, 0.1, NoteScaleModelMatrix);

    QMatrix4x4 zoom;
    zoom.scale(scaleFactor, scaleFactor);
    NoteScaleModelMatrix = NoteScaleModelMatrix * zoom;

    QVector3D afterZoom = Camera->unProject(screenCoords, 0.1, NoteScaleModelMatrix);

    //Adjust the scale matrix
    QVector3D changeInPosition = afterZoom - beforeZoom;
    NoteScaleModelMatrix.translate(changeInPosition);
    setNoteScaleModelMatrix(NoteScaleModelMatrix);

    update();
}

/**
  Sets the note that this note item displayes
  */
void cwNoteItem::setNote(cwNote* note) {
    if(Note != note) {
        if(Note != NULL) {
            disconnect(Note, NULL, this, NULL);
        }

        Note = note;
        setSelectedStation(NULL);

        if(Note != NULL) {
            connect(Note, SIGNAL(destroyed()), SLOT(noteDeleted()));
            connect(Note, SIGNAL(rotateChanged(float)), SLOT(updateNoteRotation(float)));
            connect(Note, SIGNAL(imageChanged(cwImage)), SLOT(setImage(cwImage)));
            connect(Note, SIGNAL(stationAdded()), SLOT(regenerateStationVertices()));
            connect(Note, SIGNAL(stationPositionChanged(int)), SLOT(regenerateStationVertices()));
            connect(Note, SIGNAL(stationRemoved(int)), SLOT(stationRemoved(int)));
            setImage(Note->image());

            regenerateStationVertices();
            //Update all the station with new positions and station names
            foreach(QDeclarativeItem* qmlStation, QMLNoteStations) {
                qmlStation->setProperty("note", QVariant::fromValue<QObject*>(Note));
            }
        }
    }
}

/**
  \brief Sets the image source for the the note item
  */
void cwNoteItem::setImage(cwImage image) {
    //Load the notes in an asyn way
    LoadNoteWatcher->cancel(); //Cancel previous run, if still running
    QFuture<QPair<QByteArray, QSize> > future = QtConcurrent::mapped(image.mipmaps(), LoadImage(ProjectFilename));
    LoadNoteWatcher->setFuture(future);
}

/**
  The image has complete loading it self
  */
void cwNoteItem::ImageFinishedLoading() {
    GLWidget->makeCurrent();

    QList<QPair<QByteArray, QSize> >mipmaps = LoadNoteWatcher->future().results();

    if(mipmaps.empty()) { return; }

    //Load the data into opengl
    glBindTexture(GL_TEXTURE_2D, NoteTexture);

    int trueMipmapLevel = 0;
    for(int mipmapLevel = 0; mipmapLevel < mipmaps.size(); mipmapLevel++) {

        //Get the mipmap data
        QPair<QByteArray, QSize> image = mipmaps.at(mipmapLevel);
        QByteArray imageData = image.first;
        QSize size = image.second;

        //if(size.width() < 2048 && size.height() < 2048) {
            glCompressedTexImage2D(GL_TEXTURE_2D, trueMipmapLevel, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                                   size.width(), size.height(), 0,
                                   imageData.size(), imageData.data());
            trueMipmapLevel++;
        //}
    }

   // glGenerateMipmap(GL_TEXTURE_2D); //Generate the mipmaps for NoteTexture
    glBindTexture(GL_TEXTURE_2D, 0);

    ImageSize = mipmaps.first().second;
    NoteScaleModelMatrix.setToIdentity();
    NoteScaleModelMatrix.scale(ImageSize.width(), ImageSize.height(), 1.0);
    setNoteScaleModelMatrix(NoteScaleModelMatrix);

    updateNoteRotation(Note->rotate());

    resizeGL();

    emit noteChanged(Note);

    update();
}

/**
  \brief Rotates the note item

  \param degrees, the amount of rotaton of the noteItem
  */
void cwNoteItem::updateNoteRotation(float degrees) {
    QMatrix4x4 rotationModelMatrix;
    rotationModelMatrix.translate(.5, .5, 0.0);
    rotationModelMatrix.rotate(-degrees, 0.0, 0.0, 1.0);
    rotationModelMatrix.translate(-.5, -.5, 0.0);

    setRotationMatrix(rotationModelMatrix);

    resizeGL();
    update();
}

/**
  \brief This updates the qml transfromation item, such that all the survey station
  gui elements are place in the correct spot when rendering
  */
void cwNoteItem::updateQMLTransformItem() {
    QList<cwNoteStation> stations = Note->stations();
    if(stations.size() != QMLNoteStations.size()) {
        qDebug() << "Stations size are wrong!!!, this is a bug!" << stations.size() << QMLNoteStations.size();
        return;
    }

    //This is slow, compared to GPU transformation, but not sure how to use QML,  Could use
    //Point sprits, but annoying to control.
    for(int i = 0; i < stations.size() && i < QMLNoteStations.size(); i++) {
        QDeclarativeItem* item = QMLNoteStations[i];
        cwNoteStation station = stations[i];

        QVector3D transformedPoint = Camera->viewProjectionMatrix() * modelMatrix() * QVector3D(station.positionOnNote());
        transformedPoint = Camera->mapNormalizeScreenToGLViewport(transformedPoint);

        QTransform transform;
        transform.translate(transformedPoint.x(), transformedPoint.y());

        item->setTransform(transform);
        item->setPos(-item->width() / 2.0, -item->height() / 2.0);
    }
}

/**
  \brief This sets the rotation matrix

  This will also update the qml transformation item
  */
void cwNoteItem::setRotationMatrix(QMatrix4x4 rotationMatrix) {
    RotationModelMatrix = rotationMatrix;
    updateQMLTransformItem();
}

/**
  \brief This sets the note scale model matrix

  This will also update the qml transformation item
  */
void cwNoteItem::setNoteScaleModelMatrix(QMatrix4x4 noteScaleModelMatrix) {
    NoteScaleModelMatrix = noteScaleModelMatrix;
    updateQMLTransformItem();
}

/**
  \brief Get the OpenGL model matrix for this item
  */
QMatrix4x4 cwNoteItem::modelMatrix() const {
    return NoteScaleModelMatrix * RotationModelMatrix;
}

/**
  \brief Allow QtConturrent to create a QImage

  This returns a opengl formatted image
  */
QPair<QByteArray, QSize> cwNoteItem::LoadImage::operator ()(int imageId) {
    //Extract the image data from the imageProvider
    cwProjectImageProvider imageProvidor;
    imageProvidor.setProjectPath(Filename);

    QSize size;
    QByteArray imageData = imageProvidor.requestImageData(imageId, &size);

    return QPair<QByteArray, QSize>(imageData, size);
}

/**
  \brief Adds a new station to the cwNoteItem

  The new station position will be converted from qtViewportCoordinates into
  a normalized position of the notes.

  \param qtViewportCoordinates - This is the unprojected viewport coordinates, ie
  the qt window coordinates were the top left is the origin.  These should be
  in local coordines of the cwNoteItem

  \return The index to the add station
  */
int cwNoteItem::addStation(QPoint qtViewportCoordinate) {
    QPoint glViewportPoint = Camera->mapToGLViewport(qtViewportCoordinate);
    QVector3D notePosition = Camera->unProject(glViewportPoint, 0.0, modelMatrix());

    cwNoteStation newNoteStation;
    newNoteStation.setPositionOnNote(notePosition.toPointF());

    //Try to guess the station name
    QString stationName;
    if(SelectedNoteStation != NULL) {
        int stationId = SelectedNoteStation->property("stationId").toInt();
        cwNoteStation selectedStation = Note->station(stationId);
        stationName = Note->guessNeighborStationName(selectedStation, notePosition.toPointF());
    }

    if(stationName.isEmpty()) {
        stationName = "Station Name";
    }

    qDebug() << "Found Station name:" << stationName;

    newNoteStation.station().setName(stationName);
    Note->addStation(newNoteStation);

    //Get the last station in the list and select it
    if(!QMLNoteStations.isEmpty()) {
        QMLNoteStations.last()->setProperty("selected", true);
    }


    return Note->numberOfStations() - 1;
}

/**
  \brief Moves the station to qtViewportCoordinate

  This will convert the qtVieweportCoordinate, that's in this coordinate system
  and convert it into a new note position.

  */
void cwNoteItem::moveStation(QPoint qtViewportCoordinate, cwNote* note, int stationIndex) {

    if(note == NULL) { return; }
    if(stationIndex < 0 || stationIndex >= note->numberOfStations()) { return; }

    QPoint glViewportPoint = Camera->mapToGLViewport(qtViewportCoordinate);
    QVector3D notePosition = Camera->unProject(glViewportPoint, 0.0, modelMatrix());

    Note->setStationData(cwNote::StaitonPosition, stationIndex, notePosition.toPointF());
}

/**
  \brief Sets the selected station.

  This will deselect the previous station.  Send this null to deselect the current
  station
  */
void cwNoteItem::setSelectedStation(QDeclarativeItem* station) {
    if(SelectedNoteStation != NULL && SelectedNoteStation->property("selected").toBool() != false) {
        SelectedNoteStation->setProperty("selected", false);
    }

    SelectedNoteStation = station;

    if(SelectedNoteStation != NULL && SelectedNoteStation->property("selected").toBool() != true) {
        SelectedNoteStation->setProperty("selected", true);
    }
}

/**
  This is called when the note removes a station
  */
void cwNoteItem::stationRemoved(int stationIndex) {
    //Unselect the item that's going to be deleted
    if(stationIndex >= 0 || stationIndex < QMLNoteStations.size()) {
        if(SelectedNoteStation == QMLNoteStations[stationIndex]) {
            SelectedNoteStation = NULL;
        }
        QMLNoteStations[stationIndex]->deleteLater();
        QMLNoteStations.removeAt(stationIndex);
        regenerateStationVertices();
    }
}

