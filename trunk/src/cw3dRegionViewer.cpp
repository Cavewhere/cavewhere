//Our includes
#include "cwGLRenderer.h"
#include "cwCamera.h"
#include "cwCamera.h"
#include "cwLine3D.h"
#include "cwPlane.h"
#include "cwRegularTile.h"
#include "cwEdgeTile.h"
#include "cwShaderDebugger.h"
#include "cwGLShader.h"
#include "cwGLTerrain.h"
#include "cwGLLinePlot.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwStation.h"
#include "cw3dRegionViewer.h"

//Qt includes
#include <QPainter>
#include <QString>
#include <QVector3D>
#include <QRect>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMouseEventTransition>
#include <QKeyEventTransition>
#include <QDebug>
#include <QGraphicsSceneWheelEvent>
#include <QtConcurrentMap>
#include <QFontMetrics>

cw3dRegionViewer::cw3dRegionViewer(QDeclarativeItem *parent) :
    cwGLRenderer(parent)
{
    Region = NULL;

    Terrain = new cwGLTerrain(this);
    Terrain->setCamera(Camera);
    Terrain->setShaderDebugger(ShaderDebugger);
    Terrain->setNumberOfLevels(10);
    //connect(Terrain, SIGNAL(redraw()), SLOT(updateGL()));

    LinePlot = new cwGLLinePlot(this);
    LinePlot->setCamera(Camera);
    LinePlot->setShaderDebugger(ShaderDebugger);

}

/**
  \brief This paints the cwGLRender and the linePlot labels
  */
void cw3dRegionViewer::paint(QPainter * painter, const QStyleOptionGraphicsItem * item, QWidget * widget) {
    //Run paint Framebuffer
    cwGLRenderer::paint(painter, item, widget);

    //Render the text labels
    painter->save();
    painter->setPen(QPen());
    painter->setBrush(QBrush());
    painter->setFont(QFont());
    renderStationLabels(painter);
    painter->restore();
}

/**
  \brief This paints the terrain and the line plot
  */
void cw3dRegionViewer::paintFramebuffer() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    //Terrain->draw();
    LinePlot->draw();

   glDisable(GL_DEPTH_TEST);
}

/**
  \brief Initilizes the viwer
  */
void cw3dRegionViewer::initializeGL() {
    //Initilizes the cameras
    resetView();

  //  Terrain->initialize();
    LinePlot->initialize();

    glEnableClientState(GL_VERTEX_ARRAY); // activate vertex coords array
}

/**
  \brief Called when the viewer's size changes

  This updates the projection matrix for the view
  */
void cw3dRegionViewer::resizeGL() {
    QMatrix4x4 projectionMatrix;
    projectionMatrix.perspective(55, width() / (float)height(), 1, 10000);
    Camera->setProjectionMatrix(projectionMatrix);
}
/**
  Initializes the last click for the panning state
  */
void cw3dRegionViewer::startPanning(QPoint position) {
    QPoint mappedPosition = Camera->mapToGLViewport(position);
    LastMouseGlobalPosition = unProject(mappedPosition);
}

/**
  Pans the view allow the a plane
  */
void cw3dRegionViewer::pan(QPoint position) {
    QPoint mappedPos = Camera->mapToGLViewport(position);

    //Get the ray from the front of the screen to the back of the screen
    QVector3D front = Camera->unProject(mappedPos, 0.0);
    QVector3D back = Camera->unProject(mappedPos, 1.0);

    //Create the ray that'll intersect
    cwLine3D ray(front, back);

    //Create the plane
    cwPlane xyPlane(LastMouseGlobalPosition,
                    LastMouseGlobalPosition + QVector3D(1.0, 0.0, 0.0),
                    LastMouseGlobalPosition + QVector3D(0.0, 1.0, 0.0));

    //Find the intsection on the plane
    bool hasIntersection;
    QVector3D intersection = xyPlane.intersectAsRay(ray, &hasIntersection);



    //Ray and plane are parallel, can't do anything with this
    if(!hasIntersection) { return; }

    QVector3D translateAmount = intersection - LastMouseGlobalPosition;

    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(translateAmount);
    Camera->setViewMatrix(viewMatrix);

    //Update the gl window
    update();
}

/**
  Called when the rotation is about to begin
  */
void cw3dRegionViewer::startRotating(QPoint position) {
    position = Camera->mapToGLViewport(position);
    LastMouseGlobalPosition = unProject(position);
    LastMousePosition = position;
}

/**
  Rotates the view
  */
void cw3dRegionViewer::rotate(QPoint position) {
    position = Camera->mapToGLViewport(position);
    QPoint currentMousePosition = position;
    QPointF delta = LastMousePosition - currentMousePosition;
    LastMousePosition = currentMousePosition;
    delta /= 2.0;

    //Calculate the new pitch
    Pitch = qMin(90.0f, qMax(-90.0f, Pitch + (float)delta.y()));

    //Calculate the new azimuth
    Azimuth = fmod(Azimuth - delta.x(), 360);

    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, Pitch);
    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, Azimuth);
    QQuaternion newQuat =  pitchQuat * azimuthQuat;
    QQuaternion rotationDifferance = CurrentRotation.conjugate() * newQuat;
    CurrentRotation = newQuat;

    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(LastMouseGlobalPosition);
    viewMatrix.rotate(rotationDifferance);
    viewMatrix.translate(-LastMouseGlobalPosition);
    Camera->setViewMatrix(viewMatrix);

    update();
}

