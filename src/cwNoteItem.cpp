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
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQmlContext>
#include <QGraphicsScene>
#include <QGLWidget>

//For testing
#include <QGraphicsWidget>



cwNoteItem::cwNoteItem(QQuickItem *parent) :
    cwGLRenderer(parent),
    LoadNoteWatcher(new QFutureWatcher<QPair<QByteArray, QSize> >(this)),
    Note(NULL),
    TransformUpdater(new cwTransformUpdater(this))
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);

    connect(LoadNoteWatcher, SIGNAL(finished()), SLOT(ImageFinishedLoading()));

    TransformUpdater->setCamera(Camera);
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
    cwGLShader* imageVertexShader = new cwGLShader(QOpenGLShader::Vertex);
    imageVertexShader->setSourceFile("shaders/NoteItem.vert");

    cwGLShader* imageFragmentShader = new cwGLShader(QOpenGLShader::Fragment);
    imageFragmentShader->setSourceFile("shaders/NoteItem.frag");

    ImageProgram = new QOpenGLShaderProgram(this);
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
    NoteVertexBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    NoteVertexBuffer.create();    //Create the vertexes buffer to render a quad

    QVector<QVector2D> vertices;
    vertices.reserve(4);
    vertices.append(QVector2D(0.0, 1.0));
    vertices.append(QVector2D(0.0, 0.0));
    vertices.append(QVector2D(1.0, 1.0));
    vertices.append(QVector2D(1.0, 0.0));

    //Allocate the buffer array for this object
    NoteVertexBuffer.bind();
    NoteVertexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    NoteVertexBuffer.allocate(vertices.data(), vertices.size() * sizeof(QVector2D));
    NoteVertexBuffer.release();
}

/**
  \brief This regenerates all the station vertices for the note item
  */
void cwNoteItem::regenerateStationVertices() {

//    //Make sure we have a note component so we can create it
//    if(NoteStationComponent == NULL) {
//        QQmlContext* context = QQmlEngine::contextForObject(this);
//        if(context == NULL) { return; }
//        NoteStationComponent = new QQmlComponent(context->engine(), "qml/NoteStation.qml", this);
//        if(NoteStationComponent->isError()) {
//            qDebug() << "Notecomponent errors:" << NoteStationComponent->errorString();
//        }
//    }

//    if(QMLNoteStations.size() < Note->numberOfStations()) {
//        int notesToAdd = Note->numberOfStations() - QMLNoteStations.size();
//        //Add more stations to the NoteStations
//        for(int i = 0; i < notesToAdd; i++) {
//            int noteStationIndex = QMLNoteStations.size();

//            if(noteStationIndex < 0 || noteStationIndex >= Note->stations().size()) {
//                qDebug() << "noteStationIndex out of bounds:" << noteStationIndex << QMLNoteStations.size();
//                continue;
//            }

//            QQuickItem* stationItem = qobject_cast<QQuickItem*>(NoteStationComponent->create());
//            if(stationItem == NULL) {
//                qDebug() << "Problem creating new station item ... THIS IS A BUG!";
//                continue;
//            }

//            QMLNoteStations.append(stationItem);
//            TransformUpdater->addPointItem(stationItem);
//        }
//    } else if(QMLNoteStations.size() > Note->numberOfStations()) {
//        //Remove stations from NoteStations
//        int notesToRemove = QMLNoteStations.size() - Note->numberOfStations();
//        //Add more stations to the NoteStations
//        for(int i = 0; i < notesToRemove; i++) {
//            QQuickItem* deleteStation = QMLNoteStations.last();
//            QMLNoteStations.removeLast();
//            deleteStation->deleteLater();
//        }
//    }

//    for(int i = 0; i < QMLNoteStations.size(); i++) {
//        QQuickItem* stationItem = QMLNoteStations[i];
//        stationItem->setProperty("stationId", i);
//        stationItem->setProperty("noteViewer", QVariant::fromValue(this));
//        stationItem->setProperty("note", QVariant::fromValue(Note));
//        stationItem->setParentItem(this);
//    }

//    TransformUpdater->update();

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
    Camera->setViewport(QRect(QPoint(0.0, 0.0), windowSize));

//    updateQMLTransformItem();
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
    ImageProgram->setUniformValue(ModelViewProjectionMatrix, Camera->viewProjectionMatrix() * RotationModelMatrix);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, NoteTexture);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); //Draw the quad

    NoteVertexBuffer.release();
    ImageProgram->release();
    glBindTexture(GL_TEXTURE_2D, 0);
}

//void cwNoteItem::paint(QPainter* painter, const QStyleOptionGraphicsItem * styleItem, QWidget * widget) {
////    cwGLRenderer::paint(painter, styleItem, widget);
////    drawShotsOfSelectedItem(painter);
//}

/**
  \brief Draws the legs of the selected item
  */
void cwNoteItem::drawShotsOfSelectedItem(QPainter* /*painter*/) {
//    QPen pen;
//    pen.setColor(Qt::red);
//    pen.setWidthF(1.0);
//    painter->setPen(pen);

//    if(SelectedNoteStation == NULL) { return; }
//    if(Note == NULL) { return; }

//    bool okay;
//    int stationIndex = SelectedNoteStation->property("stationId").toInt(&okay);

//    if(!okay) { return; }

//    cwTrip* parentTrip = Note->parentTrip();
//    if(parentTrip == NULL) { return; }

//    cwNoteStation selectedStation = Note->station(stationIndex);
//    if(!selectedStation.station().isValid()) { return; }

//    QVector3D selectStationPosition = selectedStation.station().position();
//    selectStationPosition.setZ(0.0);

//    //Transformation matrix for maniplating the points
//    QMatrix4x4 matrix = Note->noteTransformationMatrix();

//    //For projecting the points on to the page of notes
//    QMatrix4x4 projection = Camera->viewProjectionMatrix() * RotationModelMatrix;

//    //Get all the neighboring stations for this trip
//    QSet<cwStationReference> neighboringStations = parentTrip->neighboringStations(selectedStation.name());
//    foreach(cwStationReference station, neighboringStations) {
//        QVector3D neighborPosition = station.position();
//        neighborPosition.setZ(0.0);

//        //Scale and rotate the vector
//        QVector3D vectorBetween = neighborPosition - selectStationPosition;
//        vectorBetween = matrix * vectorBetween;

//        QVector3D centerPoint = projection * QVector3D(selectedStation.positionOnNote());
//        centerPoint = Camera->mapNormalizeScreenToGLViewport(centerPoint);

//        QVector3D neighborPoint = projection * QVector3D(selectedStation.positionOnNote() + vectorBetween.toPointF());
//        neighborPoint = Camera->mapNormalizeScreenToGLViewport(neighborPoint);

//        painter->drawLine(centerPoint.toPointF(), neighborPoint.toPointF());
//    }
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
            connect(Note, SIGNAL(stationPositionChanged(int)), SLOT(updateStationPosition(int)));
            connect(Note, SIGNAL(stationRemoved(int)), SLOT(stationRemoved(int)));
            setImage(Note->image());

            regenerateStationVertices();
            //Update all the station with new positions and station names
            foreach(QQuickItem* qmlStation, QMLNoteStations) {
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
//    GLWidget->makeCurrent();

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

    QMatrix4x4 newViewMatrix;
    newViewMatrix.setToIdentity();
    newViewMatrix.scale(ImageSize.width(), ImageSize.height(), 1.0);

    Camera->setViewMatrix(newViewMatrix);

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
//void cwNoteItem::updateQMLTransformItem() {

////    QMatrix4x4 flipY;
////    flipY.scale(width() / 2.0, -height() / 2.0, 1.0);
////    flipY.translate(1.0, -1.0, 0.0);
////    QMatrix4x4 matrix = flipY * Camera->viewProjectionMatrix() * modelViewMatrix();
////   QTransform transform2D = matrix.toTransform();
////   qDebug()  << "Maped Point: " << QMatrix4x4(Camera->viewProjectionMatrix() * modelViewMatrix()).toTransform().map(QPointF(0.0, 0.0)) << transform2D.map(QPointF(0.0, 0.0));
////   TestParent->setTransform(transform2D);

////    QList<cwNoteStation> stations = Note->stations();
////    if(stations.size() != QMLNoteStations.size()) {
////        qDebug() << "Stations size are wrong!!!, this is a bug!" << stations.size() << QMLNoteStations.size();
////        return;
////    }

////    //This is slow, compared to GPU transformation, but not sure how to use QML,  Could use
////    //Point sprits, but annoying to control.
////    for(int i = 0; i < stations.size() && i < QMLNoteStations.size(); i++) {
////        QQuickItem* item = QMLNoteStations[i];
////        cwNoteStation station = stations[i];

////        QVector3D transformedPoint = Camera->viewProjectionMatrix() * RotationModelMatrix * QVector3D(station.positionOnNote());
////        transformedPoint = Camera->mapNormalizeScreenToGLViewport(transformedPoint);

////        QTransform transform;
////        transform.translate(transformedPoint.x(), transformedPoint.y());

////        item->setTransform(transform);
////        item->setPos(-item->width() / 2.0, -item->height() / 2.0);
////    }
//}

/**
  \brief This sets the rotation matrix

  This will also update the qml transformation item
  */
void cwNoteItem::setRotationMatrix(QMatrix4x4 rotationMatrix) {
    RotationModelMatrix = rotationMatrix;
    TransformUpdater->setModelMatrix(RotationModelMatrix);
//    updateQMLTransformItem();
}

///**
//  \brief This sets the note scale model matrix

//  This will also update the qml transformation item
//  */
//void cwNoteItem::setViewMatrix(QMatrix4x4 viewMatrix) {
//    ViewMatrix = noteScaleModelMatrix;
//    updateQMLTransformItem();
//}

///**
//  \brief Get the OpenGL model matrix for this item
//  */
//QMatrix4x4 cwNoteItem::modelViewMatrix() const {
//    return ViewMatrix * RotationModelMatrix;
//}

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
  the qt window coordinates were the top left is the 54origin.  These should be
  in local coordines of the cwNoteItem

  \return The index to the add station
  */
int cwNoteItem::addStation(QPoint /*qtViewportCoordinate*/) {
//    QPoint glViewportPoint = Camera->mapToGLViewport(qtViewportCoordinate);
//    QVector3D notePosition = Camera->unProject(glViewportPoint, 0.0, RotationModelMatrix);

//    cwNoteStation newNoteStation;
//    newNoteStation.setPositionOnNote(notePosition.toPointF());

//    //Try to guess the station name
//    QString stationName;
//    if(SelectedNoteStation != NULL) {
//        int stationId = SelectedNoteStation->property("stationId").toInt();
//        cwNoteStation selectedStation = Note->station(stationId);
//        stationName = Note->guessNeighborStationName(selectedStation, notePosition.toPointF());
//    }

//    if(stationName.isEmpty()) {
//        stationName = "Station Name";
//    }

//    qDebug() << "Found Station name:" << stationName;

//    newNoteStation.station().setName(stationName);
//    Note->addStation(newNoteStation);

//    //Get the last station in the list and select it
//    if(!QMLNoteStations.isEmpty()) {
//        QMLNoteStations.last()->setProperty("selected", true);
//    }


//    return Note->numberOfStations() - 1;
    return 0;
}

/**
  \brief Moves the station to qtViewportCoordinate

  This will convert the qtVieweportCoordinate, that's in this coordinate system
  and convert it into a new note position.

  */
void cwNoteItem::moveStation(QPoint /*qtViewportCoordinate*/, cwNote* /*note*/, int /*stationIndex*/) {

//    if(note == NULL) { return; }
//    if(stationIndex < 0 || stationIndex >= note->numberOfStations()) { return; }

//    QPoint glViewportPoint = Camera->mapToGLViewport(qtViewportCoordinate);
//    QVector3D notePosition = Camera->unProject(glViewportPoint, 0.0, RotationModelMatrix);

//    Note->setStationData(cwNote::StationPosition, stationIndex, notePosition.toPointF());
}

/**
  \brief Sets the selected station.

  This will deselect the previous station.  Send this null to deselect the current
  station
  */
void cwNoteItem::setSelectedStation(QQuickItem* station) {
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

/**
  \brief Updates the station's position
  */
void cwNoteItem::updateStationPosition(int /*stationIndex*/) {
//    if(stationIndex >= 0 || stationIndex < QMLNoteStations.size()) {
//        QPointF point = Note->stationData(cwNote::StationPosition, stationIndex).value<QPointF>();
//        QMLNoteStations[stationIndex]->setProperty("position3D", QVector3D(point));
//    }
}

/**
  This adds a new scrap point to the current scrap
  */
void cwNoteItem::addScrapPoint(QPoint qtViewportCoordinate) {
    Q_UNUSED(qtViewportCoordinate);
}