void cw3dRegionViewer::wheelEvent(QGraphicsSceneWheelEvent *event) {
    zoom(event);
    event->accept();
}

/**
  Zooms the view
  */
void cw3dRegionViewer::zoom(QGraphicsSceneWheelEvent* event) {

    //Make the event position into gl viewport
    QPoint mappedPos = Camera->mapToGLViewport(event->pos().toPoint());

    //Get the ray from the front of the screen to the back of the screen
    QVector3D front = Camera->unProject(mappedPos, 0.0);

    //Find the intsection on the plane
    QVector3D intersection = unProject(mappedPos);

    //Smallray
    QVector3D ray = intersection - front;
    float rayLength = ray.length();
    ray.normalize();

    float t = .0005 * -event->delta();
    t = qMax(-1.0f, qMin(1.0f, t));
    t = rayLength * t;

    QVector3D newPositionDelta = ray * t;

    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(newPositionDelta);
    Camera->setViewMatrix(viewMatrix);

    update();
}

/**
  \brief Resets the view
  */
void cw3dRegionViewer::resetView() {
    //    QMatrix4x4 projectionMatrix;
    //    projectionMatrix.perspective(55, 1.0, .1, 10000); //ortho(-2.0, 2.0, -2.0, 2.0, -2.0, 2.0);
    //    Camera->setProjectionMatrix(projectionMatrix);

    Pitch = 90.0;
    Azimuth = 0.0;

    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, Pitch);
    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, Azimuth);

    CurrentRotation =  pitchQuat * azimuthQuat;

    QMatrix4x4 viewMatrix;
    viewMatrix.translate(QVector3D(0.0, 0.0, -2));
    Camera->setViewMatrix(viewMatrix);

    update();
}

/**
  \brief This renders the station labels for the caving region

  This will go through all the caves and render station labels for them
  */
void cw3dRegionViewer::renderStationLabels(QPainter* painter) {
    //Clear the collision lable kd-tree
    LabelKdTree.clear();

    for(int i = 0; i < Region->caveCount(); i++) {
       cwCave* cave = Region->cave(i);
       renderStationLabels(painter, cave);
    }

   // LabelKdTree.paintTree(painter, QRect(QPoint(0, 0), QSize(width(), height())));
}

/**
  \brief This renders the station labels for a cave
  */
void cw3dRegionViewer::renderStationLabels(QPainter* painter, cwCave* cave) {

    QList <QWeakPointer<cwStation> > stations = cave->stations();

    //Transforms all the station's points
    QList<QVector3D> transformedStationPoints = QtConcurrent::blockingMapped(stations,
                                                                             TransformPoint(Camera->viewProjectionMatrix(),
                                                                                            Camera->viewport()));


    Q_ASSERT(transformedStationPoints.size() == cave->stations().count());

    QFont defaultFont;
    QFontMetrics fontMetrics(defaultFont);

    //Go through all the station points and render the text
    for(int i = 0; i < stations.size(); i++) {
        QVector3D projectedStationPosition = transformedStationPoints[i];

        //Clip the stations to the rendering area
        if(projectedStationPosition.z() > 1.0 ||
                projectedStationPosition.z() < 0.0 ||
                !Camera->viewport().contains(projectedStationPosition.x(), projectedStationPosition.y())) {
            continue;
        }

        if(stations[i].isNull()) {
            continue;
        }

        QString stationName = stations[i].data()->name();

        //See if stationName overlaps with other stations
        QPoint topLeftPoint = projectedStationPosition.toPoint();
        QSize stationNameTextSize = fontMetrics.size(Qt::TextSingleLine, stationName);
        QRect stationRect(topLeftPoint, stationNameTextSize);
        stationRect.moveTop(stationRect.top() - stationNameTextSize.height() / 1.5);
        bool couldAddText = LabelKdTree.addRect(stationRect);

        if(couldAddText) {
            painter->save();
            painter->setPen(Qt::white);
            painter->drawText(topLeftPoint + QPoint(1,1), stationName);

            painter->setPen(Qt::black);
            painter->drawText(topLeftPoint, stationName);
            painter->restore();
        }
    }
}

/**
  \brief This is a helper for QtCurrentent function

  This is the kernel for multi threaded algroithm to transform the points into
  screen coordinates.  This is a helper function to renderStationLabels
  */
QVector3D cw3dRegionViewer::TransformPoint::operator()(QWeakPointer<cwStation> station) {
    QSharedPointer<cwStation> strongStation = station.toStrongRef();

    if(strongStation.isNull()) {
        return QVector3D();
    }

    QVector3D normalizeSceenCoordinate =  ModelViewProjection * strongStation->position();
    QVector3D viewportCoord = cwCamera::mapNormalizeScreenToGLViewport(normalizeSceenCoordinate, Viewport);
    return viewportCoord;
}

/**
  \brief Sets the caving region for the renderer
  */
void cw3dRegionViewer::setCavingRegion(cwCavingRegion* region) {
    if(Region != region) {
        Region = region;
        emit cavingRegionChanged();
    }
}
